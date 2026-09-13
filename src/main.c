#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "audio/game_sound.h"
#include "camera.h"
#include "cc_archive.h"
#include "cd.h"
#include "code_module.h"
#include "effect_update.h"
#include "game_loop.h"
#include "global.h"
#include "hq.h"
#include "input_record.h"
#include "level_data.h"
#include "map.h"
#include "mechanoid.h"
#include "memory_card_platform.h"
#include "mission.h"
#include "object.h"
#include "original_file.h"
#include "platform/win/game_platform.h"
#include "platform/win/platform_file.h"
#include "player.h"
#include "psx.h"
#include "random.h"
#include "render.h"
#include "resource_table.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "stubs.h"
#include "text_renderer.h"
#include "vram_alloc.h"

/* Macros. */
/* after SsEnd; calls FUN_800c0d40 */

/* -------------------------------------------------------------------------- */
/* gp-relative / absolute globals used by main                                */
/* -------------------------------------------------------------------------- */

/* 800D3C88  gp+0x00 */
/* 800D3C8C  gp+0x04 */
/* 800D3C90  gp+0x08  sdata default 1 */
/* 800D3C98  gp+0x10  language / scenario */
/* 800D3C9C  gp+0x14 */
/* 800D3CA0  gp+0x18 */
/* current stage, 800D3CAC */
/* requested stage+1, 800D3CB0 */
/* 800D3CB8  gp+0x30 */
/* 800D3CC4  gp+0x3C */
/* gp+0x44 */
/* gp+0x1E0 */
/* 800D3E6C  gp+0x1E4  set to 0x100 */
/* gp+0x1F0 */
/* gp+0x21C */
/* gp+0x234 -> 800827E4 */
/* gp+0x2B8  STR mode 2/3 */
/* gp+0x2F0 */
/* gp+0x304 */
/* gp+0x308  set to 0xA0 */
/* PAL frame height, initialized to 0x100 */
/* gp+0x328 */
/* gp+0x32C */
/* gp+0x34C */
/* 800D3FD2 */
/* gp+0x374  set to 0x80 */
/* gp+0x3F4 */
/* gp+0x480 */
/* gp+0x484 */
/* gp+0x4E0  initial SP snapshot */
/* gp+0x4F4 */
/* gp+0x504 */
/* gp+0x518 */
/* gp+0x564  set to 0x140 */
/* gp+0x5A0 */
/* set to 800709FC */
/* Originals: DAT_800EA6C8, DAT_800EA6CC, DAT_800EA6CD, DAT_800EA6CE,
 * DAT_800ED8B8.  The three color labels are embedded POLY_FT4 fields. */
#define g_shared_quad_prim_red (g_shared_quad_prim.r0)

#define g_shared_quad_prim_green (g_shared_quad_prim.g0)

#define g_shared_quad_prim_blue (g_shared_quad_prim.b0)

#define STAGE_HUB 0

#define STAGE_AIR 4

#define STAGE_R 5

#define STAGE_ALT_ENV 7

#define STAGE_SPECIAL 99

#define STAGE_ENDING 9999

/* Functions. */
/* Original: FUN_8008B8E4. */
GDB_CALL sint32 boot_language_text_load(void)
{
    const char *text_path;
    sint32 text_size;
    uint8 *text_data;
    sint32 language;
    uint32 *entry;

    if (g_language_id != -1)
        return -1;
    if (g_language_assets_preloaded == 1)
    {
        text_path = "HQ\\GAMETEXT.BIN";
        text_size = game_file_size(text_path);
        text_data = (uint8 *)runtime_heap_allocate_sector_aligned(text_size);
        g_language_id = 1;
        game_file_read(text_path, text_data);
    }
    else
    {
        char selected_path[40] = "HQ\\";
        DrawSync(0);
        vram_clear();
        InitGeom();
        SetGeomOffset(0xa0, g_screen_height / 2);
        SetGeomScreen(0x140);
        display_buffers_initialize();
        display_mask_set(1);
        runtime_heap_initialize();
        linked_list_initialize(g_object_list);
        g_sound_archive = runtime_heap_allocate_sector_aligned(0x91080);
        sound_map_reset();
        game_file_read("SOUND\\VH.CC", g_sound_archive);
        SsInit();
        SsSetTickMode(1);
        SsStart();
        sound_bank_load(0x11, 0);
        vram_clear();
        sprite_vram_reset(g_sprite_vram_regions_game);
        font_sprite_initialize();
        text_size = game_file_size("HQ\\FLAGS.BIN");
        text_data = (uint8 *)runtime_heap_allocate_sector_aligned(text_size);
        game_file_read("HQ\\FLAGS.BIN", text_data);
        resource_table_register(0x6c00, text_data);
        g_language_flag_clut = (sint16)sprite_clut_upload(0x6c00, 0, 0, 0);
        sprite_create_vram_descriptors(0x6c00, 4, g_language_flag_clut, 0);
        gpu_draw_sync(0);
        SsSetMVol(0x7f, 0x7f);
#if 0 /* Native policy: language selection UI is excluded; English is fixed. */
        language=(sint16)menu_run(
            (void *)player_assets_executable_address(0x800c8054u),0,0x2a);
        vertical_sync_wait(0x1e);
#else
        language = 1;
#endif
        SsEnd();
        spu_shutdown();
        if (language == 0)
            language = 1;
        if (language == 1)
            strcat(selected_path, "GAMETEXT.BIN");
        if (language == 2)
            strcat(selected_path, "FRENTEXT.BIN");
        if (language == 3)
            strcat(selected_path, "GERMTEXT.BIN");
        if (language == 4)
            strcat(selected_path, "SPANTEXT.BIN");
        g_language_id = language;
        text_size = game_file_size(selected_path);
        text_data = (uint8 *)runtime_heap_allocate_sector_aligned(text_size);
        game_file_read(selected_path, text_data);
        runtime_heap_defer_free(g_sound_archive);
        runtime_heap_sweep();
    }
    fixed_address_copy((void *)0x80082640u, text_data, text_size);
    entry = (uint32 *)player_assets_executable_address(0x80082640u);
    if (*entry != 0xffffffffu)
    {
        do
        {
            if (*entry != 0)
                *entry += 0x80082640u;
            ++entry;
        } while (*entry != 0xffffffffu);
    }
    vram_clear();
    return 0;
}

static void mission_launch_trace(const char *event)
{
    FILE *trace;
    if (getenv("OA_MISSION_LAUNCH_TRACE") == 0)
        return;
    trace = fopen("mission_launch_port.log", "a");
    if (trace == 0)
        return;
    fprintf(trace, "%s stage=%d next=%d mode=%d env=%p player=%p\n", event, g_stage_index, g_next_stage_index, g_input_recording_mode, (void *)g_stage_env, g_player);
    fclose(trace);
}

sint32 main(void)
{
    uint32 packed;
    uint16 *env;
    uint16 tpage;
    sint32 musicId;

    if (!ot_depth_run_self_tests())
    {
        fprintf(stderr, "OT depth arithmetic self-test failed\n");
        return 2;
    }
    g_initial_stack_pointer = 0; /* PSX stack snapshot has no Windows equivalent. */
    ResetCallback();
    /* Windows port: files are read synchronously by platform_file.c. */
    PadInit(0);
    psx_game_platform_configure();
    ResetGraph(0);
    SetGraphDebug(0);
    InitGeom();

    if (g_skipPcinit != 0)
    {
        g_debug_cheats_enabled = 0;
    }
    cd_track_table_initialize();
    /* Windows port: frame pacing uses the high-resolution PAL timer. */

    runtime_heap_initialize();
    vram_clear();
    g_screen_width = 0x140;
    g_screen_half_width = 0xA0;
    g_screen_height = 0x100;
    g_vertical_cull_extent = 0x80;
    g_display_screen_y = 0x15;
    video_mode_set(1);
    startup_warning_show();
    hq_world_map_relocate_nodes();
#if LEVEL_SELECT
    level_select_unlock_all();
#endif
    DAT_800c8ac4 = (void *)0x800709FC;
    /* The original stores literal address 0x800827E4 here.  A host C symbol
     * named after that address is not the table; resolve the executable data
     * range which contains its original 32-bit string pointers. */
    g_menu_text_pointer_table = (void *)player_assets_executable_address(0x800827e4u);

    /* Host-only diagnostic entry point. The unmodified path still starts in
     * HQ; OA_START_STAGE=0 selects the original stage-0 branch directly so
     * Mission 0 can be exercised repeatedly without changing game logic. */
    {
        const char *start_stage = getenv("OA_START_STAGE");
        if (start_stage != 0)
            g_next_stage_index = (sint32)strtol(start_stage, 0, 0) + 1;
    }

    /* Session loop: FMV / load / run / tear down sound, then restart. */
    while (!psx_quit_requested())
    {
        g_iterations += 1;
        printf("**** ITERATIONS = %d ****\n", g_iterations);

        ResetCallback();
        runtime_heap_initialize();
        g_attract_mode = 0;
        g_hq_world_map_active = 0;
        g_camera_depth_offset_target = 0;
        g_background_brightness_bias = 0;
        g_scene_brightness_bias = 0;
        vram_clear();
        g_model_render_frame = 0;
        g_vsync_count_total = 0;

        /* STR playback is a platform boundary.  Keep the surrounding PAL
         * game-side state flow, in particular the mandatory language/text
         * loader at 0x80087FCC. */
        if (g_play_fmv != 0)
        {
#if 0 /* Native STR decoder not implemented. */
            if (g_fmvSeen == 0) {
                str_video_play("V\\VIE.STR;1", 0xF0, 0xF3, 0);
                str_video_play("V\\KOTJ.STR;1", 0xB0, 0x85, 1);
            }
#endif
            boot_language_text_load();
            if (g_fmvSeen == 0)
            {
#if 0 /* Native STR decoder not implemented. */
                if (g_language_id == 1) {
                    str_video_play("V\\SCEN.STR;1", 0xB0, 0x4FF, 1);
                }
                if (g_language_id == 3) {
                    str_video_play("V\\SCGR.STR;1", 0xB0, 0x4FF, 1);
                }
                if (g_language_id == 4) {
                    str_video_play("V\\SCSP.STR;1", 0xB0, 0x4FF, 1);
                }
                if (g_language_id == 2) {
                    str_video_play("V\\SCFR.STR;1", 0xB0, 0x4FF, 1);
                }
#endif
                g_shared_scratch_value = 2;
            }
            else
            {
                g_shared_scratch_value = 3;
            }
            g_fmvSeen = 1;
#if 0 /* Native STR decoder not implemented. */
            if (g_titleStrToggle == 0) {
                strPath = "V\\TITLE.STR;1";
                strFrames = 0x88D;
            } else {
                strPath = "V\\T.STR;1";
                strFrames = 0x2AC;
            }
            str_video_play(strPath, 0xB0, strFrames, g_shared_scratch_value);
#endif
            g_play_fmv = 0;
            g_titleStrToggle ^= 1;
        }

        if (g_input_recording_mode == 0)
        {
            /* STR playback disabled. Keep the non-video ending transition. */
#if 0
            if (g_next_stage_index == STAGE_AIR) {
                str_video_play("V\\AIR.STR;1", 0xB0, 0x55A, 3);
            }
            if (g_next_stage_index == STAGE_R) {
                str_video_play("V\\R.STR;1", 0xB0, 0x635, 3);
            }
            if ((g_next_stage_index == STAGE_SPECIAL || g_next_stage_index == STAGE_HUB) &&
                g_stage_index == 0xD) {
                str_video_play("V\\CITY.STR;1", 0xB0, 0x6B7, 3);
            }
#endif
            if (g_next_stage_index == STAGE_ENDING)
            {
                /* FUN_8008b7a8("V\\E.STR;1", 0xB0, 0x645, 0); */
                display_buffers_initialize();
                SetDispMask(1);
                sprite_vram_reset(g_sprite_vram_regions_game);
                font_sprite_initialize();
                overlay_module_load("HQ\\HQ.BIN");
                hq_loading_screen_run();
                overbins_load();
            }
        }

        if (g_next_stage_index == STAGE_ENDING)
        {
            g_next_stage_index = STAGE_SPECIAL;
        }

        if (g_scuba_stage_active != 0)
        {
            sprite_vram_reset(g_sprite_vram_regions_game);
            font_sprite_initialize();
            loading_caption_show(0);
            overbins_load();
        }

        /* No VSync callback on Windows. */
        InitGeom();
        SetGeomOffset(0xA0, g_screen_height / 2);
        SetGeomScreen(0x140);

        PadInit(0);
        ResetGraph(0);
        SsInit();
        display_buffers_initialize();
        runtime_heap_initialize();
        linked_list_initialize(g_object_list);
        linked_list_initialize(g_auxiliary_object_list);
        linked_list_initialize(g_ledge_trigger_list);
        g_player_input_disabled = 0;
        g_target_indicator_enabled = 0;
        g_stage_background_resource = 0;
        g_foreground_sprite_clut_offset = 0;
        g_display_shake_ticks = 0;
        g_held_buttons = 0;
        random_range(0);
        g_frame_counter = 0;
        g_selected_weapon_slot = 0;
        SetPolyFT4(&g_shared_quad_prim);
        SetPolyFT3(&g_shared_triangle_prim);
        vram_clear();

        env = g_sprite_vram_regions_game;
        if (g_next_stage_index == STAGE_ALT_ENV)
        {
            env = g_sprite_vram_regions_hq;
        }
        sprite_vram_reset(env);
        font_sprite_initialize();

        if (g_next_stage_index == STAGE_HUB || g_next_stage_index == STAGE_SPECIAL)
        {
            loading_image_show(loading_image_next_path(), g_loading_caption_spanish);
        }
        else
        {
            SetDispMask(1);
        }

        g_shared_quad_prim_blue = 0x80;
        g_shared_quad_prim_green = 0x80;
        g_shared_quad_prim_red = 0x80;
        g_camera_world_z = 0;
        g_camera_world_y = 0;
        g_camera_world_x = 0;
        g_previous_player_world_z = 0;
        g_previous_player_world_y = 0;
        g_previous_player_world_x = 0;
        g_effect_visibility_depth_cells = 0x4C0;
        g_scene_far_z = g_map_depth_cells * 0xC000;
        g_runtime_scratch_buffer_a = runtime_heap_allocate(0x100);
        g_runtime_scratch_buffer_b = runtime_heap_allocate(0x100);
        SsSetSerialAttr(0, 0, 1);
        g_same_stage_selected = 0;

        if (g_next_stage_index == STAGE_SPECIAL || g_next_stage_index == STAGE_HUB)
        {
            g_input_recording_mode = 0;
            g_stage_index = 0xB;
            hq_assets_load();
            player_assets_runtime_initialize();
            overlay_module_load("HQ\\HQ.BIN");
            music_track_select(2, 1);
            g_next_stage_index = STAGE_HUB;
            player_spawn_at_start();
            g_primary_target_current_count = 0;
            g_primary_target_total_count = 0;
            g_secondary_target_current_count = 0;
            g_secondary_target_total_count = 0;
            g_camera_manual_y_offset = 0;
            g_camera_depth_offset = 0;
            g_camera_horizontal_bias = 0;
            g_player_input_disabled = 0;
            g_target_indicator_enabled = 0;
            g_pause_button_latched = 1;
            DAT_800d3ccc = 1;
            g_previous_camera_world_z = -1;
            g_player_far_z_limit = -1;
            g_player_near_z_limit = -1;
            g_surface_impact_cooldown = 0;
            map_grid_initialize();
            resource_table_register(0x24400, archive_member_find("MAP0001.BIN", g_stage_archive));
            resource_table_register(0x24800, archive_member_find("MAP0002.BIN", g_stage_archive));
            resource_table_register(0x24C00, archive_member_find("MAP0003.BIN", g_stage_archive));
            resource_table_register(0x25000, archive_member_find("MAP0004.BIN", g_stage_archive));
            resource_table_register(0x25400, archive_member_find("MAP0005.BIN", g_stage_archive));
            resource_table_register(0x25800, archive_member_find("MAP0006.BIN", g_stage_archive));
            sprite_archive_resource_load("CARDICON.BIN", 0x27000, 3, 0, g_stage_archive);
            tpage = sprite_clut_upload(0x27000, 0, 0, 0);
            sprite_assign_clut_range(0x27000, 3, tpage);
            effects_spawn_initial();
            hq_level_update();
            if (psx_quit_requested())
                break;
        }
        else
        {
            if (g_input_recording_mode == 2)
            {
                g_attract_mode = g_next_attract_mode;
                g_next_attract_mode ^= 1;
            }
            g_same_stage_selected = (uint32)(g_stage_index == g_next_stage_index - 1);
            if (g_language_id == 3 || g_language_id == 5)
            {
                g_attract_mode = 0;
                g_next_attract_mode = 0;
            }
            g_stage_index = g_next_stage_index - 1;
            mission_launch_trace("MISSION_LOAD");
            loading_caption_show(0);
            hq_assets_load();
            player_assets_runtime_initialize();
            g_next_stage_index = STAGE_HUB;
            player_spawn_at_start();
            if (g_stage_index == 4)
            {
                mechanoid_create(g_player_world_x + 0x8000, g_player_world_y - 0xF000, g_player_world_z);
            }
            g_frame_counter = 0;
            random_range(0);
            g_current_model_rotation_z = 0;
            g_current_model_rotation_y = 0;
            g_current_model_rotation_x = 0;
            musicId = 0xB;
            if (g_attract_mode == 0)
            {
                musicId = g_stage_env->music.fields.music_id;
            }
            music_track_select(musicId, 1);
            g_primary_target_current_count = 0;
            g_primary_target_total_count = 0;
            g_secondary_target_current_count = 0;
            g_secondary_target_total_count = 0;
            g_camera_manual_y_offset = 0;
            g_camera_depth_offset = 0;
            g_camera_horizontal_bias = 0;
            g_player_input_disabled = 0;
            g_target_indicator_enabled = 0;
            g_pause_button_latched = 1;
            g_previous_camera_world_z = -1;
            g_player_far_z_limit = -1;
            g_player_near_z_limit = -1;
            g_surface_impact_cooldown = 0;
            DAT_800d3ccc = 1;
            map_grid_initialize();
            hud_bar_initialize(&g_player_health_bar, 0x10, 0x62, 0x80, 0x400);
            g_input_run_length = -1;
            g_input_run_value = -1;
            g_mission_timer_visible = 0;
            g_mission_timer_display = mission_timer_display_create(0x60, -0x64, 0);
            packed = mission_objective_script_update(g_mission_script);
            g_primary_objective_count = (sint32)packed >> 16;
            g_secondary_objective_count = packed & 0xFFFF;
            g_primary_action_objective_present = 0;
            if (g_attract_mode != 0)
            {
                g_attract_mode = 0;
                sound_play_nonpositional(0x6F, 0, 0x40);
                g_attract_mode = 1;
                speech_random_request(0xE);
            }
            effects_spawn_initial();
            camera_update(g_player_world_x, g_player_world_y, g_player_world_z, 0, 0);
            g_mission_ending = 0;
            psx_smoke_mission_start();
            mission_launch_trace("MISSION_RUNTIME");
            mission_game_loop();
            if (g_input_recording_mode == 2)
            {
                g_attract_demo_index += 1;
            }
        }

        SsEnd();
        spu_shutdown();
        DrawSync(0);
        if (g_input_recording_mode == 1)
        {
            input_record_append(0);
            app_file_write("record.bin", g_input_record_buffer, 0x1000);
        }
    }
    return 0;
}
