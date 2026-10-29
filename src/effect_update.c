#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "collision.h"
#include "game.h"
#include "airship.h"
#include "render.h"
#include "spatial.h"
#include "game_sound.h"
#include "camera.h"
#include "effect_update.h"
#include "map_render.h"
#include "input.h"
#include "mechanism.h"
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
#include "tank.h"
#include "truck.h"
#include "v2.h"

/* Types. */
/* FUN_800A878C and FUN_800A8838: visibility-filtered effect updates. */

static EFFECT empty_effect;

/* EFFECTS file words stay byte-addressable and little-endian */
typedef struct
{
    uint8 x[2], y[2], z[2], type[2];
    uint8 vals[7][2];
} EFFECT_FILE_RECORD;

enum
{
    EFFECT_FILE_RECORD_SIZE = 22
};

typedef char EFFECT_FILE_RECORD_SIZE_MATCH[sizeof(EFFECT_FILE_RECORD) == EFFECT_FILE_RECORD_SIZE ? 1 : -1];

/* Original: DAT_800D4210. */
EFFECT *g_effects = &empty_effect;
/* Original: DAT_800D415C. */
sint32 g_effect_count;

/* Original: DAT_800CAFB4. */
typedef enum
{
    EFFECT_HANDLER_UPDATE,
    EFFECT_HANDLER_ENEMY_FACTORY,
    EFFECT_HANDLER_SPRITE_FACTORY
} EFFECT_HANDLER_KIND;

typedef struct
{
    EFFECT_HANDLER_KIND kind;

    union
    {
        EFFECT_UPDATE_CALLBACK update;
        MISSION_ENEMY_ACTOR *(*enemy_factory)(EFFECT *src);
        VRAM_SPRITE *(*sprite_factory)(EFFECT *src);
    } callback;
} EFFECT_HANDLER;

static EFFECT_HANDLER effect_handlers[0x58];

void effects_heap_reset(void)
{
    g_effects = &empty_effect;
    g_effect_count = 0;
    memset(effect_handlers, 0, sizeof(effect_handlers));
}

typedef struct
{
    COLLISION col;
    EFFECT *src_effect;
    sint32 frame;
    sint32 dst;
    sint32 reverse_exit;
} INTER_ROOM_TRANSITION;

typedef struct
{
    COLLISION col;
    sint16 frame;
    EFFECT *target_effect;
} DOOR_EFFECT_TRANSITION;

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
static void mission_music_trigger_update(EFFECT *effect)
{
    sint16 music = effect->vals[0];
    if (g_cur_music_track != music)
        music_track_select(music);
}

/* Exact PAL 0x800A8348..0x800A8460: type-0x7A animated jungle effect. */
/* Original: FUN_800A8348. */
static void jungle_animated_effect_update(EFFECT *effect)
{
    POLY_FT4 *poly = g_next_transient_prim;
    sint16 frame;
    sint32 old_frame;
    XASSERT(!(g_next_transient_prim >= g_transient_prims + TRANSIENT_PRIM_LIMIT));
    ++g_next_transient_prim;
    setPolyFT4(poly);
    setSemiTrans(poly, 1);
    frame = effect->vals[0];
    if (frame == 0)
    {
        frame = random_range(0x0b) + 1;
        effect->vals[0] = frame;
    }
    old_frame = frame - 1;
    if (effect->vals[1] == 4)
    {
        effect->vals[0] = (frame + 1);
        effect->vals[1] = 0;
    }
    effect->vals[1] = (effect->vals[1] + 1);
    if (effect->vals[0] == 13)
        effect->vals[0] = 1;
    g_next_frame_object->light_delta = -0x20;
    g_next_frame_object->force_light = 1;
    render_world_spr((0x2e800 + old_frame), effect->x, (effect->y + 0x2000u), effect->z, 0, 0x1000, 0x1000, poly, 0, 2, 0, 0);
}

/* Original: FUN_800A8250. */
static void mission_background_disable_trigger(EFFECT *effect)
{
    g_map_render.background_disabled = 1;
}

/* Original: FUN_800A8974. */
GDB_CALL EFFECT *effect_find_next(sint32 type, EFFECT *after)
{
    EFFECT *cur = after != NULL ? after : g_effects;
    EFFECT *end = g_effects + g_effect_count;

    while (cur < end)
    {
        if (cur->type == type)
            return cur;
        ++cur;
    }
    return NULL;
}

/* Original: FUN_800A9300. */
static void inter_room_transition_update(void *raw_transition)
{
    INTER_ROOM_TRANSITION *transition = (INTER_ROOM_TRANSITION *)raw_transition;
    PLAYER *player = g_player;
    sint16 old_frame = transition->frame;
    sint16 frame;
    /* Keep the full word counter while testing halfword phases */
    transition->frame = transition->frame + 1u;
    frame = transition->frame;
    if (old_frame >= 0 && old_frame < 0x2a)
        g_map_render.brightness = -frame * 4;
    if (old_frame >= 0x2a && old_frame < 0x53)
        g_map_render.brightness = (frame - 0x2a) * 3 - 0x80;
    if (frame == 0x2a)
    {
        EFFECT *src = transition->src_effect;
        EFFECT *target = 0;
        sint16 dst = transition->dst;
        for (;;)
        {
            target = effect_find_next(0x46, target);
            if (target == 0)
                fatal_error("NO DEST DOOR");
            if (target->signed_vals[0] == dst)
                break;
            ++target;
        }
        if (target->vals[1] == 999)
            target->vals[3] = src->vals[0];
        player->col.x = target->x;
        player->col.y = target->y;
        if (target->vals[2] != 0 && target->vals[2] != 2)
            player->col.y = player->col.y + 0x2000u;
        player->col.z = target->z;
        if (transition->reverse_exit != 0)
            player->col.z = player->col.z + 0x1c00u;
        sint16 exit_dir = target->vals[2];
        if (exit_dir == 0)
            player->col.y = player->col.y - 0xa00u;
        if (exit_dir == 1)
            player->col.x = player->col.x + 0x4000u;
        if (exit_dir == 2)
            player->col.y = player->col.y + 0xa00u;
        if (exit_dir == 3)
            player->col.x = player->col.x - 0x4000u;
        if (exit_dir == 4)
            player->col.z = player->col.z + 0x4000u;
        if (exit_dir == 5)
            player->col.z = player->col.z - 0x4000u;
        g_player_world.x = g_camera.prev_player_x = player->col.x;
        g_player_world.y = player->col.y;
        g_player_world.z = player->col.z;
        g_camera.limit_near = -1;
        g_camera.room_player_near_z = 0;
        g_camera.prev_z = -1;
        g_map_render.background_disabled = 1;
        camera_update(g_player_world.x, g_player_world.y, g_player_world.z, g_player->dir);
        g_camera.prev_z = -1;
    }
    if (frame == 0x65)
    {
        player = g_player;
        player->col.object.update = transition->reverse_exit ? player_ledge_control_update : player_control_update;
        player->col.sends_mask = 2;
        object_destroy(transition);
    }
}

/* Original: FUN_800A8BD0. */
void door_transition_player_update(void *raw_player)
{
    PLAYER *player = (PLAYER *)raw_player;
    if (g_camera.room_player_near_z != 0 && player->col.z < g_camera.room_player_near_z)
        player->col.z = g_camera.room_player_near_z;
    g_player_world.x = player->col.x;
    g_player_world.y = player->col.y;
    g_player_world.z = player->col.z;
    camera_update(g_player_world.x, g_player_world.y, g_player_world.z, player->prev_dir);
    g_camera.prev_player_x = g_player_world.x;
    g_camera.prev_player_y = g_player_world.y;
    g_camera.prev_player_z = g_player_world.z;
}

/* Original: FUN_800A9650. */
static void inter_room_transition_trigger_update(EFFECT *effect)
{
    PLAYER *player = g_player;
    sint32 vertical = (effect->vals[2] == 0 || effect->vals[2] == 2) ? 0x600 : 0x2800;
    sint16 dst;
    INTER_ROOM_TRANSITION *transition;
    const sint32 vertical_word = vertical;
    if (g_player_world.x < (effect->x - 0x2000) || g_player_world.x >= (effect->x + 0x2000) || g_player_world.z < (effect->z - 0x2000) || g_player_world.z >= (effect->z + 0x2000) || g_player_world.y <= (effect->y - vertical_word) || g_player_world.y >= (effect->y + vertical_word))
        return;
    if (player->col.object.update != player_control_update && player->col.object.update != player_ledge_control_update)
        return;
    sound_voice_stop(&player->spatial_sound_handle);
    player = g_player;
    if (player->col.object.update == door_transition_player_update)
        return;
    dst = effect->vals[1];
    if (dst == 999)
        dst = effect->vals[3];
    transition = (INTER_ROOM_TRANSITION *)object_create(sizeof(*transition), inter_room_transition_update);
    transition->src_effect = effect;
    transition->dst = dst;
    player = g_player;
    if (player->col.object.update == player_ledge_control_update)
        transition->reverse_exit = 1;
    player->col.object.update = door_transition_player_update;
    player->col.receives_mask = 0;
    player->col.sends_mask = 0;
    player->animation.next = 0;
    player->animation.start = 0;
}

/* Original: FUN_800A8F2C. */
static SPRITE *door_transition_spr_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation)
{
    VRAM_SPRITE *desc = (VRAM_SPRITE *)runtime_heap_allocate(sizeof(*desc));
    SPRITE *actor = world_spr_create(x, y, z, animation);
    actor->owns_resource = 1;
    spr_vram_allocate(desc, 0x38, 0x50, 1);
    actor->col.frame_desc = desc;
    desc->clut = g_player_spr_clut;
    actor->lifetime = 0x3c;
    actor->retain_offscreen = 1;
    return actor;
}

/* Original: FUN_800A8CF0. */
static void door_transition_update(void *raw_transition)
{
    DOOR_EFFECT_TRANSITION *transition = (DOOR_EFFECT_TRANSITION *)raw_transition;
    PLAYER *player = g_player;
    sint16 frame = transition->frame;
    if (frame == 0)
    {
        SPRITE *actor;
        player->col.object.update = door_transition_player_update;
        player->col.receives_mask = 0;
        player->col.sends_mask = 0;
        player->animation.next = 0;
        actor = door_transition_spr_create(transition->col.x, transition->col.y, transition->col.z, g_player_run_animation0);
        actor->light_step = -4;
        actor->scale_step = -0x20;
        actor->light_delta = 0;
        actor->velocity_y = -0xa0;
        actor->scale_x = g_default_spr_scale;
    }
    if (frame == 0x3a)
    {
        EFFECT *target = transition->target_effect;
        SPRITE *actor;
        player->col.x = target->x;
        player->col.y = target->y + 0x2000;
        player->col.z = target->z + 0x1c00;
        player->dir = 4;
        g_player_world.x = g_camera.prev_player_x = target->x;
        g_player_world.y = target->y + 0x2000;
        g_player_world.z = target->z + 0x1c00;
        g_camera.prev_z = -1;
        actor = door_transition_spr_create(target->x, target->y - 0x580, target->z + 0x1e00, g_player_run_animation4);
        actor->light_delta = -0xf0;
        actor->scale_step = 0x20;
        actor->light_step = 4;
        actor->velocity_y = 0xa0;
        actor->scale_x = (g_default_spr_scale - 0x780);
    }
    if (frame == 0x75)
    {
        player->col.object.update = player_control_update;
        player->col.sends_mask = 2;
        player->animation.next = 0;
        player->animation.start = 0;
        player->animation.mirrored = 0;
        player->weapon_animation_active = 0;
        player->animation.flags = 0x400;
        object_destroy(transition);
    }
    if (frame >= 0x11 && frame < 0x3b)
        g_map_render.brightness = -frame * 3;
    if (frame >= 0x3b && frame < 0x64)
        g_map_render.brightness = (frame - 0x3a) * 3 - 0x80;
    transition->frame = (frame + 1);
}

/* Original: FUN_800A8C80. */
static void door_transition_create(sint32 x, sint32 y, sint32 z, EFFECT *target)
{
    DOOR_EFFECT_TRANSITION *transition = (DOOR_EFFECT_TRANSITION *)object_create(sizeof(*transition), door_transition_update);
    transition->col.x = x;
    transition->col.y = y;
    transition->col.z = z;
    transition->target_effect = target;
    door_transition_update(transition);
}

/* Original: FUN_800A8A28. */
static void door_trigger_update(void *raw_effect)
{
    EFFECT *src = (EFFECT *)raw_effect;
    PLAYER *player = g_player;
    EFFECT *scan = 0;
    sint16 dst;
    /* 800A8A48 branches out when the player is already controlled by the
     * door callback, then accepts only FUN_8009AADC.  Treating both callbacks
     * as admissible spawned a fresh transition actor every held-input frame. */
    if (player->col.object.update == door_transition_player_update)
        return;
    if (player->col.object.update != player_control_update)
        return;
    if ((g_input.held & PADLup) == 0 || player->airborne != 0)
        return;
    if (g_player_world.x <= src->x - 0x1000 || g_player_world.x >= src->x + 0x1000)
        return;
    if (g_player_world.z < src->z + 0x1c00 || g_player_world.z >= src->z + 0x2000)
        return;
    if (g_player_world.y <= src->y || g_player_world.y >= src->y + 0x2800)
        return;
    sound_voice_stop(&player->spatial_sound_handle);
    dst = src->vals[1] == 999 ? (sint16)src->vals[3] : (sint16)src->vals[1];
    for (;;)
    {
        scan = effect_find_next(0x41, scan);
        if (!scan)
            fatal_error("NO DEST DOOR");
        if (scan->signed_vals[0] == dst)
            break;
        ++scan;
    }
    if (scan->vals[1] == 999)
        scan->vals[3] = src->vals[0];
    door_transition_create(src->x, g_player_world.y, g_player_world.z, scan);
}

static void initialize_mission_effect_handlers(void)
{
    memset(effect_handlers, 0, sizeof(effect_handlers));
    effect_handlers[1].callback.update = player_action_effect_create;
    effect_handlers[2].callback.update = player_action_effect_create;
    effect_handlers[3].callback.update = player_action_effect_create;
    effect_handlers[4].callback.update = mortar_create;
    effect_handlers[6].callback.update = player_action_effect_create;
    effect_handlers[7].callback.update = player_action_effect_create;
    effect_handlers[8].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[8].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x0a].kind = EFFECT_HANDLER_SPRITE_FACTORY;
    effect_handlers[0x0a].callback.sprite_factory = sentry_create;
    effect_handlers[0x0b].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x0b].callback.enemy_factory = mission_enemy_create;
    if (g_session.stage == 1)
        effect_handlers[0x0c].callback.update = truck_create;
    if (g_session.stage == 8)
        effect_handlers[0x14].callback.update = gyro_boss_create;
    if (g_session.stage == 10)
        effect_handlers[0x44].callback.update = gunship_create;
    if (g_session.stage == 12 || g_session.stage == 14 || g_session.stage == 25)
        effect_handlers[0x29].callback.update = v2_rocket_create;
    effect_handlers[0x0d].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x0d].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x0e].callback.update = building_create;
    effect_handlers[0x0f].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x0f].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x12].callback.update = player_action_effect_create;
    effect_handlers[0x13].callback.update = lift_create;
    effect_handlers[0x18].callback.update = radar_vehicle_create;
    effect_handlers[0x19].callback.update = falling_hazard_create;
    effect_handlers[0x15].callback.update = tanya_gun_emplacement_create;
    effect_handlers[0x16].callback.update = tank_create;
    effect_handlers[0x28].callback.update = tanya_create;
    effect_handlers[0x20].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x20].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x1d].callback.update = door_trigger_update;
    effect_handlers[0x21].callback.update = drop_range_create;
    effect_handlers[0x22].callback.update = inter_room_transition_trigger_update;
    effect_handlers[0x23].callback.update = jetpack_enemy_create;
    effect_handlers[0x1c].callback.update = mechanism_switch_create;
    effect_handlers[0x25].callback.update = flare_area_create;
    effect_handlers[0x26].callback.update = horizontal_camera_limit_create;
    effect_handlers[0x27].callback.update = horizontal_camera_limit_create;
    effect_handlers[0x2f].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x2f].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x30].callback.update = mini_gyro_create;
    effect_handlers[0x31].callback.update = scuba_submarine_create;
    effect_handlers[0x33].callback.update = scuba_drifting_hazard_create;
    effect_handlers[0x2a].callback.update = mission_background_disable_trigger;
    /* Resident 800CAFA8 + 2Ch*10h: type 5B, callback 8009FFD0.
     * HQ EFFECTS[5] is the signal-2 passage, using this shared lift code. */
    effect_handlers[0x2b].callback.update = lift_create;
    effect_handlers[0x2c].callback.update = lift_create;
    effect_handlers[0x55].callback.update = lift_create;
    effect_handlers[0x2d].callback.update = building_collapse_create;
    effect_handlers[0x2e].callback.update = flare_area_create;
    effect_handlers[0x36].callback.update = vertical_camera_limit_create;
    effect_handlers[0x35].callback.update = v2_radar_marker_create;
    effect_handlers[0x37].callback.update = vertical_camera_limit_create;
    effect_handlers[0x38].callback.update = scuba_tug_target_create;
    effect_handlers[0x39].callback.update = scuba_diver_enemy_create;
    effect_handlers[0x34].callback.update = scuba_mine_create;
    effect_handlers[0x3a].callback.update = scuba_noop_effect_update;
    effect_handlers[0x3b].callback.update = scuba_bubble_emitter_update;
    effect_handlers[0x3c].callback.update = camera_depth_limit_set;
    effect_handlers[0x3d].callback.update = scuba_ship_target_create;
    effect_handlers[0x3e].callback.update = mini_gyro_create;
    effect_handlers[0x43].callback.update = jungle_animated_effect_update;
    if (g_session.stage >= 27)
        effect_handlers[0x47].callback.update = bio_goo_create;
    effect_handlers[0x52].callback.update = tower_surface_trigger_update;
    if (g_session.stage >= 27)
        effect_handlers[0x48].callback.update = industry_gun_create;
    if (g_session.stage >= 27)
        effect_handlers[0x49].callback.update = robot_press_create;
    effect_handlers[0x4a].callback.update = airship_create;
    effect_handlers[0x4b].callback.update = mission_music_trigger_update;
    effect_handlers[0x4c].callback.update = player_action_effect_create;
    effect_handlers[0x4d].callback.update = player_action_effect_create;
    effect_handlers[0x4e].callback.update = player_action_effect_create;
    effect_handlers[0x4f].callback.update = player_action_effect_create;
    /* Original resident record 800CAFA8 + 50h*10h has type 8D and
     * callback 800FDAD0 at 800CB4B4. That address belongs to HQ.BIN. */
    effect_handlers[0x50].callback.update = g_session.stage == 0x0b ? hq_portrait_trigger_update : 0;
    effect_handlers[0x53].callback.update = player_action_effect_create;
    effect_handlers[0x17].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x17].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x32].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x32].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x45].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x45].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x51].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x51].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x54].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x54].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x56].kind = EFFECT_HANDLER_ENEMY_FACTORY;
    effect_handlers[0x56].callback.enemy_factory = mission_enemy_create;
    effect_handlers[0x24].callback.update = player_action_effect_create;
    effect_handlers[0x3f].callback.update = player_action_effect_create;
    effect_handlers[0x40].callback.update = mini_gyro_create;
    effect_handlers[0x41].callback.update = player_action_effect_create;
    effect_handlers[0x46].callback.update = flare_area_create;
}

/* Direct translation of 0x800A8460..0x800A85E8. */
/* Original: FUN_800A8460. */
void effects_initialize(const void *data, uint32 size)
{
    const uint8 *src = (const uint8 *)data;
    EFFECT *target;
    uint32 count = 0, offset = 0, idx;
    uint32 depth_origin;
    XASSERT(!(src == 0));
    for (;;)
    {
        XASSERT(!(offset > size || size - offset < 2));
        if (xport_load_le16(src + offset) == 0)
            break;
        XASSERT(!(size - offset < EFFECT_FILE_RECORD_SIZE || count >= 0x7fffffffu / sizeof(*target)));
        ++count;
        offset += EFFECT_FILE_RECORD_SIZE;
    }
    target = (EFFECT *)runtime_heap_allocate((count * sizeof(*target)));
    g_effect_count = count;
    g_effects = target;
    depth_origin = ((g_map.depth * 3u << 6) - g_map.depth_subcells);
    initialize_mission_effect_handlers();
    for (idx = 0; idx < count; ++idx)
    {
        const EFFECT_FILE_RECORD *entry = (const EFFECT_FILE_RECORD *)(src + idx * EFFECT_FILE_RECORD_SIZE);
        EFFECT *effect = target + idx;
        uint32 val_idx;
        sint32 handler;
        effect->x = (sint16)xport_load_le16(entry->x) * 256;
        effect->y = (sint16)xport_load_le16(entry->y) * 256;
        effect->z = ((depth_origin + (sint16)xport_load_le16(entry->z)) << 8);
        effect->type = (sint16)xport_load_le16(entry->type);
        for (handler = 0; handler < 0x58; ++handler)
            if (effect_handler_types[handler] == effect->type)
                effect->handler = handler;
        for (val_idx = 0; val_idx < 7; ++val_idx)
            effect->vals[val_idx] = xport_load_le16(entry->vals[val_idx]);
    }
}

/* Original: FUN_800A8838. */
sint32 effect_is_visible(EFFECT *effect)
{
    sint32 camera_z = g_camera.world_z;
    sint32 effect_z = effect->z;
    sint32 relative_depth; sint32 numerator; sint32 scr_x; sint32 scr_y; sint32 vertical_extent;

    if (effect_z < (camera_z + 0x4000))
        return 0;
    relative_depth = ((uint32)effect_z - camera_z);
    numerator = (((uint32)effect->x - g_camera.world_x) * 0x140);
    /* Original BREAK faults stop this operation instead of producing a quotient */
    if (relative_depth == 0)
    {
#if defined(_DEBUG)
        fputs("EFFECT VISIBILITY BREAK 7\n", stderr);
#endif
        abort();
    }
    if (relative_depth == -1 && numerator == INT32_MIN)
    {
#if defined(_DEBUG)
        fputs("EFFECT VISIBILITY X BREAK 6\n", stderr);
#endif
        abort();
    }
    scr_x = numerator / relative_depth;
    if (scr_x + 0x100u >= 0x201)
        return 0;
    numerator = (((uint32)effect->y - g_camera.world_y) * 0x163);
    if (relative_depth == -1 && numerator == INT32_MIN)
    {
#if defined(_DEBUG)
        fputs("EFFECT VISIBILITY Y BREAK 6\n", stderr);
#endif
        abort();
    }
    scr_y = numerator / relative_depth;
    vertical_extent = g_map_render.vertical_extent;
    if (scr_y < - vertical_extent || scr_y > (vertical_extent + 0x50))
        return 0;
    if (effect_z > (camera_z + (g_map_render.effect_depth << 8)))
        return 0;
    return effect_z <= g_map_render.far_z;
}

/* Original: FUN_800A878C. */
void effects_update_visible(void)
{
    sint32 i;

    for (i = 0; i < g_effect_count; ++i)
    {
        EFFECT *effect = &g_effects[i];
        const EFFECT_HANDLER *handler;

        if (!effect->type || !effect_is_visible(effect))
            continue;
        handler = &effect_handlers[effect->handler];
        if (handler->kind == EFFECT_HANDLER_ENEMY_FACTORY)
            handler->callback.enemy_factory(effect);
        else if (handler->kind == EFFECT_HANDLER_SPRITE_FACTORY)
            handler->callback.sprite_factory(effect);
        else if (handler->callback.update != 0)
            handler->callback.update(effect);
    }
}

/* Direct translation of 0x800A85F0..0x800A8788. */
/* Original: FUN_800A85F0. */
void effects_spawn_initial(void)
{
    sint32 idx;
    const sint16 *remap = stage_effect_remap[g_session.stage];
    for (idx = 0; idx < g_effect_count; idx++)
    {
        EFFECT *effect = &g_effects[idx];
        const EFFECT_HANDLER *handler;
        sint16 type = effect->type;
        if (type == 0)
            continue;
        if (type == remap[0])
        {
            effect->type = remap[1];
            type = remap[1];
        }
        switch (type)
        {
            case 0x16:
            case 0x28:
            case 0x30:
            case 0x5a:
            case 0x5b:
            case 0x96:
            case 0x45:
            case 0x4c:
            case 0x60:
            case 0x4e:
            case 0x4f:
            case 0x5e:
            case 0x6b:
            case 0x6c:
            case 0x72:
            case 0x7f:
            case 0x3e:
            case 0x7c:
            case 0x6d:
                break;
            default:
                if (!effect_is_visible(effect))
                    continue;
                break;
        }
        handler = &effect_handlers[effect->handler];
        if (handler->kind == EFFECT_HANDLER_ENEMY_FACTORY)
            handler->callback.enemy_factory(effect);
        else if (handler->kind == EFFECT_HANDLER_SPRITE_FACTORY)
            handler->callback.sprite_factory(effect);
        else if (handler->callback.update != 0)
            handler->callback.update(effect);
    }
}

/* Original: FUN_800A89D0. */
sint16 effect_count(sint32 type)
{
    EFFECT *cur = NULL;
    uint32 count = 0;
    for (;;)
    {
        cur = effect_find_next(type, cur);
        if (cur == NULL)
            break;
        ++count;
        ++cur;
    }
    return count;
}
