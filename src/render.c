#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_math.h"
#include "map.h"
#include "psx_gpu.h"
#include "game_sound.h"
#include "game.h"
#include "map_render.h"
#include "input_record.h"
#include "mission.h"
#include "object.h"
#include "game_file.h"
#include "game_runtime.h"
#include "input.h"
#include "player.h"
#include "psx.h"
#include "render.h"
#include "sprite.h"
#include "runtime_heap.h"
#include "text_renderer.h"

/* Original: FUN_80088DC0. */
GDB_CALL void fatal_error(const char *message)
{
    DrawSync(0);
    while (!xport_isquit() && pads_read() != 0)
        vertical_sync_wait(0);
    while (!xport_isquit() && pads_read() == 0)
    {
        RENDER_FRAME *frame;
        ++g_render_state.frame;
        frame = &g_render_frames[(g_render_state.frame & 1) ? 0 : 1];
        g_cur_render_frame = frame;
        g_gpu_packets = &frame->packets;
        g_gpu_packets->used = 0;
        ClearOTag(frame->ot, RENDER_OT_LENGTH);
        g_text_renderer.color_idx = 1;
        text_render(message, -0x8c, 0);
        DrawSync(0);
        vertical_sync_wait(0);
        PutDrawEnv(&frame->drawenv);
        PutDispEnv(&frame->disp_env);
        ot_submit(frame->ot);
    }
    while (!xport_isquit() && pads_read() != 0)
        vertical_sync_wait(0);
}

/* Original: FUN_80088EFC. */
void fatal_error_with_val(const char *prefix, sint32 val)
{
    char message[40];
    snprintf(message, sizeof(message), "%s%s", prefix, integer_to_text(val));
    fatal_error(message);
}

static sint32 g_screenshot_idx;

static sint32 g_debug_coords_visible;

/* Originals: DAT_800D41EC, DAT_800D3E6C, DAT_800D3F90, DAT_800D3FD2 */
/* Originals: DAT_800D41A0, DAT_800E2DC8, DAT_800E3398 */
RENDER_STATE g_render_state;

/* Original: FUN_800AEC34. */
void render_vblank_update(void)
{
    ++g_render_state.vsyncs_this_frame;
    ++g_render_state.vsyncs_total;
    XASSERT(g_cd_file_io_enabled != 0);
}

/* Original: SLES_004.74:DAT_800D41BC */
POLY_FT4 g_transient_prims[TRANSIENT_PRIM_LIMIT];

/* Original: SLES_004.74:DAT_800D401C */
POLY_FT4 *g_next_transient_prim = g_transient_prims;

/* Original: DAT_800EDCB8. */
/* Original: DAT_800F4520. */
/* Native array elements 0 and 1 retain the two resident frame identities */
__declspec(dllexport) RENDER_FRAME g_render_frames[2];

/* Originals: DAT_800D4024, DAT_800D4028, DAT_800D402C */
RENDER_FRAME *g_cur_render_frame;

/* Original: DAT_800D405C. */
/* Allocation owner remains independent of the target OT */
RENDER_PACKET_ARENA *g_gpu_packets;

/* Original: DAT_800D3CF2. */
uint16 g_render_frame_idx;

/* Original: DAT_800D3CD8. */
static sint32 pause_active;

/* Original: DAT_800D3E8C. */
sint16 g_disp_shake_ticks;

/* Original: FUN_800901B8. */
void disp_shake_start(void)
{
    g_disp_shake_ticks = 0x2c;
}

/* Variables. */
/* Semantic native-port module; original address comments are preserved. */
/* Signed bytes at 0x800C879C.  FUN_800901C8 indexes this table with the
 * signed shake timer divided by two, exactly as the original MIPS does. */
static const sint8 camera_shake_curve[23] = {0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6, -6, 7, -7, 8, -8, 9, -9, 10, -10, 11, -11};

/* Original: FUN_800B1304. */
GDB_CALL void vertical_sync_wait(sint32 mode)
{
    VSync(0);
}

/* Functions. */
/* Original: FUN_80088978. */
GDB_CALL void frame_begin(void)
{
    uint32 buttons;
    RENDER_FRAME *frame;
    psx_begin_frame();

    g_render_state.vsyncs_this_frame = 0;
    g_map_render.background_disabled = 0;

    g_render_frame_idx ^= 1;
    g_cur_render_frame = &g_render_frames[g_render_frame_idx ^ 1];
    frame = g_cur_render_frame;
    g_gpu_packets = &frame->packets;
    g_gpu_packets->used = 0;

    ClearOTagR(frame->ot, RENDER_OT_LENGTH);
    ++g_render_state.frame;

    buttons = pads_read();
    g_input.held = buttons;
    g_input.pressed = buttons & ~g_input.prev;
}

/* Original: FUN_80088A3C. */
GDB_CALL void end_frame_submit(sint32 mode)
{
    RENDER_FRAME *frame = g_cur_render_frame;
    /* 80088A4C/80088A54: release deferred VRAM descriptors before sweeping
     * their heap blocks.  Door entry/exit sprites allocate one descriptor
     * each, so omitting these calls exhausts/overlaps their dynamic VRAM
     * regions after the first transition. */
    spr_vram_release_deferred();
    runtime_heap_sweep();
    DrawSync(0);
    if (g_map_render.spr_subdiv != 0)
        --g_map_render.spr_subdiv;
    if (g_render_state.vsyncs_this_frame != 0)
        g_map_render.spr_subdiv = 0x1e;
    else
        VSync(0);
    if (g_gpu_packets->used >= RENDER_PRIM_CAPACITY)
        fatal_error("PRIM OVERFLOW");
    if (mode == 0)
        frame->drawenv.isbg = 1;
    g_render_frames[0].disp_env.screen.h = g_render_state.height;
    g_render_frames[1].disp_env.screen.h = g_render_state.height;
    PutDispEnv(&frame->disp_env);
    PutDrawEnv(&frame->drawenv);
    frame->drawenv.isbg = 0;
    frame->drawenv.dtd = 0;
    DrawOTag(&frame->ot[RENDER_OT_LENGTH - 1]);
    psx_end_frame();
    g_sound_handles_invalidated = 0;
    g_input.prev = g_input.held;
}

/* Native GPU submission boundary. Original 800FDDD4 submits before the
 * input wait at 800FDDE4; that path never calls the frame-loop presenter. */
/* Original: FUN_800B36F4. */
GDB_CALL void ot_submit(uint32 *ot)
{
    DrawOTag(ot);
    psx_end_frame();
}

/* Exact call sequences at 0x800A9F60 and 0x800A9FB0. */
/* Original: FUN_800A9F60. */
void cd_audio_pause(void)
{
    CdControl(CdlPause, 0, 0);
}

/* Original: FUN_800A9FB0. */
void cd_audio_play(void)
{
    CdControl(CdlPlay, 0, 0);
}

/* Screenshot TIM bytes have one private owner, independent of player assets */
typedef struct
{
    uint32 magic, flags, image_block_size;
    PSX_RECT rect;
    uint16 pix[320 * 240];
} SCREENSHOT_TIM_IMAGE;

typedef char SCREENSHOT_TIM_RECTANGLE_AT_0C[offsetof(SCREENSHOT_TIM_IMAGE, rect) == 0x0c ? 1 : -1];
typedef char SCREENSHOT_TIM_PIXELS_AT_14[offsetof(SCREENSHOT_TIM_IMAGE, pix) == 0x14 ? 1 : -1];
typedef char SCREENSHOT_TIM_SIZE_25814[sizeof(SCREENSHOT_TIM_IMAGE) == 0x25814 ? 1 : -1];

/* Original: FUN_80090090. */
static void screenshot_capture(void)
{
    static SCREENSHOT_TIM_IMAGE image;
    PSX_RECT rect;
    char filename[32] = "SSHOT";
    RENDER_FRAME *frame = g_cur_render_frame;

    strcat(filename, integer_to_text(g_screenshot_idx));
    strcat(filename, ".TIM");
    g_screenshot_idx = (g_screenshot_idx + 1u);

    image.magic = 0x10;
    image.flags = 2;
    image.image_block_size = 0x2580c;
    rect.x = frame->disp_env.disp.x;
    rect.y = frame->disp_env.disp.y;
    rect.w = 0x140;
    rect.h = 0xf0;
    image.rect.x = rect.x;
    image.rect.y = rect.y;
    image.rect.w = rect.w;
    image.rect.h = rect.h;
    StoreImage(&rect, (uint32 *)image.pix);
    DrawSync(0);
    game_file_write(filename, &image, sizeof(image));
}

/* Original: FUN_8008EB88. */
GDB_CALL void disp_buffers_initialize(void)
{
    sint32 half_width;
    sint32 half_height;
    RENDER_FRAME *frame0 = &g_render_frames[0];
    RENDER_FRAME *frame1 = &g_render_frames[1];

    sint32 packets_registered = gpu_register_packet_range(g_render_frames, sizeof(g_render_frames));
    XASSERT(packets_registered);
    (void)packets_registered;

    SetDefDrawEnv(&frame0->drawenv, 0, 0, g_render_state.width, g_render_state.height);
    SetDefDispEnv(&frame0->disp_env, 0, g_render_state.height, g_render_state.width, g_render_state.height);
    SetDefDrawEnv(&frame1->drawenv, 0, g_render_state.height, g_render_state.width, g_render_state.height);
    SetDefDispEnv(&frame1->disp_env, 0, 0, g_render_state.width, g_render_state.height);

    half_width = g_render_state.width / 2;
    half_height = g_render_state.height / 2;
    frame0->drawenv.ofs[0] = half_width;
    frame0->drawenv.ofs[1] = (half_height - 0x40);
    frame1->drawenv.ofs[0] = half_width;
    frame1->drawenv.ofs[1] = (half_height + g_render_state.height - 0x40);
    frame0->disp_env.screen.y = g_render_state.disp_y;
    frame1->disp_env.screen.y = g_render_state.disp_y;

    ClearOTagR(frame0->ot, RENDER_OT_LENGTH);
    ClearOTagR(frame1->ot, RENDER_OT_LENGTH);
    g_cur_render_frame = frame0;
}

/* Literal control flow of 0x80089144..0x800894B0.  Only the six low-level
 * GPU/CD calls above are native platform boundaries. */
/* Original: FUN_80089144. */
sint32 pause_menu_update(void)
{
    uint32 buttons;
    if (xport_isquit())
        goto host_quit;

    if (g_input_rec.mode != INPUT_RECORD_PLAYBACK && pad_packet_is_digital_pad(g_input.packet[0]) == 0)
    {
        sound_all_voices_stop();
        cd_audio_pause();
        disp_buffers_initialize();
        DrawSync(0);
        g_render_frame_idx ^= 1;
        g_cur_render_frame = &g_render_frames[g_render_frame_idx ^ 1];
        PutDrawEnv(&g_cur_render_frame->drawenv);
        text_render(g_game_text.entries[137], 0, -0x20);
        text_render(g_game_text.entries[71], 0, 0x30);
        scr_overlay_draw();
        ot_submit(g_cur_render_frame->ot + 1);
        DrawSync(0);
        while (pad_packet_is_digital_pad(g_input.packet[0]) == 0)
        {
            do
            {
                vertical_sync_wait(0);
                buttons = pads_read();
                if (xport_isquit())
                    goto host_quit;
            } while ((buttons & PADRup) == 0);
        }
        vertical_sync_wait(0);
        cd_audio_play();
        return 0;
    }
    if (g_session.stage == 0x0b)
        return 0;
    if ((g_input.held & PADstart) == 0 && pause_active == 0)
    {
        g_input.pause_latched = 0;
        return 0;
    }
    if (g_input.pause_latched != 0)
        return 0;

    g_input.pause_latched = 1;
    pause_active = 1;
    sound_all_voices_stop();
    cd_audio_pause();
    DrawSync(0);
    g_render_frame_idx ^= 1;
    g_cur_render_frame = &g_render_frames[g_render_frame_idx ^ 1];
    PutDrawEnv(&g_cur_render_frame->drawenv);
    text_render(g_game_text.entries[99], -0x18, 0);
    ot_submit(g_cur_render_frame->ot + 1);
    DrawSync(0);

    for (;;)
    {
        buttons = pads_read();
        if (xport_isquit())
            goto host_quit;
        if ((buttons & PADstart) == 0)
            break;
        vertical_sync_wait(0);
    }

    do
    {
        vertical_sync_wait(0);
        buttons = pads_read();
        if (xport_isquit())
            goto host_quit;
        g_input.held = buttons;
        if ((buttons & PADstart) != 0 || pause_active == 0)
        {
            g_render_frame_idx ^= 1;
            g_cur_render_frame = &g_render_frames[g_render_frame_idx ^ 1];
            cd_audio_play();
            pause_active = 0;
            return 1;
        }
    } while (buttons != PADRup || g_cd_file_io_enabled != 0);

    ot_submit(&g_cur_render_frame->ot[0x480]);
    DrawSync(0);
    g_render_frame_idx ^= 1;
    g_cur_render_frame = &g_render_frames[g_render_frame_idx ^ 1];
    screenshot_capture();
    pause_active = 0;
    return 1;

host_quit:
    /* Host shutdown must leave the blocking input loop */
    pause_active = 0;
    return 0;
}

/* Original: FUN_8008B6D4. */
static void debug_player_cell_coords_draw(void)
{
    char text[32];
    sint32 val;
    snprintf(text, sizeof(text), "%d", g_player_world.x / 0x4000);
    text_render(text, 0x6e, 0);
    val = g_player_world.y / 0x4000;
    if (val < 0)
        val = -val;
    snprintf(text, sizeof(text), "%d", val);
    text_render(text, 0x6e, 0x10);
    val = g_map.depth - (((g_map.depth * 0xc0 - g_player_world.z / 0x100) / 0x40) + 1);
    snprintf(text, sizeof(text), "%d", val);
    text_render(text, 0x6e, 0x20);
}

/* Original: FUN_8008BDA0. */
void debug_input_update(void)
{
    if (g_debug_cheats_enabled == 0)
        return;
    if (g_debug_coords_visible != 0)
        debug_player_cell_coords_draw();
    if ((g_input.held & 0xffff0000u) != 0)
    {
        if (g_session.stage != 0x0b && g_level_select_cheat_enabled != 0 && (g_input.pressed & 0x50000u) != 0)
            mission_complete_begin();
        if ((g_input.pressed & 0x1000000u) != 0)
            g_debug_coords_visible ^= 1;
    }
}

/* Direct translation of 0x800901C8..0x80090214.  This is display shake,
 * not a world-camera transform: the game changes DISPENV.screen.y while
 * DAT_800D3EAC/B4/B8 remain unchanged. */
/* Original: FUN_800901C8. */
void disp_shake_update(void)
{
    sint16 count = g_disp_shake_ticks;
    sint32 idx = count / 2;
    sint16 scr_y = g_render_state.disp_y + camera_shake_curve[idx];

    g_cur_render_frame->disp_env.screen.y = scr_y;
    psx_set_disp_offset_y(scr_y - g_render_state.disp_y);
    if (count != 0)
        g_disp_shake_ticks = (count - 1);
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

static sint32 ot_depth_model_polygon_bucket(sint32 row_bucket, sint32 depth_reference_z, sint32 depth_anchor, sint32 polygon_max_vertex_z)
{
    /* FUN_800924EC selects:
     *   main_ot + row_bucket - (trunc(DAT_800D3F08 / 256) - DAT_800D3F3C)
     * and then indexes it by max transformed vertex Z for each polygon. */
    return row_bucket - (div_256_trunc(depth_reference_z) - depth_anchor) + polygon_max_vertex_z;
}

sint32 ot_depth_should_swap_cell_objects(sint32 cur_world_z, sint32 next_world_z)
{
    /* FUN_80094548, 80094970..8009498C:
     *   slt v0,next_world_z,current_world_z
     *   beq v0,zero,no_swap
     * AddPrim prepends each submitted packet.  Ascending producer order is
     * therefore consumed by the GPU as the required far-to-near order. */
    return next_world_z < cur_world_z;
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
