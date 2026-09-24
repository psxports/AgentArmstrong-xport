#include "psx_gpu.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "input_record.h"
#include "level_data.h"
#include "map.h"
#include "memory_card_platform.h"
#include "mission.h"
#include "object.h"
#include "original_file.h"
#include "game_runtime.h"
#include "input.h"
#include "player.h"
#include "random.h"
#include "render.h"
#include "runtime_heap.h"
#include "spatial.h"
#include "sprite.h"
#include "stubs.h"
#include "text_renderer.h"

/* Macros. */
#pragma optimize("", off)

#pragma optimize("", on)

/* Functions. */
__declspec(dllexport) DISPENV *g_psx_current_display_env;

/* 0x800A4684..0x800A4780.  Mission 0 takes the final branch immediately;
 * the remaining body is retained because it is part of the same original
 * function and is used by stages 2 and 13. */
/* Original: FUN_800A4684. */
void mission_required_items_check(void)
{
    sint32 count;
    void *inventory;
    MISSION_TEXT_MESSAGE *message;

    if (g_stage_index != 0x0d && g_stage_index != 2)
        return;
    if (g_required_mission_item_count == -1)
        return;

    count = (sint16)effect_count(0x0c);
    inventory = player_inventory_find(0x0c, g_player);
    if (inventory != 0)
        count += (uint16) * (uint16 *)inventory;
    count += (sint16)object_count_by_type(0x0c);
    count += (sint16)object_count_by_type(4);
    if (count >= g_required_mission_item_count)
        return;

    g_required_mission_item_count = -1;
    message = mission_text_message_create(player_assets_executable_text_pointer(0x800827d4u));
    message->steady = 0;
    message = mission_text_message_create(player_assets_executable_text_pointer(0x800827d8u));
    message->steady = 0;
    message = mission_text_message_create(player_assets_executable_text_pointer(0x800827dcu));
    message->steady = 0;
    message = mission_text_message_create(player_assets_executable_text_pointer(0x800827e0u));
    message->steady = 0;
    player_death_begin();
}

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
            text_render(player_assets_executable_text_pointer(0x800827c8u), -0x2a, -6);
        else
            text_render(text, 0, -0x40);
        g_render_frame_buffer_0.drawenv.dtd = 1;
        g_render_frame_buffer_1.drawenv.dtd = 1;
        screen_overlay_draw();
        end_frame_submit(0);
    }
}

/* Original: FUN_80089084. */
GDB_CALL void loading_image_show(const char *path, const char *text)
{
    frame_begin();
    end_frame_submit(1);
    g_loading_image_buffer = runtime_heap_allocate_sector_aligned(game_file_size(path));
    game_file_read(path, g_loading_image_buffer);
    tim_image_upload(g_loading_image_buffer);
    if (text != 0)
        text_render(text, -0x2a, 0x38);
    ot_submit(&g_current_render_frame->ot[0x480]);
    gpu_draw_sync(0);
    frame_begin();
    end_frame_submit(1);
    runtime_heap_defer_free(g_loading_image_buffer);
    runtime_heap_sweep();
    display_mask_set(1);
}

/* Original: FUN_800B230C. */
GDB_CALL DISPENV *display_env_initialize(DISPENV *env, sint32 x, sint32 y, sint32 width, sint32 height)
{
    env->disp.x = (sint16)x;
    env->disp.y = (sint16)y;
    env->disp.w = (sint16)width;
    env->disp.h = (sint16)height;
    env->screen.x = env->screen.y = 0;
    env->screen.w = env->screen.h = 0;
    env->isinter = env->isrgb24 = 0;
    env->pad0 = env->pad1 = 0;
    return env;
}

/* Original: FUN_800B39C0. */
GDB_CALL DISPENV *display_env_apply(DISPENV *env)
{
    g_psx_current_display_env = env;
    gpu_set_display(env->disp.x, env->disp.y, env->disp.w, env->disp.h);
    return env;
}

/* Original: FUN_8008BC84. */
GDB_CALL void startup_warning_show(void)
{
    const char *path = "COMMON0\\WARNING.TIM";
    DISPENV *env = (DISPENV *)&g_render_frame_buffer_1.display_env;
    display_env_initialize(env, 0, 0, 0x280, g_screen_height);
    g_current_render_frame = &g_render_frame_buffer_1;
    g_render_frame_buffer_0.display_env.screen.y = g_display_screen_y;
    env->screen.y = g_display_screen_y;
    g_loading_image_buffer = runtime_heap_allocate_sector_aligned(game_file_size(path));
    game_file_read(path, g_loading_image_buffer);
    tim_image_upload(g_loading_image_buffer);
    display_env_apply(env);
    vertical_sync_wait(0);
    display_mask_set(1);
#ifndef XPORT_NATIVE
    g_shared_scratch_value = 200;
    do
    {
        vertical_sync_wait(0);
        --g_shared_scratch_value;
    } while (g_shared_scratch_value > 0);
#else
    /* User-requested native startup policy: omit WARNING.TIM's fixed
     * 200-VSync hold (MIPS 8008BD28..8008BD48, about four PAL seconds).
     * Keep initialization/upload and the original loop's final counter
     * value; language loading is still mandatory. This is not an MIPS fix. */
    g_shared_scratch_value = 0;
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
        void *menu = (void *)player_assets_executable_address(g_level_select_cheat_enabled ? 0x800c7f04u : 0x800c7e94u);
        sint32 menu_result;

        if (g_stage_index != 8 && g_stage_index != 6)
            g_scene_far_z = g_camera_world_z + 0x34000;

        g_background_brightness_bias = g_scene_brightness_bias = 0;
        if (g_attract_mode != 0)
        {
            g_background_brightness_bias = 0x20 - secondary_random_range(4) * 8;
            g_scene_brightness_bias = g_background_brightness_bias;
            ambient_camera_sprite_create();
            ambient_speech_timer_update();
        }

        /* Original FUN_800b0130 changes MIPS sp to 0x1F800400. Windows keeps
         * the normal thread stack; retain only a zero context token. */
        g_mips_frame_stack_pointer = 0;
        room_camera_limits_update();
        if (g_previous_camera_world_z != -1)
        {
            g_previous_camera_world_x = g_camera_world_x;
            g_previous_camera_world_y = g_camera_world_y;
            g_previous_camera_world_z = g_camera_world_z;
        }

        mission_objective_script_update(g_mission_script);
        g_shared_scratch_value = 1;
        while (g_shared_scratch_value <= g_primary_objective_count && g_primary_objective_states[g_shared_scratch_value] == 1)
            ++g_shared_scratch_value;
        if (g_shared_scratch_value == g_primary_objective_count + 1)
            mission_complete_begin();

        frame_begin();
        if (g_input_recording_mode == 1)
            input_record_append(g_held_buttons);
        if (g_input_recording_mode == 2)
        {
            if (g_held_buttons != 0)
            {
                return;
            }
            g_held_buttons = input_playback_next();
            g_pressed_buttons = g_held_buttons & ~g_previous_held_buttons;
        }

        selected_weapon_sync();
        weapon_selection_update();
        spatial_buckets_reset();
        g_next_frame_object = g_frame_objects;
        g_transient_prim_cursor = g_transient_prim_buffer;
        effects_update_visible();
        g_mission_enemy_update_count = 0;
        object_list_update();
        g_background_brightness_bias = g_scene_brightness_bias;
        object_collisions_dispatch();
        spatial_buckets_build();

        if (g_muzzle_light_pending != 0)
            map_light_stamp_apply(g_player_world_x + g_muzzle_light_x_direction * 0x4000, g_player_world_z + g_muzzle_light_z_direction * 0x4000, 0, g_muzzle_flash_light_stamp);
        render_map_scene();
        g_muzzle_light_pending = 0;
        map_light_stamp_apply(g_player_world_x + g_muzzle_light_x_direction * 0x4000, g_player_world_z + g_muzzle_light_z_direction * 0x4000, 0x80, g_muzzle_flash_light_stamp);

        if (g_input_recording_mode == 2 && (g_frame_counter & 0x10) != 0)
            text_render(g_input_playback_label, (-g_font_glyph_width * 4) / 2, 0x46);
        if (g_input_recording_mode == 1)
            text_render(integer_to_text(g_frame_counter), 0, 0x5c);

        g_stage_background_resource = (sint16)g_stage_env->background_resource;
        if (g_stage_background_disabled != 0)
            g_stage_background_resource = 0;
        if (g_stage_background_resource != 0)
            render_stage_background();
        if (g_attract_mode == 0)
            mission_hud_render();

        g_previous_held_buttons = g_held_buttons;
        g_scene_far_z = g_map_depth_cells * 0xc000;
        debug_input_update();
        if (frame_render_work_enabled() == 0)
        {
            sprite_vram_release_deferred();
            runtime_heap_sweep();
            DrawSync(0);
        }
        else
        {
            end_frame_submit(g_stage_background_resource);
        }
        pause_menu_update();
        player_cheats_update();

        if ((g_pressed_buttons & 0x100) != 0)
        {
            if (g_input_recording_mode == 2)
            {
                g_next_stage_index = 99;
                g_pressed_buttons = g_held_buttons = 0;
                return;
            }
            menu_result = menu_run(menu, 0, 0);
            g_shared_scratch_value = menu_result;
            if (menu_result == 0x41)
                mission_unlock_text_show(g_stage_index);
            if (menu_result == 0x42)
                return;
            if (menu_result == 0x43)
                g_next_stage_index = g_stage_index + 1;
            if (menu_result == 0x4c)
            {
                const sint32 *level = (const sint32 *)player_assets_executable_address(0x800c7f0cu + (uint32)g_menu_selection_index * 0x10u);
                g_next_stage_index = *level + 1;
            }
        }
        if (g_next_stage_index != 0)
            return;
        if (g_surface_impact_cooldown != 0)
            --g_surface_impact_cooldown;
        display_shake_update();
        mission_required_items_check();
    }
}
