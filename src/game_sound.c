#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "render.h"
#include "game_math.h"
#include "game.h"
#include "effect_update.h"
#include "camera.h"
#include "object.h"
#include "game_file.h"
#include "player.h"
#include "random.h"
#include "runtime_heap.h"
#include "cc_archive.h"
#include "collision.h"
#include "game_sound.h"

/* Original: DAT_800E7F68 */
static sint16 recent_speech_samples[4];
/* Original: DAT_800D3D74 */
static uint32 recent_speech_write_idx;
/* Original: DAT_800D3D76 */
sint16 g_ambient_speech_timer;
/* Original: DAT_800D3D9C */
sint32 g_sound_handles_invalidated;

/* Original: DAT_800D422C. */
void *g_sound_archive;
sint32 g_sound_archive_size;

/* Originals: DAT_800D3D78, DAT_800D3D7C, DAT_800D3D80,
 * DAT_800D3D84, DAT_800D3D88. */
sint32 g_sfx_playback_volume = 0x7f;

sint32 g_sfx_volume_setting = 0x7f;

sint32 g_music_volume_setting = 0x7f;

sint32 g_cur_music_track = -1;

sint32 g_cd_audio_track_offset;

/* Original: SLES_004.74:DAT_800CBAF0 */
const uint8 g_music_track_volumes[24] = {0, 0, 127, 127, 127, 127, 127, 127, 127, 127, 127, 40, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};

/* Original data: SLES_004.74:0x800CBA2C */
static const uint16 speech_choices0[6] = {5, 124, 121, 116, 122, 123};

/* Original data: SLES_004.74:0x800CBA38 */
static const uint16 speech_choices1[5] = {4, 126, 125, 112, 118};

/* Original data: SLES_004.74:0x800CBA44 */
static const uint16 speech_choices2[4] = {3, 114, 126, 113};

/* Original data: SLES_004.74:0x800CBA4C */
static const uint16 speech_choices3[2] = {1, 117};

/* Original data: SLES_004.74:0x800CBA50 */
static const uint16 speech_choices4[5] = {4, 120, 128, 129, 119};

/* Original data: SLES_004.74:0x800CBA5C */
static const uint16 speech_choices5[7] = {6, 123, 115, 149, 146, 145, 147};

/* Original data: SLES_004.74:0x800CBA6C */
static const uint16 speech_choices6[4] = {3, 135, 134, 136};

/* Original data: SLES_004.74:0x800CBA74 */
static const uint16 speech_choices7[2] = {1, 127};

/* Original data: SLES_004.74:0x800CBA78 */
static const uint16 speech_choices8[3] = {2, 132, 133};

/* Original data: SLES_004.74:0x800CBA80 */
static const uint16 speech_choices9[5] = {4, 150, 142, 143, 144};

/* Original data: SLES_004.74:0x800CBA8C */
static const uint16 speech_choices10[4] = {3, 139, 140, 141};

/* Original data: SLES_004.74:0x800CBA94 */
static const uint16 speech_choices11[3] = {2, 137, 138};

/* Original data: SLES_004.74:0x800CBA9C */
static const uint16 speech_choices12[4] = {3, 151, 140, 139};

/* Original data: SLES_004.74:0x800CBAA4 */
static const uint16 speech_choices13[5] = {4, 148, 147, 149, 145};

/* Original data: SLES_004.74:0x800CBAB0 */
static const uint16 speech_choices14[2] = {1, 131};

/* Original data: SLES_004.74:0x800CBAB4 */
static const uint16 *const speech_choices[15] = {speech_choices0, speech_choices1, speech_choices2, speech_choices3, speech_choices4, speech_choices5, speech_choices6, speech_choices7, speech_choices8, speech_choices9, speech_choices10, speech_choices11, speech_choices12, speech_choices13, speech_choices14};

/* Types. */
typedef struct SOUND_MAP_ENTRY
{
    uint8 sound_code;
    uint8 bank;
    uint8 program;
} SOUND_MAP_ENTRY;

typedef struct
{
    COLLISION col;       /* Legacy +0x00 */
    sint16 speech_id;    /* Legacy +0x7C */
    sint16 delay_ticks;  /* Legacy +0x7E */
    sint16 voice_handle; /* Legacy +0x80 */
} SPEECH_PLAYBACK;

/* Variables. */
static const uint8 original_sound_codes[294] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 65, 40, 41, 52, 53, 70, 71, 72, 73, 32, 33, 34, 35, 36, 65, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 52, 53, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 54, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146,
    147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 42, 43, 44, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 0,  1,  2,   3,   4,   5,   6,   7,   8,  9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  40,  41,  42,  43,  44,  37,  38,  39,  48,  49,  50,  51,  52,  53,  54,  0,   0,
};

static const char *const bank_names[28] = {"AMAIN1", "ABIKE1", "DOCKDAM1", "ENEMY1", "TRCK", "ROB", "AGNTSND1", "GYRO", "FLAME1", "TNK", "AHQ", "JETPACK1", "BIGMAN1", "GUNEMP1", "BFAL", "ENWS", "UWAT", "MAIN", "DOCK", "AIRF", "JNGL", "INDU", "CAVE", "SNWS", "FNWS", "MRF3", "MRF5", "MRF13"};

static SOUND_MAP_ENTRY sound_map[294];

static SOUND_VOLUME_PAIR spatial_volume;

static SpuVoiceAttr voice_attr;

/* Functions. */

/* Original: FUN_800A99F0. */
GDB_CALL void music_track_select(sint32 id)
{
    enum { MUSIC_TRACK_COUNT = sizeof(g_music_track_volumes) / sizeof(g_music_track_volumes[0]) };
    sint32 product, volume, track;
    SpuCommonAttr routing = {0};
    if (id >= MUSIC_TRACK_COUNT)
    {
        fatal_error_with_val("MUSIC TRACK", id);
        abort();
    }
    g_cur_music_track = id;
    product = mul_lo(g_music_track_volumes[id], g_music_volume_setting);
    if (product < 0)
        product = (product + 0x7fu);
    volume = (sint32)((uint32)product << 9) >> 16;
    routing.mask = SPU_COMMON_CDMIX | SPU_COMMON_CDVOLL | SPU_COMMON_CDVOLR;
    routing.cd.mix = SPU_ON;
    routing.cd.volume.left = routing.cd.volume.right = 0x3fff;
    SpuSetCommonAttr(&routing);
    SsSetSerialVol(SS_SERIAL_A, volume, volume);
    track = ((uint32)id + g_cd_audio_track_offset);
    CdPlay(1, &track, 0);
}

/* Original: FUN_800AA1FC. */
void sound_map_reset(void)
{
    sint32 idx;
    for (idx = 0; idx < 294; ++idx)
    {
        sound_map[idx].sound_code = original_sound_codes[idx];
        sound_map[idx].bank = 0xff;
    }
}

/* Original: FUN_800AA000. */
void sound_bank_load(sint32 bank_idx, sint32 first_sound)
{
    char header_name[32];
    char body_path[64];
    RESOURCE_DATA header_member;
    const uint8 *header;
    uint8 *body;
    sint32 body_size;
    sint16 bank;
    uint16 program_count;
    sint32 program;
    if (bank_idx < 0 || bank_idx >= 28)
        return;
    strcpy(header_name, bank_names[bank_idx]);
    strcat(header_name, ".VH");
    header_member = archive_member_find(header_name, g_sound_archive, g_sound_archive_size);
    header = header_member.data;
    if (!header || header_member.size < 32)
    {
        fatal_error("VAB ERROR");
        return;
    }
    program_count = (header[18] | (header[19] << 8));
    if (program_count > 128 || header_member.size < 32u + 128u * 16u + program_count * 16u * 32u + 256u * 2u)
    {
        fatal_error("VAB HEADER SIZE");
        return;
    }
    bank = SsVabOpenHead((uint8 *)header, -1);
    if (bank == -1)
    {
        fatal_error("VAB ERROR");
        return;
    }
    strcpy(body_path, "SOUND\\");
    strcat(body_path, bank_names[bank_idx]);
    strcat(body_path, ".VB");
    body_size = game_file_size(body_path);
    body = (uint8 *)malloc(body_size);
    if (!body)
    {
        fatal_error("VAB MEMORY");
        return;
    }
    game_file_read(body_path, body, body_size);
    if (SsVabTransBody(body, bank) == -1 || !SsVabTransCompleted(SS_WAIT_COMPLETED))
        fatal_error("VAB TRANSFER");
    free(body);
    runtime_heap_sweep();
    for (program = 0; program < program_count && first_sound + program < 294; ++program)
    {
        sound_map[first_sound + program].bank = bank;
        sound_map[first_sound + program].program = program;
    }
}

static void resolve_duplicate_sounds(void)
{
    sint32 dst, src;
    for (dst = 0; dst < 294; ++dst)
    {
        if (sound_map[dst].bank != 0xff)
            continue;
        for (src = 0; src < 294; ++src)
        {
            if (sound_map[src].sound_code == sound_map[dst].sound_code && sound_map[src].bank != 0xff)
            {
                sound_map[dst].bank = sound_map[src].bank;
                sound_map[dst].program = sound_map[src].program;
                break;
            }
        }
    }
}

/* Original: FUN_800AA224. */
void stage_sound_banks_load(void)
{
    sint32 level;
    const char *path = g_session.attract_mode ? "SOUND\\PVH.CC" : "SOUND\\VH.CC";
    sound_map_reset();
    g_sound_archive = game_file_load(path, &g_sound_archive_size);
#if defined(_DEBUG)
    sint32 archive_valid = archive_validate(g_sound_archive, g_sound_archive_size);
    XASSERT(archive_valid);
#endif
    if (g_session.attract_mode)
    {
        if (g_session.language == 1)
            sound_bank_load(15, 111);
        if (g_session.language == 4)
            sound_bank_load(23, 111);
        if (g_session.language == 2)
            sound_bank_load(24, 111);
        recent_speech_write_idx = g_ambient_speech_timer = 0;
        memset(recent_speech_samples, 0, sizeof(sint16) * 4);
    }
    else
    {
        level = g_session.stage;
        if (g_session.scuba_active)
        {
            sound_bank_load(16, 152);
        }
        else
        {
            if (level != 3 && level != 5 && (level < 12 || level >= 16) && level != 17)
                sound_bank_load(17, 0);
            if (level == 3)
                sound_bank_load(25, 170);
            if (level == 5)
                sound_bank_load(26, 205);
            if (level < 2 || level == 2 || (level >= 7 && level < 9))
                sound_bank_load(18, 32);
            if ((level >= 12 && level < 14) || (level >= 14 && level < 16) || level == 17)
                sound_bank_load(27, 245);
            if ((level >= 9 && level < 11) || (level >= 22 && level < 24) || (level >= 24 && level < 26))
                sound_bank_load(20, 55);
            if ((level >= 27 && level < 29) || level == 29 || (level >= 31 && level < 33))
                sound_bank_load(21, 66);
            if (level == 11)
                sound_bank_load(10, 104);
            if (level == 1)
                sound_bank_load(4, 80);
            if (level == 4)
                sound_bank_load(5, 84);
            if (level == 8)
                sound_bank_load(7, 96);
            if ((level >= 12 && level < 14) || level == 2 || level == 15)
                sound_bank_load(14, 106);
            if (effect_find_next(54, 0))
                sound_bank_load(9, 100);
        }
    }
    /* Shared SsVabOpenHead copies the header, programs, tones and sample table */
    free(g_sound_archive);
    g_sound_archive = 0;
    g_sound_archive_size = 0;
    resolve_duplicate_sounds();
    SsSetTickMode(SS_TICK60);
    SsStart();
    runtime_heap_sweep();
    SsSetMVol(127, 127);
}

/* Original: FUN_800AA64C. */
sint16 sound_play_nonpositional(sint32 id, sint32 note_delta, sint32 volume)
{
    sint32 scaled;
    if (g_session.attract_mode || id < 0 || id >= 294 || sound_map[id].bank == 0xff)
        return -1;
    scaled = volume * g_sfx_playback_volume;
    if (scaled < 0)
        scaled += 127;
    scaled >>= 7;
    return SsUtKeyOn(sound_map[id].bank, sound_map[id].program, 0, (note_delta + 60), 0, scaled, scaled);
}

/* Original: FUN_800AA6F0. */
const SOUND_VOLUME_PAIR *sound_spatial_volume_calculate(sint32 type, sint32 x, sint32 y, sint32 z)
{
    sint32 dz = z - g_camera.world_z, dx = x - g_camera.world_x, dy = (y - g_camera.world_y) / 2;
    sint32 distance = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx) / 2 + (dy < 0 ? -dy : dy);
    sint32 volume = type - distance / 2560, scr, span, left, right;
    sint16 ratio;
    if ((sint16)volume <= 0 || dz == 0)
        return 0;
    if ((sint16)volume >= 128)
        volume = 127;
    span = 40960 / volume;
    scr = 320 * dx / dz;
    if (span <= 0)
        return 0;
    ratio = ((scr < 0 ? -scr : scr) << 8) / span;
    left = volume;
    right = volume;
    /* 800AA868/800AA878 are subu channel,base,pan_delta.  The old port
     * followed a bad decompiler expression (-1-pan_delta), which forced the
     * attenuated side below zero and the following clamp muted it entirely. */
    if (scr <= 0)
        left = volume - ratio;
    else
        right = volume - ratio;
    left = left * g_sfx_playback_volume / 128;
    right = right * g_sfx_playback_volume / 128;
    if (left < 0)
        left = 0;
    if (right < 0)
        right = 0;
    if (!(left | right))
        return 0;
    spatial_volume.volume_left = (right << 7);
    spatial_volume.volume_right = (left << 7);
    return &spatial_volume;
}

/* Original: FUN_800AA8FC. */
uint16 sound_play_positional(sint32 id, sint32 note_delta, sint32 type, sint32 x, sint32 y, sint32 z)
{
    const SOUND_VOLUME_PAIR *volume;
    sint32 left, right;
    if (g_session.attract_mode || id < 0 || id >= 294 || sound_map[id].bank == 0xff)
        return 0xffff;
    volume = sound_spatial_volume_calculate(type, x, y, z);
    if (!volume)
        return 0xffff;
    left = volume->volume_left;
    if (left < 0)
        left += 127;
    left >>= 7;
    right = volume->volume_right;
    if (right < 0)
        right += 127;
    right >>= 7;
    return SsUtKeyOn(sound_map[id].bank, sound_map[id].program, 0, (note_delta + 60), 0, left, right);
}

/* Original: FUN_800AAA7C. */
SpuVoiceAttr *sound_voice_spatial_volume_update(const SOUND_VOLUME_PAIR *volume, sint32 handle)
{
    sint32 left, right;
    if (handle < 0 || handle >= 24 || !volume)
        return 0;
    left = volume->volume_left * g_sfx_playback_volume;
    if (left < 0)
        left += 127;
    left >>= 7;
    right = volume->volume_right * g_sfx_volume_setting;
    if (right < 0)
        right += 127;
    right >>= 7;
    memset(&voice_attr, 0, sizeof(voice_attr));
    voice_attr.voice = SPU_KEYCH(handle);
    voice_attr.mask = SPU_VOICE_VOLL | SPU_VOICE_VOLR;
    voice_attr.volume.left = left;
    voice_attr.volume.right = right;
    sound_voice_attributes_apply(&voice_attr);
    SpuGetVoiceAttr(&voice_attr);
    return &voice_attr;
}

/* Exact 0x800AAE78..0x800AAEC4.  The loop counter lives in s0 while a
 * separate halfword at sp+0x10 is passed to FUN_800AA9DC each iteration. */
/* Original: FUN_800AAE78. */
void sound_all_voices_stop(void)
{
    sint16 voice;
    sint16 handle;
    for (voice = 0; voice < 0x18; voice++)
    {
        handle = voice;
        sound_voice_stop(&handle);
    }
    g_sound_handles_invalidated = 1;
}

/* Original: FUN_800AA9DC. */
void sound_voice_stop(sint16 *handle)
{
    sint32 id = g_session.scuba_active ? 163 : 11;
    if (!handle || *handle == -1 || sound_map[id].bank == 0xff)
        return;
    if (SsUtKeyOnV(*handle, sound_map[id].bank, sound_map[id].program, 0, 127, 0, 0, 0) != -1)
        *handle = -1;
}

/* Original: FUN_800AAB40. */
void sound_voice_attributes_apply(SpuVoiceAttr *attr)
{
    if (!attr || attr->voice == 0)
        return;
    SpuSetVoiceAttr(attr);
}

/* Original: FUN_800AAB60. */
static sint32 sound_voice_status_get(sint8 voice)
{
    SpuVoiceAttr attributes;
    if (voice < 0 || voice >= 24)
        return -1;
    if (!SpuGetKeyStatus(SPU_KEYCH(voice)))
        return 0;
    memset(&attributes, 0, sizeof(attributes));
    attributes.voice = SPU_KEYCH(voice);
    SpuGetVoiceAttr(&attributes);
    return attributes.envx ? 1 : 3;
}

/* Original: FUN_800AADAC. */
static void speech_playback_update(void *raw_object)
{
    SPEECH_PLAYBACK *object = (SPEECH_PLAYBACK *)raw_object;
    sint16 handle;
    sint16 note;
    sint16 timer = object->delay_ticks + 1;
    object->delay_ticks = timer;
    if (timer == 30)
    {
        note = (g_session.language == 4 || g_session.language == 2) ? 0 : 3;
        g_session.attract_mode = 0;
        handle = sound_play_nonpositional(object->speech_id, note, 127);
        object->voice_handle = handle;
        g_session.attract_mode = 1;
    }
    else
    {
        handle = object->voice_handle;
        if (handle != -1 && sound_voice_status_get(handle) == 0)
            object_destroy(object);
    }
}

/* Exact game-side speech selector at 0x800AABF8..0x800AADA8. */
/* Original: FUN_800AABF8. */
void speech_random_request(sint32 kind)
{
    sint16 candidates[16];
    const uint16 *list;
    SPEECH_PLAYBACK *object;
    sint16 candidate_count = 0;
    sint16 sel = -1;
    sint16 count, idx, recent;
    sint32 random;
    if (!g_session.attract_mode || kind < 0 || kind >= 15)
        return;
    if (object_find_next_by_type(0, 0x61) != 0)
        return;
    list = speech_choices[kind];
    if (!list)
        return;
    count = list[0];
    for (idx = 0; idx < count; ++idx)
    {
        sint16 sound = list[idx + 1];
        for (recent = 0; recent < 4; ++recent)
            if (sound == recent_speech_samples[recent])
                break;
        if (recent == 4 && candidate_count < 16)
            candidates[candidate_count++] = sound;
    }
    if (candidate_count != 0)
    {
        random = secondary_random_range(candidate_count) - 1;
        if (random < 0)
            random = 0;
        sel = candidates[random];
        object = (SPEECH_PLAYBACK *)object_create(sizeof(*object), speech_playback_update);
        object->col.object_type = 0x61;
        object->speech_id = sel;
        object->delay_ticks = 0;
        object->voice_handle = -1;
        g_ambient_speech_timer = 0;
    }
    recent_speech_samples[recent_speech_write_idx & 3] = sel;
    recent_speech_write_idx = (recent_speech_write_idx + 1) & 3;
}
