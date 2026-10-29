#include <stdlib.h>

#include "game_loop.h"
#include "psx_gpu.h"
#include "collision.h"
#include "effect_update.h"
#include "game.h"
#include "map_render.h"
#include "camera.h"
#include "input_record.h"
#include "level_data.h"
#include "map.h"
#include "menu.h"
#include "mission.h"
#include "object.h"
#include "game_file.h"
#include "game_runtime.h"
#include "input.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "render.h"
#include "runtime_heap.h"
#include "spatial.h"
#include "sprite.h"
#include "text_renderer.h"

/* Original: DAT_800D3DD0 */
__declspec(dllexport) sint16 g_loading_image_idx;

/* Original: DAT_80082F71 */
const char g_loading_caption_spanish[] = "\x13"
                                         "CARGANDO";

/* Original: FUN_8008BE50. */
GDB_CALL const char *loading_image_next_path(void)
{
    static const char *const paths[3] = {"HQ\\TITLE.TIM", "HQ\\LOAD1.TIM", "HQ\\LOAD2.TIM"};
    g_loading_image_idx = (g_loading_image_idx + 1);
    if (g_loading_image_idx >= 3)
        g_loading_image_idx = 0;
    XASSERT(!(g_loading_image_idx < 0 || g_loading_image_idx >= 3));
    return paths[g_loading_image_idx];
}

/* Functions. */

/* 0x80088FE8..0x80089080.  This is the two-frame loading caption used by
 * the mission-launch path; a null argument selects the resident "LOADING"
 * string through pointer slot 0x800827C8. */
/* Original: FUN_80088FE8. */
GDB_CALL void loading_caption_show(const char *text)
{
    sint32 frame;

    for (frame = 0; frame < 2; frame++)
    {
        frame_begin();
        if (text == 0)
            text_render(g_game_text.entries[98], -0x2a, -6);
        else
            text_render(text, 0, -0x40);
        g_render_frames[0].drawenv.dtd = 1;
        g_render_frames[1].drawenv.dtd = 1;
        scr_overlay_draw();
        end_frame_submit(0);
    }
}

/* Original: FUN_80089084. */
GDB_CALL void loading_image_show(const char *path, const char *text)
{
    void *image;
    sint32 image_size;
    frame_begin();
    end_frame_submit(1);
    image_size = game_file_size(path);
    image = runtime_heap_allocate_sector_aligned(image_size);
    game_file_read(path, image, image_size);
    tim_image_upload(image, image_size);
    if (text != 0)
        text_render(text, -0x2a, 0x38);
    ot_submit(&g_cur_render_frame->ot[0x480]);
    DrawSync(0);
    frame_begin();
    end_frame_submit(1);
    runtime_heap_defer_free(image);
    runtime_heap_sweep();
    SetDispMask(1);
}

/* Original: FUN_8008BC84. */
GDB_CALL void startup_warning_show(void)
{
    const char *path = "COMMON0\\WARNING.TIM";
    void *image;
    sint32 image_size;
    DISPENV *env = &g_render_frames[1].disp_env;
    SetDefDispEnv(env, 0, 0, 0x280, g_render_state.height);
    g_cur_render_frame = &g_render_frames[1];
    g_render_frames[0].disp_env.screen.y = g_render_state.disp_y;
    env->screen.y = g_render_state.disp_y;
    image_size = game_file_size(path);
    image = runtime_heap_allocate_sector_aligned(image_size);
    game_file_read(path, image, image_size);
    tim_image_upload(image, image_size);
    PutDispEnv(env);
    vertical_sync_wait(0);
    SetDispMask(1);
#ifndef XPORT_NATIVE
    sint32 remaining = 200;
    do
    {
        vertical_sync_wait(0);
        --remaining;
    } while (remaining > 0);
#else
    /* User-requested native startup policy: omit WARNING.TIM's fixed
     * 200-VSync hold (MIPS 8008BD28..8008BD48, about four PAL seconds).
     * Keep initialization/upload; language loading is still mandatory */
#endif
}

/* Literal game-side control flow of 0x8008B06C..0x8008B5E0.  SetSp and the
 * final frame pacing are native platform boundaries; all PAL state writes,
 * branch conditions and game-function ordering remain in this body. */
/* Original: FUN_8008B06C. */
void mission_game_loop(void)
{
    while (!xport_isquit())
    {
        MENU_ENTRY *menu = g_level_select_cheat_enabled ? g_level_pause_menu : g_pause_menu;
        sint32 menu_result;

        if (g_session.stage != 8 && g_session.stage != 6)
            g_map_render.far_z = g_camera.world_z + 0x34000;

        g_map_render.background_brightness = g_map_render.brightness = 0;
        if (g_session.attract_mode != 0)
        {
            g_map_render.background_brightness = 0x20 - secondary_random_range(4) * 8;
            g_map_render.brightness = g_map_render.background_brightness;
            ambient_camera_spr_create();
            ambient_speech_timer_update();
        }

        room_camera_limits_update();
        if (g_camera.prev_z != -1)
        {
            g_camera.prev_x = g_camera.world_x;
            g_camera.prev_y = g_camera.world_y;
            g_camera.prev_z = g_camera.world_z;
        }

        mission_objective_script_update(g_stage_env->mission_script);
        sint32 objective_idx = 1;
        while (objective_idx <= g_mission.primary_count && g_mission.primary[objective_idx] == 1)
            ++objective_idx;
        if (objective_idx == g_mission.primary_count + 1)
            mission_complete_begin();

        frame_begin();
        if (g_input_rec.mode == INPUT_RECORD_WRITE)
            input_rec_append(g_input.held);
        if (g_input_rec.mode == INPUT_RECORD_PLAYBACK)
        {
            if (g_input.held != 0)
            {
                return;
            }
            g_input.held = input_playback_next();
            g_input.pressed = g_input.held & ~g_input.prev;
        }

        weapon_selection_update();
        spatial_buckets_reset();
        g_next_frame_object = g_frame_objects;
        g_next_transient_prim = g_transient_prims;
        effects_update_visible();
        g_mission.enemy_updates = 0;
        object_list_update();
        g_map_render.background_brightness = g_map_render.brightness;
        object_cols_dispatch();
        spatial_buckets_build();

        if (g_player_muzzle_light.pending != 0)
            map_light_stamp_apply(g_player_world.x + g_player_muzzle_light.dir_x * 0x4000, g_player_world.z + g_player_muzzle_light.dir_z * 0x4000, 0, &g_player_muzzle_light_stamp);
        render_map_scene();
        g_player_muzzle_light.pending = 0;
        map_light_stamp_apply(g_player_world.x + g_player_muzzle_light.dir_x * 0x4000, g_player_world.z + g_player_muzzle_light.dir_z * 0x4000, 0x80, &g_player_muzzle_light_stamp);

        if (g_input_rec.mode == INPUT_RECORD_PLAYBACK && (g_render_state.frame & 0x10) != 0)
            text_render(g_input_playback_label, (-g_text_renderer.glyph_width * 4) / 2, 0x46);
        if (g_input_rec.mode == INPUT_RECORD_WRITE)
            text_render(integer_to_text(g_render_state.frame), 0, 0x5c);

        g_map_render.background_resource = g_stage_env->background_resource;
        if (g_map_render.background_disabled != 0)
            g_map_render.background_resource = 0;
        if (g_map_render.background_resource != 0)
            render_stage_background();
        if (g_session.attract_mode == 0)
            mission_hud_render();

        g_input.prev = g_input.held;
        g_map_render.far_z = g_map.depth * 0xc000;
        debug_input_update();
        if (frame_render_work_enabled() == 0)
        {
            spr_vram_release_deferred();
            runtime_heap_sweep();
            DrawSync(0);
            gpu_frame_end();
        }
        else
        {
            end_frame_submit(g_map_render.background_resource);
        }
        pause_menu_update();
        player_cheats_update();

        if ((g_input.pressed & PADselect) != 0)
        {
            if (g_input_rec.mode == INPUT_RECORD_PLAYBACK)
            {
                g_session.next_stage = 99;
                g_input.pressed = g_input.held = 0;
                return;
            }
            menu_result = menu_run(menu, 0, 0);
            if (menu_result == 0x41)
                mission_unlock_text_show(g_session.stage);
            if (menu_result == 0x42)
                return;
            if (menu_result == 0x43)
                g_session.next_stage = g_session.stage + 1;
            if (menu_result == 0x4c)
            {
                g_session.next_stage = menu[g_menu_selection_idx].choice + 1;
            }
        }
        if (g_session.next_stage != 0)
            return;
        if (g_surface_impact_cooldown != 0)
            --g_surface_impact_cooldown;
        disp_shake_update();
        mission_required_items_check();
    }
}
