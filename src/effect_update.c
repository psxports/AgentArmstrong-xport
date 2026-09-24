#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "airship.h"
#include "game_sound.h"
#include "camera.h"
#include "effect_update.h"
#include "global.h"
#include "gunship.h"
#include "gyro.h"
#include "hq.h"
#include "industry.h"
#include "map.h"
#include "mini_gyro.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "random.h"
#include "resque_tanya.h"
#include "runtime_heap.h"
#include "scuba_player.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "stubs.h"
#include "tank.h"
#include "truck.h"
#include "v2.h"
#include "vram_alloc.h"

/* Types. */
/* FUN_800A878C and FUN_800A8838: visibility-filtered effect updates. */

typedef EFFECT_UPDATE_CALLBACK EffectUpdateCallback;

typedef struct
{
    uint8 field_00[0x2C];  /* +0x00 */
    EFFECT *source_effect; /* +0x2C */
    sint32 frame;          /* +0x30 */
    sint32 destination;    /* +0x34 */
    sint32 reverse_exit;   /* +0x38 */
    uint8 field_3c[0x40];  /* +0x3C */
} INTER_ROOM_TRANSITION;

typedef struct
{
    uint8 field_00[0x20];  /* +0x00 */
    sint32 x;              /* +0x20 */
    sint32 y;              /* +0x24 */
    sint32 z;              /* +0x28 */
    uint8 field_2c[0x50];  /* +0x2C */
    sint16 frame;          /* +0x7C */
    uint16 field_7e;       /* +0x7E */
    EFFECT *target_effect; /* +0x80 */
} DOOR_EFFECT_TRANSITION;

#if defined(AP_32BIT)
typedef char InterRoomTransition_size_7c[sizeof(INTER_ROOM_TRANSITION) == 0x7c ? 1 : -1];
typedef char InterRoomTransition_source_at_2c[offsetof(INTER_ROOM_TRANSITION, source_effect) == 0x2c ? 1 : -1];
typedef char InterRoomTransition_frame_at_30[offsetof(INTER_ROOM_TRANSITION, frame) == 0x30 ? 1 : -1];
typedef char DoorEffectTransition_size_84[sizeof(DOOR_EFFECT_TRANSITION) == 0x84 ? 1 : -1];
typedef char DoorEffectTransition_frame_at_7c[offsetof(DOOR_EFFECT_TRANSITION, frame) == 0x7c ? 1 : -1];
typedef char DoorEffectTransition_target_at_80[offsetof(DOOR_EFFECT_TRANSITION, target_effect) == 0x80 ? 1 : -1];
#endif

/* Variables. */
/* 0x800CAFA8, 88 records of 0x10 bytes. The lookup in FUN_800A8460 scans
 * all records, so duplicate type 0x2C intentionally resolves to slot 0x28. */
static const uint16 effect_handler_types[0x58] = {0x01, 0x13, 0x14, 0x15, 0x16, 0x1b, 0x0b, 0x0c, 0x1d, 0x1e, 0x20, 0x21, 0x22, 0x23, 0x28, 0x27, 0x29, 0x2c, 0x2a, 0x30, 0x32, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3b, 0x3d, 0x3e, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x4a, 0x4c, 0x4e, 0x4f, 0x2c, 0x50, 0x59, 0x5a, 0x5b, 0x5e, 0x60, 0x49, 0x62, 0x65, 0x68, 0x67, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x72, 0x73, 0x75, 0x77, 0x74, 0x78, 0x71, 0x7a, 0x7b, 0x66, 0x7c, 0x7d, 0x7f, 0x80, 0x81, 0x8b, 0x8c, 0x89, 0x88, 0x8a, 0x8d, 0x8e, 0x8f, 0x1f, 0x92, 0x96, 0x07, 0x00};

/* 800A8620 indexes 800CB518 by stage*4; 800A8658/8668 read two signed
 * halfwords. Exact 33-stage table: 49->07 belongs to stage 13, not stage 2.
 * The original deliberately retains the effect's existing handler slot. */
static const sint16 stage_effect_remap[33][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x49, 7}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0x21, 0x8e}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};

/* Functions. */
/* Exact PAL 0x800AAB84..0x800AABB8: type-0x8B music trigger. */
/* Original: FUN_800AAB84. */
void mission_music_trigger_update(EFFECT *effect)
{
    sint32 music = (sint16)effect->values[0];
    if (g_current_music_track != music)
        music_track_select(music, 1);
}

/* Exact PAL 0x800A8348..0x800A8460: type-0x7A animated jungle effect. */
/* Original: FUN_800A8348. */
void jungle_animated_effect_update(EFFECT *effect)
{
    POLY_FT4 *prim = (POLY_FT4 *)(intptr)g_transient_prim_cursor;
    sint32 frame, old_frame;
    g_transient_prim_cursor += 0x28;
    setPolyFT4(prim);
    setSemiTrans(prim, 1);
    frame = (sint16)effect->values[0];
    if (frame == 0)
    {
        frame = random_range(0x0b) + 1;
        effect->values[0] = (uint16)frame;
    }
    old_frame = frame - 1;
    if ((sint16)effect->values[1] == 4)
    {
        effect->values[0] = (uint16)(frame + 1);
        effect->values[1] = 0;
    }
    effect->values[1] = (uint16)(effect->values[1] + 1);
    if ((sint16)effect->values[0] == 13)
        effect->values[0] = 1;
    g_next_frame_object->light_delta = -0x20;
    g_next_frame_object->force_light = 1;
    render_world_sprite((uint32)(0x2e800 + old_frame), effect->x, effect->y + 0x2000, effect->z, 0, 0x1000, 0x1000, prim, 0, 2, 0, 0);
#ifdef XPORT_NATIVE
    if (getenv("OA_STAGE22_TRACE"))
    {
        FILE *file = fopen("../status/river-action-native.log", "a");
        if (file)
        {
            fprintf(file, "RIVER_NATIVE_7A value=%d tick=%d frame_id=%d xyz=%d,%d,%d\n", (sint16)effect->values[0], (sint16)effect->values[1], 0x2e800 + old_frame, effect->x, effect->y, effect->z);
            fclose(file);
        }
    }
#endif
}

/* Exact FUN_800A8250, 0x800A8250..0x800A8260: type-0x59 trigger callback. */
void mission_background_disable_trigger(void)
{
    g_stage_background_disabled = 1;
}

/* Original: FUN_800A8974. */
GDB_CALL EFFECT *effect_find_next(sint32 type, EFFECT *after)
{
    EFFECT *current = after != NULL ? after : g_effects;
    EFFECT *end = g_effects + g_effect_count;

    while (current < end)
    {
        if (current->type == type)
            return current;
        ++current;
    }
    return NULL;
}

/* Exact FUN_800A9300, the type-0x46 inter-room transition timeline. */
void inter_room_transition_update(INTER_ROOM_TRANSITION *transition)
{
    PLAYER *player = g_player;
    sint32 old_frame = transition->frame;
    sint32 frame = old_frame + 1;
    transition->frame = frame;
    if ((uint16)old_frame < 0x2a)
        g_scene_brightness_bias = -(sint16)(frame << 2);
    if ((uint16)(old_frame - 0x2a) < 0x29)
        g_scene_brightness_bias = ((sint16)frame - 0x2a) * 3 - 0x80;
    if ((sint16)frame == 0x2a)
    {
        EFFECT *source = transition->source_effect;
        EFFECT *target = 0;
        sint16 destination = (sint16)transition->destination;
        for (;;)
        {
            target = effect_find_next(0x46, target);
            if (target == 0)
                fatal_error("NO DEST DOOR");
            if ((sint16)target->values[0] == destination)
                break;
            ++target;
        }
        if ((sint16)target->values[1] == 999)
            target->values[3] = source->values[0];
        player->x = target->x;
        player->y = target->y;
        if ((sint16)target->values[2] != 0 && (sint16)target->values[2] != 2)
            player->y += 0x2000;
        player->z = target->z;
        if (transition->reverse_exit != 0)
            player->z += 0x1c00;
        g_action_transition_direction = (sint16)target->values[2];
        if (g_action_transition_direction == 0)
            player->y -= 0xa00;
        if (g_action_transition_direction == 1)
            player->x += 0x4000;
        if (g_action_transition_direction == 2)
            player->y += 0xa00;
        if (g_action_transition_direction == 3)
            player->x -= 0x4000;
        if (g_action_transition_direction == 4)
            player->z += 0x4000;
        if (g_action_transition_direction == 5)
            player->z -= 0x4000;
        g_player_world_x = g_previous_player_world_x = player->x;
        g_player_world_y = player->y;
        g_player_world_z = player->z;
        g_camera_limit_near = -1;
        g_room_player_near_z = 0;
        g_previous_camera_world_z = -1;
        g_stage_background_disabled = 1;
        camera_update(g_player_world_x, g_player_world_y, g_player_world_z, player->direction, 0);
        g_previous_camera_world_z = -1;
    }
    if ((sint16)frame == 0x65)
    {
        player->update = transition->reverse_exit ? player_ledge_control_update : player_control_update;
        player->draw_mode = 2;
        object_destroy(transition);
    }
}

/* Original: FUN_800A8BD0. */
void door_transition_player_update(PLAYER *player)
{
    if (g_room_player_near_z != 0 && player->z < g_room_player_near_z)
        player->z = g_room_player_near_z;
    g_player_world_x = player->x;
    g_player_world_y = player->y;
    g_player_world_z = player->z;
    camera_update(g_player_world_x, g_player_world_y, g_player_world_z, player->previous_direction, player->unchanged_direction_frames);
    g_previous_player_world_x = g_player_world_x;
    g_previous_player_world_y = g_player_world_y;
    g_previous_player_world_z = g_player_world_z;
}

/* Exact FUN_800A9650, type-0x46 proximity/transition trigger. */
void inter_room_transition_trigger_update(EFFECT *effect)
{
    PLAYER *player = g_player;
    sint32 vertical = ((sint16)effect->values[2] == 0 || (sint16)effect->values[2] == 2) ? 0x600 : 0x2800;
    sint16 destination;
    INTER_ROOM_TRANSITION *transition;
    if (g_player_world_x < effect->x - 0x2000 || g_player_world_x >= effect->x + 0x2000 || g_player_world_z < effect->z - 0x2000 || g_player_world_z >= effect->z + 0x2000 || g_player_world_y <= effect->y - vertical || g_player_world_y >= effect->y + vertical)
        return;
    if (player->update != player_control_update && player->update != player_ledge_control_update)
        return;
    sound_voice_stop(&player->spatial_sound_handle);
    if (player->update == (FUNC_COLLISION_UPDATE)door_transition_player_update)
        return;
    destination = (sint16)effect->values[1];
    if (destination == 999)
        destination = (sint16)effect->values[3];
    transition = (INTER_ROOM_TRANSITION *)object_create(sizeof(*transition), (FUNC_COLLISION_UPDATE)inter_room_transition_update);
    transition->source_effect = effect;
    transition->destination = destination;
    if (player->update == player_ledge_control_update)
        transition->reverse_exit = 1;
    player->update = (FUNC_COLLISION_UPDATE)door_transition_player_update;
    player->render_flags = 0;
    player->draw_mode = 0;
    player->animation.next_address = 0;
    player->animation.start_address = 0;
}

/* Original: FUN_800A8F2C. */
static SPRITE *door_transition_sprite_create(sint32 x, sint32 y, sint32 z, const sint32 *animation)
{
    VRAM_SPRITE *descriptor = (VRAM_SPRITE *)runtime_heap_allocate(sizeof(*descriptor));
    SPRITE *actor = world_sprite_create(x, y, z, animation);
    actor->owns_resource = 1;
    sprite_vram_allocate(descriptor, 0x38, 0x50, 1);
    actor->collision.frame_descriptor = descriptor;
    descriptor->clut = g_player_sprite_clut;
    actor->lifetime = 0x3c;
    actor->field_9c = 1;
    return actor;
}

/* Original: FUN_800A8CF0. */
static void door_transition_update(DOOR_EFFECT_TRANSITION *transition)
{
    PLAYER *player = g_player;
    sint16 frame = transition->frame;
    if (frame == 0)
    {
        SPRITE *actor;
        player->update = (FUNC_COLLISION_UPDATE)door_transition_player_update;
        player->render_flags = 0;
        player->draw_mode = 0;
        player->animation.next_address = 0;
        actor = door_transition_sprite_create(transition->x, transition->y, transition->z, (const sint32 *)player_assets_executable_address(0x800c9058u));
        actor->field_aa = -4;
        actor->scale_step = -0x20;
        actor->field_a8 = 0;
        actor->velocity_y = -0xa0;
        actor->scale_x = (uint16)g_default_model_scale;
    }
    if (frame == 0x3a)
    {
        EFFECT *target = transition->target_effect;
        SPRITE *actor;
        player->x = target->x;
        player->y = target->y + 0x2000;
        player->z = target->z + 0x1c00;
        player->direction = 4;
        g_player_world_x = g_previous_player_world_x = target->x;
        g_player_world_y = target->y + 0x2000;
        g_player_world_z = target->z + 0x1c00;
        g_previous_camera_world_z = -1;
        actor = door_transition_sprite_create(target->x, target->y - 0x580, target->z + 0x1e00, (const sint32 *)player_assets_executable_address(0x800c9040u));
        actor->field_a8 = -0xf0;
        actor->scale_step = 0x20;
        actor->field_aa = 4;
        actor->velocity_y = 0xa0;
        actor->scale_x = (uint16)(g_default_model_scale - 0x780);
    }
    if (frame == 0x75)
    {
        player->update = player_control_update;
        player->draw_mode = 2;
        player->animation.next_address = 0;
        player->animation.start_address = 0;
        player->animation.reserved = 0;
        player->weapon_animation_active = 0;
        player->animation.flags = 0x400;
        object_destroy(transition);
    }
    if ((uint16)(frame - 0x11) < 0x2a)
        g_scene_brightness_bias = -frame * 3;
    if ((uint16)(frame - 0x3b) < 0x29)
        g_scene_brightness_bias = (frame - 0x3a) * 3 - 0x80;
    transition->frame = (sint16)(frame + 1);
}

/* Original: FUN_800A8C80. */
static void door_transition_create(sint32 x, sint32 y, sint32 z, EFFECT *target)
{
    DOOR_EFFECT_TRANSITION *transition = (DOOR_EFFECT_TRANSITION *)object_create(sizeof(*transition), (FUNC_COLLISION_UPDATE)door_transition_update);
    transition->x = x;
    transition->y = y;
    transition->z = z;
    transition->target_effect = target;
    door_transition_update(transition);
}

/* Original: FUN_800A8A28. */
void door_trigger_update(void *raw_effect)
{
    EFFECT *source = (EFFECT *)raw_effect;
    PLAYER *player = g_player;
    EFFECT *scan = 0;
    sint16 destination;
    /* 800A8A48 branches out when the player is already controlled by the
     * door callback, then accepts only FUN_8009AADC.  Treating both callbacks
     * as admissible spawned a fresh transition actor every held-input frame. */
    if (player->update == (FUNC_COLLISION_UPDATE)door_transition_player_update)
        return;
    if (player->update != player_control_update)
        return;
    if ((g_held_buttons & 0x1000) == 0 || player->airborne != 0)
        return;
    if (g_player_world_x <= source->x - 0x1000 || g_player_world_x >= source->x + 0x1000)
        return;
    if (g_player_world_z < source->z + 0x1c00 || g_player_world_z >= source->z + 0x2000)
        return;
    if (g_player_world_y <= source->y || g_player_world_y >= source->y + 0x2800)
        return;
    sound_voice_stop(&player->spatial_sound_handle);
    destination = (sint16)source->values[1] == 999 ? (sint16)source->values[3] : (sint16)source->values[1];
    for (;;)
    {
        scan = effect_find_next(0x41, scan);
        if (!scan)
            fatal_error("NO DEST DOOR");
        if ((sint16)scan->values[0] == destination)
            break;
        ++scan;
    }
    if ((sint16)scan->values[1] == 999)
        scan->values[3] = source->values[0];
    door_transition_create(source->x, g_player_world_y, g_player_world_z, scan);
}

static void initialize_mission_effect_handlers(void)
{
    g_effect_handlers[1].update = player_action_effect_create;
    g_effect_handlers[2].update = player_action_effect_create;
    g_effect_handlers[3].update = player_action_effect_create;
    g_effect_handlers[4].update = mortar_create;
    g_effect_handlers[6].update = player_action_effect_create;
    g_effect_handlers[7].update = player_action_effect_create;
    g_effect_handlers[8].update = (EffectUpdateCallback)mission_enemy_create;
    g_effect_handlers[0x0b].update = (EffectUpdateCallback)mission_enemy_create;
    if (g_stage_index == 1)
        g_effect_handlers[0x0c].update = (EffectUpdateCallback)truck_create;
    if (g_stage_index == 8)
        g_effect_handlers[0x14].update = (EffectUpdateCallback)gyro_boss_create;
    if (g_stage_index == 10)
        g_effect_handlers[0x44].update = (EffectUpdateCallback)gunship_create;
    if (g_stage_index == 12 || g_stage_index == 14 || g_stage_index == 25)
        g_effect_handlers[0x29].update = (EffectUpdateCallback)v2_rocket_create;
    g_effect_handlers[0x0d].update = (EffectUpdateCallback)mission_enemy_create;
    g_effect_handlers[0x0e].update = building_create;
    g_effect_handlers[0x0f].update = (EffectUpdateCallback)mission_enemy_create;
    g_effect_handlers[0x12].update = player_action_effect_create;
    g_effect_handlers[0x13].update = lift_create;
    g_effect_handlers[0x18].update = radar_vehicle_create;
    g_effect_handlers[0x19].update = falling_hazard_create;
    g_effect_handlers[0x15].update = tanya_gun_emplacement_create;
    g_effect_handlers[0x16].update = tank_create;
    g_effect_handlers[0x28].update = tanya_create;
    g_effect_handlers[0x20].update = (EffectUpdateCallback)mission_enemy_create;
    g_effect_handlers[0x1d].update = (EffectUpdateCallback)door_trigger_update;
    g_effect_handlers[0x21].update = drop_range_create;
    g_effect_handlers[0x22].update = inter_room_transition_trigger_update;
    g_effect_handlers[0x23].update = jetpack_enemy_create;
    g_effect_handlers[0x1c].update = airship_switch_create;
    g_effect_handlers[0x25].update = flare_area_create;
    g_effect_handlers[0x26].update = horizontal_camera_limit_create;
    g_effect_handlers[0x27].update = horizontal_camera_limit_create;
    g_effect_handlers[0x2f].update = (EffectUpdateCallback)mission_enemy_create;
    g_effect_handlers[0x30].update = mini_gyro_create;
    g_effect_handlers[0x31].update = (EffectUpdateCallback)scuba_submarine_create;
    g_effect_handlers[0x33].update = (EffectUpdateCallback)scuba_drifting_hazard_create;
    g_effect_handlers[0x2a].update = (EffectUpdateCallback)mission_background_disable_trigger;
    /* Resident 800CAFA8 + 2Ch*10h: type 5B, callback 8009FFD0.
     * HQ EFFECTS[5] is the signal-2 passage, using this shared lift code. */
    g_effect_handlers[0x2c].update = lift_create;
    g_effect_handlers[0x2d].update = building_collapse_create;
    g_effect_handlers[0x2e].update = flare_area_create;
    g_effect_handlers[0x36].update = vertical_camera_limit_create;
    g_effect_handlers[0x35].update = v2_radar_marker_create;
    g_effect_handlers[0x37].update = vertical_camera_limit_create;
    g_effect_handlers[0x38].update = (EffectUpdateCallback)scuba_tug_target_create;
    g_effect_handlers[0x39].update = (EffectUpdateCallback)scuba_diver_enemy_create;
    g_effect_handlers[0x34].update = (EffectUpdateCallback)scuba_mine_create;
    g_effect_handlers[0x3a].update = scuba_noop_effect_update;
    g_effect_handlers[0x3b].update = scuba_bubble_emitter_update;
    g_effect_handlers[0x3c].update = camera_depth_limit_set;
    g_effect_handlers[0x3d].update = (EffectUpdateCallback)scuba_ship_target_create;
    g_effect_handlers[0x3e].update = mini_gyro_create;
    g_effect_handlers[0x43].update = jungle_animated_effect_update;
    if (g_stage_index >= 27)
        g_effect_handlers[0x47].update = bio_goo_create;
    g_effect_handlers[0x52].update = tower_surface_trigger_update;
    if (g_stage_index >= 27)
        g_effect_handlers[0x48].update = industry_gun_controller_create;
    if (g_stage_index >= 27)
        g_effect_handlers[0x49].update = robot_press_create;
    g_effect_handlers[0x4a].update = (EffectUpdateCallback)airship_create;
    g_effect_handlers[0x4b].update = mission_music_trigger_update;
    g_effect_handlers[0x4c].update = player_action_effect_create;
    g_effect_handlers[0x4d].update = player_action_effect_create;
    g_effect_handlers[0x4e].update = player_action_effect_create;
    g_effect_handlers[0x4f].update = player_action_effect_create;
    /* Original resident record 800CAFA8 + 50h*10h has type 8D and
     * callback 800FDAD0 at 800CB4B4. That address belongs to HQ.BIN. */
    g_effect_handlers[0x50].update = g_stage_index == 0x0b ? hq_portrait_trigger_update : 0;
    g_effect_handlers[0x53].update = player_action_effect_create;
}

/* Direct translation of 0x800A8460..0x800A85E8. */
/* Original: FUN_800A8460. */
void effects_initialize(void)
{
    const EFFECT_DATA *source = (const EFFECT_DATA *)g_effect_data;
    EFFECT *target;
    sint32 count = 0;
    sint32 index;
    sint32 depth_origin;

    while (source[count].x != 0)
        ++count;
    g_effect_count = count;
    target = (EFFECT *)runtime_heap_allocate(count * 0x24);
    g_effects = target;
    depth_origin = g_map_depth_cells * 3 * 0x40 - g_map_depth_subcells;
    initialize_mission_effect_handlers();
    for (index = 0; index < count; index++)
    {
        sint32 value_index;
        sint32 handler;
        target[index].x = (sint32)(uint16)source[index].x << 8;
        target[index].y = (sint32)source[index].y << 8;
        target[index].z = (depth_origin + (sint32)source[index].z) << 8;
        target[index].type = source[index].type;
        for (handler = 0; handler < 0x58; handler++)
            if (effect_handler_types[handler] == (uint16)target[index].type)
                target[index].handler = (sint16)handler;
        for (value_index = 0; value_index < 7; value_index++)
            target[index].values[value_index] = source[index].values[value_index];
    }
}

/* Original: FUN_800A8838. */
sint32 effect_is_visible(EFFECT *effect)
{
    sint32 relative_depth = effect->z - g_camera_world_z;
    sint32 screen_x;
    sint32 screen_y;

    if (relative_depth < 0x4000)
        return 0;
    screen_x = ((effect->x - g_camera_world_x) * 0x140) / relative_depth;
    if ((uint32)(screen_x + 0x100) >= 0x201)
        return 0;
    screen_y = ((effect->y - g_camera_world_y) * 0x163) / relative_depth;
    if (screen_y < -g_vertical_cull_extent || screen_y > g_vertical_cull_extent + 0x50)
        return 0;
    if (effect->z > g_camera_world_z + g_effect_visibility_depth_cells * 0x100)
        return 0;
    return effect->z <= g_scene_far_z;
}

/* Original: FUN_800A878C. */
void effects_update_visible(void)
{
    sint32 i;
#ifdef XPORT_NATIVE
    {
        static sint32 airfield_trace_frame;
        const char *trace = getenv("OA_AIRFIELD_TRACE");
        if (trace && *trace && g_stage_index == 17 && g_effect_count == 218 && airfield_trace_frame < 120)
        {
            EFFECT *focus = &g_effects[9];
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y;
            g_camera_world_z = focus->z - 0x10000;
            g_scene_far_z = 0x7fffffff;
            ++airfield_trace_frame;
        }
    }
    {
        static sint32 tug_trace_frame;
        const char *trace = getenv("OA_TUG_TRACE");
        if (trace && *trace && g_stage_index == 18 && g_effect_count == 246 && tug_trace_frame < 120)
        {
            EFFECT *focus = &g_effects[17];
            FILE *file;
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y;
            g_camera_world_z = focus->z - 0x4000;
            g_scene_far_z = 0x7fffffff;
            if (tug_trace_frame == 0)
            {
                file = fopen("../status/tug-o-war-native.log", "a");
                if (file)
                {
                    fprintf(file, "TUG_NATIVE_READY stage=%d effects=%d index=17 source_type_post=%u handler=%u xyz=%d,%d,%d\n", g_stage_index, g_effect_count, focus->type, focus->handler, focus->x, focus->y, focus->z);
                    fclose(file);
                }
            }
            ++tug_trace_frame;
        }
    }
    {
        static sint32 stage19_trace_frame;
        const char *trace = getenv("OA_STAGE19_TRACE");
        if (trace && *trace && g_stage_index == 19 && g_effect_count == 116 && stage19_trace_frame < 120)
        {
            EFFECT *focus = &g_effects[17];
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y;
            g_camera_world_z = focus->z - 0x4000;
            g_scene_far_z = 0x7fffffff;
            ++stage19_trace_frame;
        }
    }
    {
        static sint32 stage20_trace_frame;
        const char *trace = getenv("OA_STAGE20_TRACE");
        if (trace && *trace && g_stage_index == 20 && g_effect_count == 272 && stage20_trace_frame < 120)
        {
            EFFECT *focus = &g_effects[24];
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y + 0x2000;
            g_camera_world_z = focus->z - 0x2000;
            g_scene_far_z = 0x7fffffff;
            /* Diagnostic-only counterpart of the explicit PAL handler-entry trace. */
            if (stage20_trace_frame == 0 && focus->type == 0x73)
                scuba_ship_target_create(focus);
            ++stage20_trace_frame;
        }
    }
    {
        static sint32 stage21_trace_frame;
        const char *trace = getenv("OA_STAGE21_TRACE");
        if (trace && *trace && g_stage_index == 21 && g_effect_count == 183 && stage21_trace_frame < 120)
        {
            EFFECT *focus = &g_effects[48];
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y - 0x2c00;
            g_camera_world_z = focus->z - 0x4000;
            g_scene_far_z = 0x7fffffff;
            if (stage21_trace_frame == 0 && focus->type == 0x75)
                mini_gyro_create(focus);
            ++stage21_trace_frame;
        }
    }
    {
        static sint32 stage22_trace_frame;
        const char *trace = getenv("OA_STAGE22_TRACE");
        if (trace && *trace && g_stage_index == 22 && g_effect_count == 211 && stage22_trace_frame == 0)
        {
            EFFECT *focus = &g_effects[177];
            FILE *file = fopen("../status/river-action-native.log", "w");
            if (file)
            {
                fprintf(file, "RIVER_NATIVE_READY stage=22 effects=%d index=177 type=%u handler=%u xyz=%d,%d,%d forced_handler_entry=1\n", g_effect_count, focus->type, focus->handler, focus->x, focus->y, focus->z);
                fclose(file);
            }
            focus->values[0] = 11;
            focus->values[1] = 4;
            jungle_animated_effect_update(focus);
            ++stage22_trace_frame;
        }
    }
    {
        static sint32 stage23_trace_frame;
        const char *trace = getenv("OA_STAGE23_TRACE");
        if (trace && *trace && g_stage_index == 23 && g_effect_count == 244 && stage23_trace_frame < 120)
        {
            EFFECT *focus = &g_effects[168];
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y - 0xe00;
            g_camera_world_z = focus->z - 0x4000;
            g_scene_far_z = 0x7fffffff;
            if (stage23_trace_frame == 0 && focus->type == 0x36)
                tank_create(focus);
            ++stage23_trace_frame;
        }
    }
    {
        static sint32 stage24_trace_frame;
        const char *trace = getenv("OA_STAGE24_TRACE");
        if (trace && *trace && g_stage_index == 24 && g_effect_count == 92 && stage24_trace_frame == 0)
        {
            EFFECT *low = &g_effects[0], *high = &g_effects[56];
            FILE *file = fopen("../status/jungle-falls-native.log", "w");
            camera_depth_limit_set(low);
            camera_depth_limit_set(high);
            if (file)
            {
                fprintf(file, "JUNGLE_FALLS_NATIVE stage=24 effects=92 indices=0,56 types=%u,%u handlers=%u,%u z=%d,%d globals=%d,%d\n", low->type, high->type, low->handler, high->handler, low->z, high->z, g_player_far_z_limit, g_player_near_z_limit);
                fclose(file);
            }
            ++stage24_trace_frame;
        }
    }
    {
        static sint32 stage25_trace_frame;
        const char *trace = getenv("OA_STAGE25_TRACE");
        if (trace && *trace && g_stage_index == 25 && g_effect_count == 133 && stage25_trace_frame == 0)
        {
            EFFECT *focus = &g_effects[125];
            if (focus->type == 0x50)
                v2_rocket_create(focus);
            ++stage25_trace_frame;
        }
    }
    {
        static sint32 stage26_trace_frame;
        const char *trace = getenv("OA_STAGE26_TRACE");
        if (trace && *trace && g_stage_index == 26 && g_effect_count == 158 && stage26_trace_frame == 0)
        {
            EFFECT *focus = &g_effects[39];
            sint32 player_x = g_player_world_x;
            g_player_world_x = focus->x;
            if (focus->type == 0x39)
                falling_hazard_create(focus);
            g_player_world_x = player_x;
            ++stage26_trace_frame;
        }
    }
    {
        static sint32 stage27_trace_frame;
        const char *trace = getenv("OA_STAGE27_TRACE");
        if (trace && *trace && g_stage_index == 27 && g_effect_count == 179 && stage27_trace_frame == 0)
        {
            EFFECT *focus = &g_effects[35];
            if (focus->type == 0x7f)
                industry_gun_controller_create(focus);
            ++stage27_trace_frame;
        }
    }
    {
        static sint32 stage28_trace_frame;
        const char *trace = getenv("OA_STAGE28_TRACE");
        if (trace && *trace && g_stage_index == 28 && g_effect_count == 349 && stage28_trace_frame == 0)
        {
            sint32 j;
            EFFECT *focus = 0;
            for (j = 0; j < g_effect_count; j++)
                if (g_effects[j].type == 0x8f)
                {
                    focus = &g_effects[j];
                    break;
                }
            if (focus)
            {
                sint32 x = g_player_world_x, y = g_player_world_y, z = g_player_world_z;
                g_player_world_x = focus->x;
                g_player_world_y = focus->y + 0x2000;
                g_player_world_z = focus->z;
                tower_surface_trigger_update(focus);
                g_player_world_x = x;
                g_player_world_y = y;
                g_player_world_z = z;
            }
            ++stage28_trace_frame;
        }
    }
    {
        static sint32 stage29_trace_frame;
        const char *trace = getenv("OA_STAGE29_TRACE");
        if (trace && *trace && g_stage_index == 29 && g_effect_count == 265 && stage29_trace_frame == 0)
        {
            sint32 j;
            EFFECT *a = 0, *b = 0;
            for (j = 0; j < g_effect_count; j++)
            {
                if (g_effects[j].type == 0x89)
                    a = &g_effects[j];
                if (g_effects[j].type == 0x8c)
                    b = &g_effects[j];
            }
            if (a)
                player_action_effect_create(a);
            if (b)
                player_action_effect_create(b);
            ++stage29_trace_frame;
        }
    }
    {
        static sint32 stage31_trace_frame;
        const char *trace = getenv("OA_STAGE31_TRACE");
        if (trace && *trace && g_stage_index == 31 && g_effect_count == 293 && stage31_trace_frame == 0)
        {
            sint32 j;
            for (j = 0; j < g_effect_count; j++)
                if (g_effects[j].type == 0x7d)
                {
                    bio_goo_create(&g_effects[j]);
                    break;
                }
            ++stage31_trace_frame;
        }
    }
    {
        static sint32 stage32_trace_frame;
        const char *trace = getenv("OA_STAGE32_TRACE");
        if (trace && *trace && g_stage_index == 32 && g_effect_count == 229 && stage32_trace_frame == 0)
        {
            sint32 j;
            if (g_effects[19].type == 0x92)
                player_action_effect_create(&g_effects[19]);
            for (j = 0; j < g_effect_count; j++)
                if (g_effects[j].type == 0x80)
                {
                    robot_press_create(&g_effects[j]);
                    break;
                }
            ++stage32_trace_frame;
        }
    }
    {
        static sint32 trace_frame;
        static sint32 trace_header;
        const char *trace = getenv("OA_SUB_BASE_TRACE");
        if (trace && *trace && !trace_header)
        {
            FILE *file = fopen("../status/sub-base-native-focus.log", "w");
            if (file)
            {
                fprintf(file, "stage=%d effects=%d\n", g_stage_index, g_effect_count);
                fclose(file);
            }
            trace_header = 1;
        }
        if (trace && *trace && g_stage_index == 16 && g_effect_count == 284)
        {
            static const sint16 focus_indices[6] = {99, 65, 5, 23, 21, 1};
            sint32 segment = trace_frame < 30 ? 0 : trace_frame < 90 ? 1 : trace_frame < 120 ? 2 : trace_frame < 150 ? 3 : trace_frame < 180 ? 4 : 5;
            EFFECT *focus = &g_effects[focus_indices[segment]];
            g_camera_world_x = focus->x;
            g_camera_world_y = focus->y;
            g_camera_world_z = focus->z - 0x4000;
            if ((trace_frame % 30) == 0)
            {
                FILE *file = fopen("../status/sub-base-native-focus.log", trace_frame ? "a" : "w");
                if (file)
                {
                    fprintf(file, "frame=%d index=%d type=%u handler=%u xyz=%d,%d,%d\n", trace_frame, focus_indices[segment], focus->type, focus->handler, focus->x, focus->y, focus->z);
                    fclose(file);
                }
            }
            ++trace_frame;
        }
    }
#endif

    for (i = 0; i < g_effect_count; ++i)
    {
        EFFECT *effect = &g_effects[i];
        EffectUpdateCallback callback;

        if (!effect->type || !effect_is_visible(effect))
            continue;
        callback = g_effect_handlers[effect->handler].update;
        if (callback != NULL)
            callback(effect);
    }
}

static sint32 effect_bypasses_initial_culling(sint32 type)
{
    static const uint16 types[] = {0x16, 0x28, 0x30, 0x5a, 0x5b, 0x96, 0x45, 0x4c, 0x60, 0x4e, 0x4f, 0x5e, 0x6b, 0x6c, 0x72, 0x7f, 0x3e, 0x7c, 0x6d};
    sint32 index;
    for (index = 0; index < (sint32)(sizeof(types) / sizeof(types[0])); index++)
        if (type == types[index])
            return 1;
    return 0;
}

/* Direct translation of 0x800A85F0..0x800A8788. */
/* Original: FUN_800A85F0. */
void effects_spawn_initial(void)
{
    sint32 index;
    const sint16 *remap = stage_effect_remap[g_stage_index];
    for (index = 0; index < g_effect_count; index++)
    {
        EFFECT *effect = &g_effects[index];
        EffectUpdateCallback callback;
        sint32 type = (sint16)effect->type;
        if (type == 0)
            continue;
        if (type == remap[0])
        {
            effect->type = remap[1];
            type = remap[1];
        }
        if (!effect_bypasses_initial_culling(type) && !effect_is_visible(effect))
            continue;
        callback = g_effect_handlers[effect->handler].update;
        if (callback != 0)
            callback(effect);
    }
}

/* Direct translation of FUN_800A89D0. */
sint32 effect_count(sint32 type)
{
    EFFECT *current = NULL;
    sint16 count = 0;
    for (;;)
    {
        current = effect_find_next(type, current);
        if (current == NULL)
            break;
        ++count;
        ++current;
    }
    return count;
}
