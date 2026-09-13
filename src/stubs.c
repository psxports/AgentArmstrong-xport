#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "audio/psyq_sound.h"
#include "game_loop.h"
#include "global.h"
#include "memory_card_platform.h"
#include "object.h"
#include "platform/win/game_platform.h"
#include "platform/win/input.h"
#include "player.h"
#include "render.h"
#include "text_renderer.h"

/* Macros. */
/* Linkable placeholders for functions outside the recovered frame frontier.
 * Every stub is intentionally shallow and should be deleted when its real C
 * implementation lands. */

#define itoa crt_itoa

#undef itoa

/* Functions. */
__declspec(dllexport) volatile uint32 g_psx_draw_sync_count;

__declspec(dllexport) volatile sint32 g_psx_draw_sync_last_mode;

__declspec(dllexport) volatile uint32 g_psx_ss_set_tick_mode_calls, g_psx_ss_start_calls, g_psx_ss_set_mvol_calls;

__declspec(dllexport) volatile uint32 g_psx_spu_shutdown_core_calls;

GDB_CALL sint32 game_psx_draw_sync(void *user, sint32 mode)
{
    (void)user;
    g_psx_draw_sync_last_mode = mode;
    ++g_psx_draw_sync_count;
    return 0;
}

void game_psx_sound_initialize(void *user)
{
    (void)user;
    psyq_sound_init();
}

void game_psx_sound_shutdown(void *user)
{
    (void)user;
    psyq_sound_end();
}

GDB_CALL void game_psx_sound_set_tick_mode(void *user, sint32 mode)
{
    (void)user;
    psyq_sound_set_tick_mode(mode);
    ++g_psx_ss_set_tick_mode_calls;
}

GDB_CALL void game_psx_sound_start(void *user)
{
    (void)user;
    psyq_sound_start();
    ++g_psx_ss_start_calls;
}

GDB_CALL void game_psx_sound_set_master_volume(void *user, sint16 left, sint16 right)
{
    (void)user;
    psyq_sound_set_master_volume(left, right);
    ++g_psx_ss_set_mvol_calls;
}

void game_psx_sound_set_serial_attributes(void *user, sint8 serial, sint8 attribute, sint8 value)
{
    (void)user;
    (void)serial;
    (void)attribute;
    (void)value;
}

void game_psx_sound_set_serial_volume(void *user, sint8 serial, sint16 left, sint16 right)
{
    (void)user;
    (void)serial;
    psyq_sound_music_set_volume(((sint32)left + right) / 2);
}

/* Original: FUN_8008BE50. */
GDB_CALL const char *loading_image_next_path(void)
{
    ++g_loading_image_index;
    if (g_loading_image_index >= 3)
        g_loading_image_index = 0;
    return player_assets_executable_pointer(0x800c8254u + 4 * (sint32)g_loading_image_index);
}

/* PsyQ helper `itoa` at 0x800B0EC4 uses one resident buffer and "%d". */
GDB_CALL char *integer_to_text(sint32 value)
{
    static char text[32];
    sprintf(text, "%d", value);
    return text;
}

/* Exact PAL 0x80088DC0..0x80088EF8 fatal-error display/input loop. */
/* Original: FUN_80088DC0. */
GDB_CALL void fatal_error(const char *message)
{
    uint32 buttons;

    gpu_draw_sync(0);
    while (controllers_read(0) != 0)
        vertical_sync_wait(0);

    do
    {
        while (controllers_read(0) == 0)
        {
            RENDER_FRAME *frame;
            g_render_frame_index ^= 1;
            frame = g_render_frame_index ? &g_render_frame_buffer_0 : &g_render_frame_buffer_1;
            g_current_render_frame = frame;
            g_gpu_packet_cursor = frame->prim_area;
            ClearOTagR(frame->ot, RENDER_OT_LENGTH);
            g_text_color_index = 1;
            text_render(message, -0x8c, 0);
            gpu_draw_sync(0);
            vertical_sync_wait(0);
            drawenv_apply(&frame->drawenv);
            display_env_apply(&frame->display_env);
            ot_submit(frame->ot);
        }
        buttons = controllers_read(0);
        if (buttons != 0)
            vertical_sync_wait(0);
    } while (buttons != 0);
}

/* Original: FUN_800A99F0. */
GDB_CALL void music_track_select(sint32 id, sint32 unused)
{
    const uint8 *volume_table = (const uint8 *)player_assets_executable_address(0x800cbaf0u);
    sint32 volume;
    (void)unused;
    g_current_music_track = id;
    volume = ((sint32)volume_table[id] * g_music_volume_setting) / 0x80;
    psx_select_music_track(id + g_cd_audio_track_offset, volume);
}

/* 0x800B2058..0x800B2064. */
/* Original: FUN_800B2058. */
GDB_CALL sint32 video_mode_set(sint32 value)
{
    sint32 previous = g_video_mode;
    g_video_mode = value;
    return previous;
}

static void psx_spu_shutdown_core(void)
{
    psyq_sound_quit();
    ++g_psx_spu_shutdown_core_calls;
}

/* Original: FUN_800BB5F8. */
GDB_CALL void spu_shutdown(void)
{
    psx_spu_shutdown_core();
}

/* Native formatting adapter; no independent original MIPS entry. */
void fatal_error_with_value(const char *prefix, sint32 value)
{
    char message[40];
    sprintf(message, "%s%d", prefix, value);
    fatal_error(message);
}
