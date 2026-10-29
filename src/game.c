#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_config.h"
#include "model.h"
#include "game.h"
#include "spatial.h"
#include "game_sound.h"
#include "camera.h"
#include "cc_archive.h"
#include "code_module.h"
#include "effect_update.h"
#include "game_loop.h"
#include "map_render.h"
#include "input.h"
#include "hq.h"
#include "input_record.h"
#include "level_data.h"
#include "map.h"
#include "mechanoid.h"
#include "mission.h"
#include "menu.h"
#include "object.h"
#include "game_file.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "psx_spu.h"
#include "random.h"
#include "render.h"
#include "resource_table.h"
#include "runtime_heap.h"
#include "save.h"
#include "sprite.h"
#include "text_renderer.h"

sint32 g_cd_file_io_enabled = 1;

/* Originals: DAT_800D3CAC, DAT_800D3CB0, DAT_800D3C98, DAT_800D3CB8, DAT_800D3DA4 */
/* Originals: DAT_800D3D16, DAT_800D3D1A, DAT_800D3D18, DAT_800D3D14 */
GAME_SESSION g_session = {0, 0, -1, 1};

/* Originals: DAT_800D3C88, DAT_800D3C8C, DAT_800D3C90, DAT_800D3CA0 */
static uint8 intro_seen;
static sint32 skip_pc_init = 1;
static sint32 title_video_toggle;

#define STAGE_HUB 0

#define STAGE_AIR 4

#define STAGE_R 5

#define STAGE_ALT_ENV 7

#define STAGE_SPECIAL 99

#define STAGE_ENDING 9999

/* Original data: SLES_004.74:0x800C8E48..0x800C8E7F */
const ATTRACT_DEMO g_attract_demos[ATTRACT_DEMO_COUNT] = {
    {4, 0x00000000}, {7, 0x00000000}, {10, 0x00000000}, {26, 0x00000000}, {12, 0x00000000}, {32, 0x00000000}, {-1, 0xffffffff},
};

/* Original: FUN_8009616C. */
sint32 frame_render_work_enabled(void)
{
    if (g_input_rec.mode != INPUT_RECORD_PLAYBACK)
        return 1;
    XASSERT(!(g_session.attract_demo_idx < 0 || g_session.attract_demo_idx >= ATTRACT_DEMO_COUNT));
    if (g_render_state.frame == g_attract_demos[g_session.attract_demo_idx].render_start_frame)
        SsSetMVol(0x7f, 0x7f);
    if (g_render_state.frame < g_attract_demos[g_session.attract_demo_idx].render_start_frame)
    {
        SsSetMVol(0, 0);
        return 0;
    }
    return 1;
}

/* Functions. */
/* Original: SLES_004.74:FUN_8008B8E4. */
GDB_CALL sint32 boot_language_text_load(void)
{
    sint32 text_size;
    uint8 *text_data;
    sint32 language;

    if (g_session.language != -1)
        return -1;
    {
        char sel_path[40] = "HQ\\";
        DrawSync(0);
        vram_clear();
        InitGeom();
        SetGeomOffset(0xa0, g_render_state.height / 2);
        SetGeomScreen(0x140);
        disp_buffers_initialize();
        SetDispMask(1);
        runtime_heap_initialize();
        linked_list_initialize(&g_object_list);
        sound_map_reset();
        g_sound_archive = game_file_load("SOUND\\VH.CC", &g_sound_archive_size);
        if (!archive_validate(g_sound_archive, g_sound_archive_size))
            fatal_error("SOUND ARCHIVE");
        SsInit();
        SsSetTickMode(SS_TICK60);
        SsStart();
        sound_bank_load(0x11, 0);
        vram_clear();
        spr_vram_reset(&g_game_vram_layout);
        text_font_init();
        text_size = game_file_size("HQ\\FLAGS.BIN");
        text_data = runtime_heap_allocate_sector_aligned(text_size);
        game_file_read("HQ\\FLAGS.BIN", text_data, text_size);
        resource_table_register(0x6c00, text_data, text_size, RESOURCE_COPY);
        g_spr_pal.cluts[CLUT_LANGUAGE_FLAG] = spr_clut_upload(0x6c00, 0);
        spr_create_vram_descs(0x6c00, 4, g_spr_pal.cluts[CLUT_LANGUAGE_FLAG], 0);
        DrawSync(0);
        g_game_text.entries[GAME_TEXT_MENU_FIRST + 27] = " ";
        g_game_text.entries[GAME_TEXT_MENU_FIRST + 28] = "    ";
        for (language = 0; language < 2; ++language)
        {
            frame_begin();
            g_text_renderer.color_idx = 2;
            text_render("SELECT LANGUAGE", -15 * g_text_renderer.glyph_width / 2, 2 * g_text_renderer.line_height - g_render_state.height / 2);
            end_frame_submit(1);
        }
        SsSetMVol(0x7f, 0x7f);
        language = language_menu_run();
        SsEnd();
        SsQuit();
        if (language == 0)
            language = 1;
        if (language == 1)
            strcat(sel_path, "GAMETEXT.BIN");
        if (language == 2)
            strcat(sel_path, "FRENTEXT.BIN");
        if (language == 3)
            strcat(sel_path, "GERMTEXT.BIN");
        if (language == 4)
            strcat(sel_path, "SPANTEXT.BIN");
        g_session.language = language;
        text_size = game_file_size(sel_path);
        text_data = runtime_heap_allocate_sector_aligned(text_size);
        game_file_read(sel_path, text_data, text_size);
        free(g_sound_archive);
        g_sound_archive = 0;
        g_sound_archive_size = 0;
        runtime_heap_sweep();
    }
    if (text_size < 0 || !game_text_load(text_data, text_size, g_session.language))
        fatal_error("BAD GAME TEXT");
    vram_clear();
    return 0;
}

static int game_main(void)
{
    sint32 packed;
    const VRAM_LAYOUT *vram_layout;
    sint32 music_id;
    const char *video_path;
    sint32 video_frames;
    sint32 video_mode;

    psx_set_disp_offset_y(0);

    ResetCallback();
    /* Native files are read synchronously by game_file.c */
    PadInit(0);
    ResetGraph(0);
    SetGraphDebug(0);
    InitGeom();

    VSyncCallback(render_vblank_update);

    if (skip_pc_init != 0)
    {
        g_debug_cheats_enabled = 0;
    }
    /* Frame pacing uses the high-resolution PAL timer */

    runtime_heap_initialize();
    vram_clear();
    g_render_state.width = 0x140;
    g_render_state.half_width = 0xA0;
    g_render_state.height = 0x100;
    g_map_render.vertical_extent = 0x80;
    g_render_state.disp_y = 0x15;
    SetVideoMode(1);
    startup_warning_show();
    hq_world_map_relocate_nodes();
    game_progress_load();
#if LEVEL_SELECT
    level_select_unlock_all();
#endif

    /* Play movies, run the session and release sound before restarting */
    while (!xport_isquit())
    {

        ResetCallback();
        runtime_heap_initialize();
        g_session.attract_mode = 0;
        g_camera.frozen = 0;
        g_camera.depth_offset_target = 0;
        g_map_render.background_brightness = 0;
        g_map_render.brightness = 0;
        vram_clear();
        g_model_draw.target_frame = 0;
        g_render_state.vsyncs_total = 0;

        if (g_session.play_fmv != 0)
        {
#if !defined(AP_WEB)
            if (intro_seen == 0)
            {
                str_video_play("V\\VIE.STR;1", 0xF0, 0xF3, 0);
                str_video_play("V\\KOTJ.STR;1", 0xB0, 0x85, 1);
            }
#endif
            boot_language_text_load();
            if (intro_seen == 0)
            {
                if (g_session.language == 1)
                {
                    str_video_play("V\\SCEN.STR;1", 0xB0, 0x4FF, 1);
                }
                if (g_session.language == 3)
                {
                    str_video_play("V\\SCGR.STR;1", 0xB0, 0x4FF, 1);
                }
                if (g_session.language == 4)
                {
                    str_video_play("V\\SCSP.STR;1", 0xB0, 0x4FF, 1);
                }
                if (g_session.language == 2)
                {
                    str_video_play("V\\SCFR.STR;1", 0xB0, 0x4FF, 1);
                }
                video_mode = 2;
            }
            else
            {
                video_mode = 3;
            }
            intro_seen = 1;
            if (title_video_toggle == 0)
            {
                video_path = "V\\TITLE.STR;1";
                video_frames = 0x88D;
            }
            else
            {
                video_path = "V\\T.STR;1";
                video_frames = 0x2AC;
            }
            str_video_play(video_path, 0xB0, video_frames, video_mode);
            g_session.play_fmv = 0;
            title_video_toggle ^= 1;
        }

        if (g_input_rec.mode == INPUT_RECORD_NONE)
        {
            if (g_session.next_stage == STAGE_AIR)
            {
                str_video_play("V\\AIR.STR;1", 0xB0, 0x55A, 3);
            }
            if (g_session.next_stage == STAGE_R)
            {
                str_video_play("V\\R.STR;1", 0xB0, 0x635, 3);
            }
            if ((g_session.next_stage == STAGE_SPECIAL || g_session.next_stage == STAGE_HUB) && g_session.stage == 0xD)
            {
                str_video_play("V\\CITY.STR;1", 0xB0, 0x6B7, 3);
            }
            if (g_session.next_stage == STAGE_ENDING)
            {
                str_video_play("V\\E.STR;1", 0xB0, 0x645, 0);
                disp_buffers_initialize();
                SetDispMask(1);
                spr_vram_reset(&g_game_vram_layout);
                text_font_init();
                overlay_module_select("HQ\\HQ.BIN");
                hq_loading_scr_run();
                overbins_load();
            }
        }

        if (g_session.next_stage == STAGE_ENDING)
        {
            g_session.next_stage = STAGE_SPECIAL;
        }

        if (g_session.scuba_active != 0)
        {
            spr_vram_reset(&g_game_vram_layout);
            text_font_init();
            loading_caption_show(0);
            overbins_load();
        }

        VSyncCallback(render_vblank_update);
        InitGeom();
        SetGeomOffset(0xA0, g_render_state.height / 2);
        SetGeomScreen(0x140);

        PadInit(0);
        ResetGraph(0);
        SsInit();
        disp_buffers_initialize();
        runtime_heap_initialize();
        linked_list_initialize(&g_object_list);
        linked_list_initialize(&g_auxiliary_object_list);
        linked_list_initialize(&g_ledge_trigger_list);
        g_target_indicator_enabled = 0;
        g_map_render.background_resource = 0;
        g_map_render.foreground_clut = 0;
        g_disp_shake_ticks = 0;
        g_input.held = 0;
        random_range(0);
        g_render_state.frame = 0;
        g_sel_weapon_slot = 0;
        vram_clear();

        vram_layout = &g_game_vram_layout;
        if (g_session.next_stage == STAGE_ALT_ENV)
        {
            vram_layout = &g_hq_vram_layout;
        }
        spr_vram_reset(vram_layout);
        text_font_init();

        if (g_session.next_stage == STAGE_HUB || g_session.next_stage == STAGE_SPECIAL)
        {
            loading_image_show(loading_image_next_path(), g_loading_caption_spanish);
        }
        else
        {
            SetDispMask(1);
        }

        g_camera.world_z = 0;
        g_camera.world_y = 0;
        g_camera.world_x = 0;
        g_camera.prev_player_z = 0;
        g_camera.prev_player_y = 0;
        g_camera.prev_player_x = 0;
        g_map_render.effect_depth = 0x4C0;
        g_map_render.far_z = g_map.depth * 0xC000;

        if (g_session.next_stage == STAGE_SPECIAL || g_session.next_stage == STAGE_HUB)
        {
            g_input_rec.mode = INPUT_RECORD_NONE;
            g_session.stage = 0xB;
            hq_assets_load();
            player_assets_runtime_initialize();
            overlay_module_select("HQ\\HQ.BIN");
            music_track_select(2);
            g_session.next_stage = STAGE_HUB;
            player_spawn_at_start();
            g_mission.primary_targets_done = 0;
            g_mission.primary_targets_total = 0;
            g_mission.secondary_targets_done = 0;
            g_mission.secondary_targets_total = 0;
            g_camera.manual_y_offset = 0;
            g_camera.depth_offset = 0;
            g_camera.horizontal_bias = 0;
            g_target_indicator_enabled = 0;
            g_input.pause_latched = 1;
            g_camera.prev_z = -1;
            g_camera.player_far_z_limit = -1;
            g_camera.player_near_z_limit = -1;
            g_surface_impact_cooldown = 0;
            map_grid_initialize(&g_stage_map_input);
            archive_resource_register("MAP0001.BIN", 0x24400, g_stage_archive, g_stage_archive_size);
            archive_resource_register("MAP0002.BIN", 0x24800, g_stage_archive, g_stage_archive_size);
            archive_resource_register("MAP0003.BIN", 0x24C00, g_stage_archive, g_stage_archive_size);
            archive_resource_register("MAP0004.BIN", 0x25000, g_stage_archive, g_stage_archive_size);
            archive_resource_register("MAP0005.BIN", 0x25400, g_stage_archive, g_stage_archive_size);
            archive_resource_register("MAP0006.BIN", 0x25800, g_stage_archive, g_stage_archive_size);
#if 0 /* Native save/load uses xport_save and xport_load */
            spr_archive_resource_load("CARDICON.BIN", 0x27000, 3, 0, g_stage_archive, g_stage_archive_size);
            {
                uint16 tpage = spr_clut_upload(0x27000, 0);
                spr_assign_clut_range(0x27000, 3, tpage);
            }
#endif
            effects_spawn_initial();
            hq_level_update();
            if (xport_isquit())
                break;
        }
        else
        {
            if (g_input_rec.mode == INPUT_RECORD_PLAYBACK)
            {
                g_session.attract_mode = g_session.next_attract_mode;
                g_session.next_attract_mode ^= 1;
            }
            if (g_session.language == 3 || g_session.language == 5)
            {
                g_session.attract_mode = 0;
                g_session.next_attract_mode = 0;
            }
            g_session.stage = g_session.next_stage - 1;
            loading_caption_show(0);
            hq_assets_load();
            player_assets_runtime_initialize();
            g_session.next_stage = STAGE_HUB;
            player_spawn_at_start();
            if (g_session.stage == 4)
            {
                mechanoid_create(g_player_world.x + 0x8000, g_player_world.y - 0xF000, g_player_world.z);
            }
            g_render_state.frame = 0;
            random_range(0);
            g_model_draw.rotation_z = 0;
            g_model_draw.rotation_y = 0;
            g_model_draw.rotation_x = 0;
            music_id = 0xB;
            if (g_session.attract_mode == 0)
            {
                music_id = g_stage_env->music.fields.music_id;
            }
            music_track_select(music_id);
            g_mission.primary_targets_done = 0;
            g_mission.primary_targets_total = 0;
            g_mission.secondary_targets_done = 0;
            g_mission.secondary_targets_total = 0;
            g_camera.manual_y_offset = 0;
            g_camera.depth_offset = 0;
            g_camera.horizontal_bias = 0;
            g_target_indicator_enabled = 0;
            g_input.pause_latched = 1;
            g_camera.prev_z = -1;
            g_camera.player_far_z_limit = -1;
            g_camera.player_near_z_limit = -1;
            g_surface_impact_cooldown = 0;
            map_grid_initialize(&g_stage_map_input);
            hud_bar_initialize(&g_player_health_bar, 0x10, 0x62, 0x80, 0x400);
            g_input_rec.run_len = -1;
            g_input_rec.run_val = -1;
            g_mission.timer_visible = 0;
            g_mission.timer = mission_timer_disp_create(0x60, -0x64, 0);
            packed = mission_objective_script_update(g_stage_env->mission_script);
            g_mission.primary_count = packed >> 16;
            g_mission.secondary_count = packed & 0xFFFF;
            g_mission.primary_action_present = 0;
            if (g_session.attract_mode != 0)
            {
                g_session.attract_mode = 0;
                sound_play_nonpositional(0x6F, 0, 0x40);
                g_session.attract_mode = 1;
                speech_random_request(0xE);
            }
            effects_spawn_initial();
            camera_update(g_player_world.x, g_player_world.y, g_player_world.z, 0);
            g_mission.ending = 0;
            mission_game_loop();
            if (g_input_rec.mode == INPUT_RECORD_PLAYBACK)
            {
                g_session.attract_demo_idx = (g_session.attract_demo_idx + 1);
            }
        }

        SsEnd();
        SsQuit();
        DrawSync(0);
        if (g_input_rec.mode == INPUT_RECORD_WRITE)
        {
            input_rec_append(0);
            XASSERT(!(g_input_rec.data == 0 || g_input_rec.size < 0x1000));
            app_file_write("record.bin", g_input_rec.data, 0x1000);
        }
    }
    return 0;
}

// XPORT REVISION: 2026-09-29T20:00:00Z
void xport_main(void)
{
    uint32 idx;
    xport_set_exit_code(game_main());
    VSyncCallback(0);
    input_release();
    SsEnd();
    SsQuit();
    runtime_heap_release();
    for (idx = 0; idx < RESOURCE_SLOT_COUNT; ++idx)
        resource_table_register(idx << 10, 0, 0, RESOURCE_BORROW);
    free((void *)g_mechanoid_initial_pose.data);
    memset(&g_mechanoid_initial_pose, 0, sizeof(g_mechanoid_initial_pose));
    free(g_player_asset_files.overbins);
    free(g_player_asset_files.diver);
    memset(&g_player_asset_files, 0, sizeof(g_player_asset_files));
    text_renderer_release();
    g_render_frames[0].packets.used = 0;
    g_render_frames[1].packets.used = 0;
    g_gpu_packets = 0;
    g_cur_render_frame = 0;
    g_render_frame_idx = 0;
}
