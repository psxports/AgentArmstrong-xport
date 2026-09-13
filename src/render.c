#include <stdio.h>
#include <string.h>
#include "app.h"
#include "audio/game_sound.h"
#include "audio/psyq_sound.h"
#include "global.h"
#include "memory_card_platform.h"
#include "mission.h"
#include "object.h"
#include "original_file.h"
#include "platform/win/game_platform.h"
#include "platform/win/input.h"
#include "player.h"
#include "psx.h"
#include "render.h"
#include "stubs.h"
#include "text_renderer.h"

/* Variables. */
/* Semantic native-port module; original address comments are preserved. */
/* Signed bytes at 0x800C879C.  FUN_800901C8 indexes this table with the
 * signed shake timer divided by two, exactly as the original MIPS does. */
static const sint8 camera_shake_curve[23] = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7, 8, -8, 9, -9, 10, -10, 11, -11};

/* Functions. */
/* These symbols need to become members of a native RenderFrame structure.
 * Their sizes are not declared until the neighboring memory is mapped. */
/* primitive allocation cursor base */
/* ordering table / frame buffer 0 */
/* ordering table / frame buffer 1 */
/* current primitive allocation cursor */

/* Original: FUN_80088978. */
GDB_CALL void frame_begin(void)
{
    uint32 buttons;
    RENDER_FRAME *frame;
    psx_begin_frame();

    g_prim_link_workspace[0] = 0;
    g_prim_link_cursor = g_prim_link_workspace;
    g_vsync_count_this_frame = 0;
    g_stage_background_disabled = 0;

    g_render_frame_index ^= 1;
    g_current_render_frame = g_render_frame_index ? &g_render_frame_buffer_0 : &g_render_frame_buffer_1;
    frame = g_current_render_frame;
    g_gpu_packet_cursor = frame->prim_area;

    ClearOTagR(frame->ot, RENDER_OT_LENGTH);
    ++g_frame_counter;

    buttons = controllers_read(0);
    g_held_buttons = buttons;
    g_pressed_buttons = buttons & ~g_previous_held_buttons;
}

/* Exact predicate at 0x800AEED4..0x800AEF5C. */
/* Original: FUN_800AEED4. */
sint16 controller_packet_is_digital_pad(const uint8 *packet)
{
    if (packet[0] == 0xff)
        return 0;
    return (sint16)((packet[1] >> 4) == 4);
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
    /* gp+0x114 = 0x800D3D9C. */
    {
        g_sound_handles_invalidated = 1;
    }
}

/* PsyQ/CD platform boundaries used by the literal functions below. */
/* Original: FUN_800B30F4. */
GDB_CALL void gpu_draw_sync(sint32 mode)
{
    (void)mode;
}

/* Original: FUN_800B3768. */
GDB_CALL void drawenv_apply(void *env)
{
    (void)env;
}

/* Native GPU submission boundary. Original 800FDDD4 submits before the
 * input wait at 800FDDE4; that path never calls the frame-loop presenter. */
/* Original: FUN_800B36F4. */
GDB_CALL void ot_submit(uint32 *ot)
{
    DrawOTag(ot);
    psx_end_frame();
}

/* Original: FUN_800B8750. */
static void cd_sync(sint32 command, void *state)
{
    (void)command;
    (void)state;
}

/* Original: FUN_800B8A24. */
static void cd_control(sint32 command, sint32 left, sint32 right)
{
    (void)command;
    (void)left;
    (void)right;
}

/* Exact call sequences at 0x800A9F60 and 0x800A9FB0. */
/* Original: FUN_800A9F60. */
void cd_audio_pause(void)
{
    cd_sync(0, &g_cd_sync_state);
    cd_control(9, 0, 0);
    cd_sync(0, &g_cd_sync_state);
    psyq_sound_music_pause();
}

/* Original: FUN_800A9FB0. */
void cd_audio_play(void)
{
    cd_sync(0, &g_cd_sync_state);
    cd_control(3, 0, 0);
    cd_sync(0, &g_cd_sync_state);
    psyq_sound_music_resume();
}

static const char *executable_text(uint32 address)
{
    return (const char *)player_assets_resolve_address(address);
}

/* Direct 0x80090090..0x800901B4.  The PSX used 0x80010000 both as the TIM
 * work area and as the resident OVERBINS/BIGDIVER image, hence the final
 * reload.  A growable native buffer represents that fixed-address region. */
/* Original: FUN_80090090. */
static void screenshot_capture(void)
{
    enum
    {
        SCREENSHOT_WORK_CAPACITY = 397312
    };
    static uint8 work[SCREENSHOT_WORK_CAPACITY];
    const char *restore_path = g_scuba_stage_active == 0 ? "COMMON0\\OVERBINS.BIN" : "WATER\\BIGDIVER.BIN";
    PSX_RECT rect;
    char filename[32] = "SSHOT";
    RENDER_FRAME *frame = g_current_render_frame;
    SCREENSHOT_TIM_IMAGE *image = (SCREENSHOT_TIM_IMAGE *)work;

    strcat(filename, integer_to_text(g_screenshot_index));
    strcat(filename, ".TIM");
    ++g_screenshot_index;

    image->magic = 0x10;
    image->flags = 2;
    image->image_block_size = 0x2580c;
    rect.x = frame->display_env.disp.x;
    rect.y = frame->display_env.disp.y;
    rect.w = 0x140;
    rect.h = 0xf0;
    image->rectangle.x = rect.x;
    image->rectangle.y = rect.y;
    image->rectangle.w = rect.w;
    image->rectangle.h = rect.h;
    StoreImage(&rect, (uint32 *)image->pixels);
    DrawSync(0);
    game_file_write(filename, work, 0x25814);
    game_file_read(restore_path, work);
}

/* The original environments are embedded in the two frame buffers:
 * DRAWENV at +0x1204 and DISPENV at +0x1260. */
static void set_def_draw_env(DRAWENV *env, sint32 x, sint32 y, sint32 w, sint32 h)
{
    env->clip.x = (sint16)x;
    env->clip.y = (sint16)y;
    env->clip.w = (sint16)w;
    env->clip.h = (sint16)h;
    env->ofs[0] = (sint16)x;
    env->ofs[1] = (sint16)y;
    env->dtd = 1;
    env->dfe = 0;
    env->isbg = 0;
}

static void set_def_disp_env(DISPENV *env, sint32 x, sint32 y, sint32 w, sint32 h)
{
    env->disp.x = (sint16)x;
    env->disp.y = (sint16)y;
    env->disp.w = (sint16)w;
    env->disp.h = (sint16)h;
    env->screen.x = 0;
    env->screen.y = 0;
    env->screen.w = 0;
    env->screen.h = 0;
    env->isinter = 0;
    env->isrgb24 = 0;
}

/* Original: FUN_8008EB88. */
GDB_CALL void display_buffers_initialize(void)
{
    sint32 half_width;
    sint32 half_height;
    RENDER_FRAME *first = &g_render_frame_buffer_0;
    RENDER_FRAME *second = &g_render_frame_buffer_1;

    set_def_draw_env(&first->drawenv, 0, 0, g_screen_width, g_screen_height);
    set_def_disp_env(&first->display_env, 0, g_screen_height, g_screen_width, g_screen_height);
    set_def_draw_env(&second->drawenv, 0, g_screen_height, g_screen_width, g_screen_height);
    set_def_disp_env(&second->display_env, 0, 0, g_screen_width, g_screen_height);

    half_width = g_screen_width / 2;
    half_height = g_screen_height / 2;
    first->drawenv.ofs[0] = (sint16)half_width;
    first->drawenv.ofs[1] = (sint16)(half_height - 0x40);
    second->drawenv.ofs[0] = (sint16)half_width;
    second->drawenv.ofs[1] = (sint16)(half_height + g_screen_height - 0x40);
    first->display_env.screen.y = g_display_screen_y;
    second->display_env.screen.y = g_display_screen_y;

    ClearOTagR(first->ot, RENDER_OT_LENGTH);
    ClearOTagR(second->ot, RENDER_OT_LENGTH);
    g_current_render_frame = first;
}

/* Literal control flow of 0x80089144..0x800894B0.  Only the six low-level
 * GPU/CD calls above are native platform boundaries. */
/* Original: FUN_80089144. */
sint32 pause_menu_update(void)
{
    uint32 buttons;

    if (g_input_recording_mode != 2 && controller_packet_is_digital_pad(g_controller_packet) == 0)
    {
        sound_all_voices_stop();
        cd_audio_pause();
        display_buffers_initialize();
        gpu_draw_sync(0);
        g_render_frame_index ^= 1;
        g_current_render_frame = g_render_frame_index ? &g_render_frame_buffer_0 : &g_render_frame_buffer_1;
        drawenv_apply(&g_current_render_frame->drawenv);
        text_render(player_assets_executable_text_pointer(0x80082864u), 0, -0x20);
        text_render(player_assets_executable_text_pointer(0x8008275cu), 0, 0x30);
        screen_overlay_draw();
        ot_submit(g_current_render_frame->ot + 1);
        gpu_draw_sync(0);
        while (controller_packet_is_digital_pad(g_controller_packet) == 0)
        {
            do
            {
                vertical_sync_wait(0);
                buttons = controllers_read(0);
            } while ((buttons & 0x10) == 0);
        }
        vertical_sync_wait(0);
        cd_audio_play();
        return 0;
    }
    if (g_stage_index == 0x0b)
        return 0;
    if ((g_held_buttons & 0x800) == 0 && g_pause_active == 0)
    {
        g_pause_button_latched = 0;
        return 0;
    }
    if (g_pause_button_latched != 0)
        return 0;

    g_pause_button_latched = 1;
    g_pause_active = 1;
    sound_all_voices_stop();
    cd_audio_pause();
    gpu_draw_sync(0);
    g_render_frame_index ^= 1;
    g_current_render_frame = g_render_frame_index ? &g_render_frame_buffer_0 : &g_render_frame_buffer_1;
    drawenv_apply(&g_current_render_frame->drawenv);
    text_render(player_assets_executable_text_pointer(0x800827ccu), -0x18, 0);
    ot_submit(g_current_render_frame->ot + 1);
    gpu_draw_sync(0);

    for (;;)
    {
        buttons = controllers_read(0);
        if ((buttons & 0x800) == 0)
            break;
        vertical_sync_wait(0);
    }

    do
    {
        vertical_sync_wait(0);
        buttons = controllers_read(0);
        g_held_buttons = buttons;
        if ((buttons & 0x800) != 0 || g_pause_active == 0)
        {
            g_render_frame_index ^= 1;
            g_current_render_frame = g_render_frame_index ? &g_render_frame_buffer_0 : &g_render_frame_buffer_1;
            cd_audio_play();
            g_pause_active = 0;
            return 1;
        }
    } while (buttons != 0x10 || g_cd_file_io_enabled != 0);

    ot_submit(&g_current_render_frame->ot[0x480]);
    gpu_draw_sync(0);
    g_render_frame_index ^= 1;
    g_current_render_frame = g_render_frame_index ? &g_render_frame_buffer_0 : &g_render_frame_buffer_1;
    screenshot_capture();
    g_pause_active = 0;
    return 1;
}

/* Original: FUN_8008B6D4. */
void debug_player_cell_coordinates_draw(void)
{
    char text[32];
    sint32 value;
    sprintf(text, "%d", g_player_world_x / 0x4000);
    text_render(text, 0x6e, 0);
    value = g_player_world_y / 0x4000;
    if (value < 0)
        value = -value;
    sprintf(text, "%d", value);
    text_render(text, 0x6e, 0x10);
    value = g_map_depth_cells - (((g_map_depth_cells * 0xc0 - g_player_world_z / 0x100) / 0x40) + 1);
    sprintf(text, "%d", value);
    text_render(text, 0x6e, 0x20);
}

/* Original: FUN_8008BDA0. */
void debug_input_update(void)
{
    if (g_debug_cheats_enabled == 0)
        return;
    if (g_debug_coordinates_visible != 0)
        debug_player_cell_coordinates_draw();
    if ((g_held_buttons & 0xffff0000u) != 0)
    {
        if (g_stage_index != 0x0b && g_level_select_cheat_enabled != 0 && (g_pressed_buttons & 0x50000u) != 0)
            mission_complete_begin();
        if ((g_pressed_buttons & 0x1000000u) != 0)
            g_debug_coordinates_visible ^= 1;
    }
}

/* Direct translation of 0x800901C8..0x80090214.  This is display shake,
 * not a world-camera transform: the game changes DISPENV.screen.y while
 * DAT_800D3EAC/B4/B8 remain unchanged. */
/* Original: FUN_800901C8. */
void display_shake_update(void)
{
    sint16 count = g_display_shake_ticks;
    sint32 index = count / 2;
    sint16 screen_y = (sint16)(g_display_screen_y + camera_shake_curve[index]);

    g_current_render_frame->display_env.screen.y = screen_y;
    psx_set_display_offset_y((sint32)screen_y - (sint32)g_display_screen_y);
    if (count != 0)
        g_display_shake_ticks = (sint16)(count - 1);
}

sint32 ot_depth_row_bucket(sint32 visible_row)
{
    /* FUN_80093DE0 initializes DAT_800D4074 to 0x480 and subtracts 0x40
     * once for each row which passes both far-plane tests. */
    return 0x480 - visible_row * 0x40;
}

sint32 ot_depth_inherit_row_bucket(sint32 stored_bucket, sint32 row_bucket)
{
    /* FUN_80094548 writes DAT_800D4074 into FrameObject +0x14 only when
     * the producer left that field zero.  Explicit buckets are preserved. */
    return stored_bucket == 0 ? row_bucket : stored_bucket;
}

sint32 ot_depth_model_polygon_bucket(sint32 row_bucket, sint32 depth_reference_z, sint32 depth_anchor, sint32 polygon_max_vertex_z)
{
    /* FUN_800924EC selects:
     *   main_ot + row_bucket - (trunc(DAT_800D3F08 / 256) - DAT_800D3F3C)
     * and then indexes it by max transformed vertex Z for each polygon. */
    return row_bucket - (div_256_trunc(depth_reference_z) - depth_anchor) + polygon_max_vertex_z;
}

sint32 ot_depth_should_swap_cell_objects(sint32 current_world_z, sint32 next_world_z)
{
    /* FUN_80094548, 80094970..8009498C:
     *   slt v0,next_world_z,current_world_z
     *   beq v0,zero,no_swap
     * AddPrim prepends each submitted packet.  Ascending producer order is
     * therefore consumed by the GPU as the required far-to-near order. */
    return next_world_z < current_world_z;
}

sint32 ot_depth_run_self_tests(void)
{
    if (ot_depth_row_bucket(0) != 0x480)
        return 0;
    if (ot_depth_row_bucket(3) != 0x3c0)
        return 0;
    if (ot_depth_inherit_row_bucket(0, 0x340) != 0x340)
        return 0;
    if (ot_depth_inherit_row_bucket(0x100, 0x340) != 0x100)
        return 0;
    if (ot_depth_model_polygon_bucket(0x340, 0x84100, 0x841, 7) != 0x347)
        return 0;
    if (ot_depth_model_polygon_bucket(0x340, -0x101, -1, 0) != 0x340)
        return 0;
    /* FUN_800924EC's hierarchical-object path retains a caller-selected
     * depth reference.  Ordinary CELLSDAT/runtime models use FUN_800914D8's
     * fixed gp+0x3ec bucket and must not call this helper. */
    if (ot_depth_model_polygon_bucket(0x400, 0x90000, 0x680, 0x2a0) != 0x420)
        return 0;
    if (ot_depth_model_polygon_bucket(0x400, 0x92000, 0x680, 0x2a0) != 0x400)
        return 0;
    if (!ot_depth_should_swap_cell_objects(0x90000, 0x80000))
        return 0;
    if (ot_depth_should_swap_cell_objects(0x80000, 0x90000))
        return 0;
    if (ot_depth_should_swap_cell_objects(0x80000, 0x80000))
        return 0;
    return 1;
}
