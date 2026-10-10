#include "video.h"
#include "psx.h"
#include "psx_gpu.h"
#include "psx_press.h"
#include "psx_spu.h"
#include "psx_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIDEO_RING_SECTORS 256u
#define VIDEO_MAX_WIDTH 320u
#define VIDEO_MAX_HEIGHT 240u
#define VIDEO_TIMEOUT_FIELDS 300u

const uint32 xport_gpu_graph_type_address = 0x800CEC20u;
const uint32 xport_cd_ready_callback_address = 0x800D2DFCu;
const uint32 xport_cd_sync_callback_address = 0x800D2E00u;
const uint32 xport_cd_status_address = 0x800D2E0Cu;
const uint32 xport_cd_setloc_table_address = 0x800D2D74u;

static sint32 video_mount_str(const char *path)
{
    char data_path[260], cue_path[] = ".xport-movie.cue";
    const char *source = path;
    char *destination = data_path;
    FILE *cue;
    size_t remaining = sizeof(data_path);
    sint32 mounted;
    if (!path || !*path)
        return 0;
    if (strncmp(path, "DATA\\", 5) && strncmp(path, "DATA/", 5))
    {
        memcpy(destination, "DATA/", 5);
        destination += 5;
        remaining -= 5;
    }
    while (*source && *source != ';' && remaining > 1)
    {
        *destination++ = *source == '\\' ? '/' : *source;
        ++source;
        --remaining;
    }
    *destination = 0;
    if (*source && *source != ';')
        return 0;
    cue = xport_fopen(cue_path, "w");
    if (!cue)
        return 0;
    fprintf(cue, "FILE \"%s\" BINARY\n TRACK 01 MODE2/2352\n  INDEX 01 00:00:00\n", data_path);
    if (fclose(cue) != 0)
        return 0;
    mounted = cd_mount_cue(cue_path);
    remove(cue_path);
    return mounted;
}

sint32 video_play_str(const char *path, sint32 expected_height, sint32 last_frame)
{
    uint32 *ring = NULL, *vlc = NULL, *strip = NULL, *frame, *raw_header;
    uint32 width, height, x, buffer = 0u, idle = 0u, decoded = 0u;
    uint8 read_mode = 0xC0u;
    sint32 result = 0;
    sint32 stream_active = 0, decoder_active = 0;
    CdlLOC location;
    DISPENV display;
    PSX_RECT rectangle;
    DecDCTCallback previous_in = NULL, previous_out = NULL;
    if (!video_mount_str(path))
    {
        fprintf(stderr, "STR video: cannot mount %s\n", path ? path : "(null)");
        return 0;
    }
    if (!CdInit())
    {
        fprintf(stderr, "STR video: CD initialization failed for %s\n", path);
        cd_unmount_image();
        return 0;
    }
    ring = (uint32 *)malloc(VIDEO_RING_SECTORS * 2048u);
    vlc = (uint32 *)malloc(VIDEO_MAX_WIDTH * VIDEO_MAX_HEIGHT * 2u);
    strip = (uint32 *)malloc(12u * VIDEO_MAX_HEIGHT * sizeof(uint32));
    if (!ring || !vlc || !strip)
    {
        fprintf(stderr, "STR video: allocation failed\n");
        goto cleanup;
    }
    previous_in = DecDCTinCallback(NULL);
    previous_out = DecDCToutCallback(NULL);
    decoder_active = 1;
    DecDCTReset(0);
    DecDCTvlcSize(0);
    StSetRing(ring, VIDEO_RING_SECTORS);
    StSetStream(1u, 0u, last_frame > 0 ? (uint32)last_frame + 1u : 0xFFFFFFFFu, NULL, NULL);
    stream_set_read_mode(read_mode);
    stream_active = 1;
    CdIntToPos(0, &location);
    if (!CdControl(2u, (uint8 *)&location, NULL) || !CdRead2(read_mode))
    {
        fprintf(stderr, "STR video: cannot start %s\n", path);
        goto cleanup;
    }
    fprintf(stderr, "STR video: begin %s\n", path);
    while (!xport_isquit())
    {
        uint16 buttons;
        stream_pump();
        if (StGetNext(&frame, &raw_header))
        {
            VSync(0);
            if (++idle >= VIDEO_TIMEOUT_FIELDS)
            {
                fprintf(stderr, "STR video: timeout after frame %u\n", decoded);
                goto cleanup;
            }
            continue;
        }
        idle = 0u;
        width = ((StHEADER *)raw_header)->width;
        height = ((StHEADER *)raw_header)->height;
        if (!width || width > VIDEO_MAX_WIDTH || !height || height > VIDEO_MAX_HEIGHT || (width & 15u) || (height & 15u))
        {
            fprintf(stderr, "STR video: unsupported frame %ux%u\n", width, height);
            StFreeRing(frame);
            goto cleanup;
        }
        if (expected_height > 0 && height != (uint32)expected_height)
            fprintf(stderr, "STR video: expected height %d, got %u\n", expected_height, height);
        if (DecDCTvlc(frame, vlc) != 0)
        {
            fprintf(stderr, "STR video: malformed VLC frame %u\n", ((StHEADER *)raw_header)->frameCount);
            StFreeRing(frame);
            goto cleanup;
        }
        decoded = ((StHEADER *)raw_header)->frameCount;
        StFreeRing(frame);
        DecDCTin(vlc, 3);
        for (x = 0u; x < width; x += 16u)
        {
            DecDCTout(strip, (sint32)(12u * height));
            DecDCToutSync(0);
            rectangle.x = (sint16)((VIDEO_MAX_WIDTH - width) * 3u / 4u + x * 3u / 2u);
            rectangle.y = (sint16)(buffer * VIDEO_MAX_HEIGHT);
            rectangle.w = 24;
            rectangle.h = (sint16)height;
            LoadImagePSX(&rectangle, strip);
        }
        DrawSync(0);
        SetDefDispEnv(&display, 0, (sint32)(buffer * VIDEO_MAX_HEIGHT), (sint32)VIDEO_MAX_WIDTH, (sint32)height);
        display.isrgb24 = 1;
        PutDispEnv(&display);
        SetDispMask(1);
        if (!gpu_present())
            goto cleanup;
        buttons = (uint16)PadRead(0);
        if (decoded > 2u && (buttons & (PADstart | PADRdown)))
            break;
        if (last_frame > 0 && decoded >= (uint32)last_frame)
            break;
        buffer ^= 1u;
    }
    result = 1;
cleanup:
    CdControl(CdlPause, NULL, NULL);
    if (stream_active)
        StUnSetRing();
    if (decoder_active)
    {
        DecDCTinSync(0);
        DecDCToutSync(0);
        DecDCTinCallback(previous_in);
        DecDCToutCallback(previous_out);
    }
    cd_unmount_image();
    free(strip);
    free(vlc);
    free(ring);
    SetDefDispEnv(&display, 0, 0, 320, 256);
    PutDispEnv(&display);
    fprintf(stderr, "STR video: end %s at frame %u\n", path ? path : "(null)", decoded);
    return result;
}
