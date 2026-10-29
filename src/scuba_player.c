#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "game_math.h"
#include "game.h"
#include "airship.h"
#include "spatial.h"
#include "animation.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "render.h"
#include "input.h"
#include "hq.h"
#include "map.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "psx.h"
#include "random.h"
#include "runtime_heap.h"
#include "scuba_player.h"
#include "sprite.h"
#include "sprite_renderer.h"

/* Original: DAT_800D4038. */
MODEL_POSE_RESOURCE g_scuba_tug_initial_pose;

/* Original: DAT_800D4030. */
sint16 g_scuba_tug_model_base;

/* Original: DAT_800D3F7C. */
sint16 g_depth_charge_model_id;

/* Original: DAT_800D4004. */
MODEL_POSE_RESOURCE g_mini_sub_initial_pose;

/* Original: DAT_800D4000. */
sint16 g_mini_sub_model_base;

/* Original data: WATER/SCUBA.BIN:0x800FF084..0x800FF0A7 */
static const ANIM_COMMAND bubble_item_animation[5] = {
    {ANIM_STEP_FRAME, {4, 188417, 0}, 0}, {ANIM_STEP_FRAME, {4, 188418, 0}, 0}, {ANIM_STEP_FRAME, {4, 188419, 0}, 0}, {ANIM_STEP_FRAME, {4, 188418, 0}, 0}, {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF0A8..0x800FF123 */
static const ANIM_COMMAND blast_animation[16] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {4, 179200, 0}, 0}, {ANIM_STEP_FRAME, {4, 179201, 0}, 0}, {ANIM_STEP_FRAME, {4, 179202, 0}, 0}, {ANIM_STEP_FRAME, {4, 179203, 0}, 0}, {ANIM_STEP_FRAME, {4, 179204, 0}, 0}, {ANIM_STEP_FRAME, {4, 179205, 0}, 0}, {ANIM_STEP_FRAME, {4, 179206, 0}, 0}, {ANIM_STEP_FRAME, {4, 179207, 0}, 0}, {ANIM_STEP_FRAME, {4, 179208, 0}, 0}, {ANIM_STEP_FRAME, {4, 179209, 0}, 0}, {ANIM_STEP_FRAME, {4, 179210, 0}, 0}, {ANIM_STEP_FRAME, {4, 179211, 0}, 0}, {ANIM_STEP_FRAME, {4, 179212, 0}, 0}, {ANIM_STEP_FRAME, {4, 179213, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF124..0x800FF16F */
static const ANIM_COMMAND turn_flash_animation[10] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {3, 179214, 0}, 0}, {ANIM_STEP_FRAME, {3, 179215, 0}, 0}, {ANIM_STEP_FRAME, {3, 179216, 0}, 0}, {ANIM_STEP_FRAME, {3, 179217, 0}, 0}, {ANIM_STEP_FRAME, {3, 179218, 0}, 0}, {ANIM_STEP_FRAME, {3, 179219, 0}, 0}, {ANIM_STEP_FRAME, {3, 179220, 0}, 0}, {ANIM_STEP_FRAME, {3, 179221, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF170..0x800FF19B */
static const ANIM_COMMAND particle_animation[6] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {4, 179222, 0}, 0}, {ANIM_STEP_FRAME, {4, 179223, 0}, 0}, {ANIM_STEP_FRAME, {4, 179224, 0}, 0}, {ANIM_STEP_FRAME, {4, 179223, 0}, 0}, {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF19C..0x800FF1AF */
static const ANIM_COMMAND water_particle_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {4000, 179225, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF1B0..0x800FF1C3 */
static const ANIM_COMMAND projectile_trail_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {200, 179227, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF1D4..0x800FF1E7 */
static const ANIM_COMMAND projectile_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {4000, 179226, 0}, 0},
    {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FF1F4..0x800FF207 */
static const ANIM_COMMAND blast_impact_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {60, 179228, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: WATER/SCUBA.BIN:0x800FEFBC..0x800FF083 */
static const uint32 initial_pose_data[50] = {
    0x00000006u, 0x00000002u, 0x00000038u, 0xffffffffu, 0x00000050u, 0xffffffffu, 0x00000068u, 0x00000001u, 0x00000080u, 0x00000001u, 0x00000098u, 0x00000001u, 0x000000b0u, 0x00000001u, 0xc0000000u, 0xfbfa0000u, 0x1c6396f0u, 0xc0000000u, 0xfbfa0000u, 0x1c6396f0u, 0x00008000u, 0x00008000u, 0x1553cf40u, 0x00008000u, 0x00008000u, 0x1553cf40u, 0x00000000u, 0x00000000u, 0xb7d5f8d7u, 0x00000000u, 0x00000000u, 0xb7d5f8d7u, 0x00000000u, 0x00000000u, 0xd1ba08d0u, 0x00000000u, 0x00000000u, 0xd1ba08d0u, 0x00000000u, 0x00000000u, 0xdb18f8d7u, 0x00000000u, 0x00000000u, 0xdb18f8d7u, 0x00000000u, 0x00000000u, 0x3f59fc25u, 0x00000000u, 0x00000000u, 0x3f59fc25u,
};
static const MODEL_POSE_RESOURCE initial_pose = {initial_pose_data, sizeof(initial_pose_data)};

/* Original data: WATER/SCUBA.BIN:0x800FF1C4..0x800FF1D3 */
static const sint16 dir_frames[8] = {
    72, 36, 0, 108, 144, 108, 0, 36,
};

/* Original data: WATER/SCUBA.BIN:0x800FF214..0x800FF21D */
static const sint16 tug_model_offsets[5] = {
    0, -1, -1, -1, -1,
};

/* Original data: WATER/SCUBA.BIN:0x800FF1E8..0x800FF1EF */
const sint8 g_scuba_mini_gyro_model_offsets[8] = {
    3, 2, 0, 1, -1, -1, -1, -1,
};

/* Types. */
/* WATER/SCUBA.BIN player overlay.
 *
 * Every routine below is translated from the PAL module loaded at
 * 0x800FADD0.  The checked module SHA-256 is recorded in
 * status/scuba-callgraph.json; offsets in comments are original addresses. */

typedef struct
{
    sint32 x;
    sint32 y;
    sint32 z;
} SCUBA_VECTOR;

typedef struct
{
    sint16 count;
    sint16 idx;
    sint16 dir;
    EFFECT *points[13];
    SCUBA_VECTOR step;
} SCUBA_ROUTE;

typedef struct
{
    COLLISION col;
    EFFECT *owner_effect;
} SCUBA_SHIP_TARGET;

typedef struct
{
    COLLISION col;
    MODEL_NODE nodes[5];
    MODEL_NODE *node_table[5];
    sint16 oscillation_step;
    sint16 oscillation;
    uint16 sink_ticks;
    EFFECT *owner_effect;
} SCUBA_TUG_TARGET;

typedef struct
{
    COLLISION col;
    MODEL_NODE nodes[8];
    MODEL_NODE *node_table[8];
    SCUBA_ROUTE body_route;
    SCUBA_ROUTE aim_route;
    SCUBA_VECTOR aim_pos;
    EFFECT *owner_effect;
    sint16 target_pitch;
    sint16 target_yaw;
    sint16 sound_handle;
    sint16 fire_countdown;
} SCUBA_SUBMARINE;

typedef struct
{
    COLLISION col;
    ANIM animation;
    EFFECT *route_points[20];
    VRAM_SPRITE vram_desc;
    EFFECT *owner_effect;
    FIXED_DDA_LINE path;
    sint16 route_idx;
    sint16 route_count;
    sint16 fire_countdown;
    sint16 route_dir;
    sint16 half_speed;
    sint32 route_speed;
    sint16 dir;
    sint16 desired_dir;
    sint16 prev_target_dir;
    sint16 animation_frame;
    MODEL_NODE nodes[6];
    MODEL_NODE *node_table[6];
    sint32 vertical_animation;
} SCUBA_DIVER;

typedef struct
{
    COLLISION col;
    MODEL_NODE node;
    sint32 velocity_y;
    sint32 rotation_step_y;
    sint32 rotation_step_x;
} SCUBA_DRIFTING_HAZARD;

typedef struct
{
    COLLISION col;
    EFFECT *owner_effect;
    sint16 phase;
    sint16 phase_step;
    sint16 proximity_ticks;
    sint32 vertical_animation;
} SCUBA_MINE;

typedef struct
{
    COLLISION col;
    sint16 model_id;
    sint16 fire_countdown;
} SCUBA_MODEL_TARGET;

typedef struct SCUBA_PLAYER
{
    PLAYER base;
    MISSION_HUD_BAR air_bar;
    sint16 animation_frame;
    SCUBA_VECTOR desired_velocity;
    SCUBA_VECTOR velocity;
    MODEL_NODE nodes[6];
    MODEL_NODE *node_table[6];
    sint16 animation_countdown;
    sint16 turn_flash_ticks;
    sint16 desired_dir;
    sint16 fire_latch;
    sint16 hit_turn_ticks;
} SCUBA_PLAYER;

/* Macros. */

/* Variables. */
static sint32 scuba_particle_count;

static MATRIX scuba_identity = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};

static sint32 scuba_type67_spawn_sequence;

static SCUBA_VECTOR scuba_route_target;

/* Functions. */
static void scuba_player_update(void *raw_player);

/* 0x800FADD4: bubble/explosion sprite constructor. */
/* Original: SCUBA_FUN_800fadd4. */
/* Original: SCUBA:SCUBA_800FADD4. */
static SPRITE *scuba_blast_spr_create(sint32 x, sint32 y, sint32 z)
{
    SPRITE *effect = world_spr_create(x, y, z, blast_animation);
    effect->light_delta = -0x10;
    effect->light_step = -2;
    effect->scale_x = (random_range(0x1000) + 0x1000);
    effect->force_light = 1;
    sound_play_positional(0x9c, 0, 0x7f, x, y, z);
    return effect;
}

/* 0x800FDE7C: callback used by the death emitter. */
/* Original: SCUBA_FUN_800fde7c. */
/* Original: SCUBA:SCUBA_800FDE7C. */
void *scuba_death_particle_emit(void *raw_emitter)
{
    TIMED_CALLBACK *emitter = (TIMED_CALLBACK *)raw_emitter;
    sint32 x = emitter->col.x + ((random_range(0xc0) - 0x60u) * 0x100);
    sint32 y = emitter->col.y + ((random_range(0x80) - 0x40u) * 0x100);
    return scuba_blast_spr_create(x, y, (emitter->col.z - random_range(0x20) * 0x100u));
}

/* 0x800FBE08..0x800FBF30: damage/death accounting. */
/* Original: SCUBA_FUN_800fbe08. */
/* Original: SCUBA:SCUBA_800FBE08. */
static void scuba_player_damage_apply(void *raw_hit, void *raw_player)
{
    COLLISION *hit = (COLLISION *)raw_hit;
    SCUBA_PLAYER *player = (SCUBA_PLAYER *)raw_player;
    if (g_invulnerability_cheat_enabled != 0)
        return;
    if (hit != 0)
    {
        sint16 damage = hit->hit.damage;
        if (player->base.damage_cooldown != 0)
            return;
        sound_play_positional(0xa6, 0, 0xc8, player->base.col.x, player->base.col.y, player->base.col.z);
        if (damage == 0)
            player->base.health -= 0x80;
        else
        {
            if (hit->object_type == 2)
                player->base.health -= 0xaa;
            player->base.health -= damage;
        }
        player->base.damage_cooldown = 0x78;
        player->hit_turn_ticks = 0x32;
    }
    if (player->base.health <= 0)
    {
        player->base.health = 0;
        timed_callback_create(scuba_death_particle_emit, player->base.col.x, player->base.col.y, player->base.col.z, 0x28, 2);
        player_death_begin();
    }
}

/* 0x800FD538: hit conversion into the common blast/effect objects. */
/* Original: SCUBA_FUN_800fd538. */
/* Original: SCUBA:SCUBA_800FD538. */
void scuba_blast_effect_create(COLLISION *src)
{
    SPRITE *blast = expl_flash_create(src->x, src->y, src->z);
    SPRITE *effect;
    blast->animation.start = blast_impact_animation;
    blast->animation.next = blast_impact_animation;
    blast->light_step = -8;
    blast->rotation_step_x = 8;
    blast->scale_step = 0x200;
    effect = world_spr_create(src->x, src->y, src->z, blast_animation);
    effect->scale_x = 0x2000;
    effect->col.player_contact_callback = scuba_player_damage_apply;
    if (src->object_type != 0x79)
        effect->col.receives_mask = 2;
    else
        effect->retain_offscreen = 1;
    col_box_set(effect, 0x80, 0x80, 0xc0);
    effect->col.box_y /= 2;
    sound_play_positional(0xa1, 4, 0x96, src->x, src->y, src->z);
    disp_shake_start();
    if (src->object_type != 0x67)
    {
        effect->col.object_type = 2;
        effect->col.hit.damage = 4;
        effect->col.receives_mask |= 4;
        effect->col.sends_mask |= 1;
    }
}

/* Original: SCUBA_FUN_800fd960. */
/* Original: SCUBA:SCUBA_800FD960. */
static void scuba_target_damage(void *raw_object, void *raw_hit)
{
    COLLISION *object = (COLLISION *)raw_object;
    COLLISION *hit = (COLLISION *)raw_hit;
    object->flash_clut_ticks = 1;
    object->hit.health = (object->hit.health - hit->hit.damage);
    if (hit->object_type == 0x64)
        hit->hit.damage = 0;
    sound_play_positional(0xa2, 6, 0x7f, object->x, object->y, object->z);
    if (object->hit.health <= 0)
    {
        scuba_blast_effect_create(object);
        object_destroy(object);
    }
}

/* Exact 0x800FEE18..0x800FEE68: shared damage plus objective counter 10. */
/* Original: SCUBA_FUN_800fee18. */
/* Original: SCUBA:SCUBA_800FEE18. */
static void scuba_ship_target_damage(void *raw_object, void *raw_hit)
{
    SCUBA_SHIP_TARGET *object = (SCUBA_SHIP_TARGET *)raw_object;
    COLLISION *hit = (COLLISION *)raw_hit;
    scuba_target_damage(&object->col, hit);
    if (object->col.hit.health <= 0)
        --g_mission.counts[10];
}

/* Exact 0x800FED94..0x800FEE18: visible ship-target sprite/update. */
/* Original: SCUBA_FUN_800fed94. */
/* Original: SCUBA:SCUBA_800FED94. */
static void scuba_ship_target_update(void *raw_object)
{
    SCUBA_SHIP_TARGET *object = (SCUBA_SHIP_TARGET *)raw_object;
    if (world_object_is_visible(object->col.x, object->col.y, object->col.z, 0xc800))
    {
        render_world_spr(0x2dc00, object->col.x, object->col.y, object->col.z, 0, 0x1000, 0x1000, 0, 0, 2, 0, 0);
        object_hit_flash_apply(object);
    }
}

/* Exact 0x800FECE4..0x800FED94: type-0x73 ship target constructor. */
/* Original: SCUBA_FUN_800fece4. */
/* Original: SCUBA:SCUBA_800FECE4. */
void scuba_ship_target_create(EFFECT *effect)
{
    SCUBA_SHIP_TARGET *object = (SCUBA_SHIP_TARGET *)object_create(sizeof(*object), scuba_ship_target_update);
    if (object == 0)
        return;
    object->col.x = effect->x;
    object->col.y = effect->y + 0x2000;
    object->col.z = effect->z + 0x2100;
    object->col.hit.health = 0x14;
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = scuba_ship_target_damage;
    col_box_set(object, 0x40, 0x80, 0x20);
    object->col.box_y /= 2;
    effect->type = 0;
    object->owner_effect = effect;
}

/* Exact 0x800FDB64..0x800FDDC8: tug target update/render controller. */
/* Original: SCUBA_FUN_800fdb64. */
/* Original: SCUBA:SCUBA_800FDB64. */
static void scuba_tug_target_update(void *raw_o)
{
    SCUBA_TUG_TARGET *o = (SCUBA_TUG_TARGET *)raw_o;
    MODEL_NODE *node = &o->nodes[0];
    if (!world_object_is_visible(o->col.x, o->col.y, o->col.z, 0x15e00))
        return;
    if (o->sink_ticks != 0)
    {
        if (o->sink_ticks == 0x64)
            timed_callback_create(scuba_death_particle_emit, o->col.x, o->col.y, o->col.z, 0x28, 2);
        if (o->sink_ticks == 0x7d)
            scuba_blast_effect_create(&o->col);
        if (o->sink_ticks >= 0x82)
        {
            object_destroy(o);
            return;
        }
        ++o->sink_ticks;
        o->col.y += 0x200;
        node->rotation_x -= 3;
        node->rotation_z += 2;
    }
    else
    {
        o->oscillation += o->oscillation_step;
        if (o->oscillation >= 0x31)
            o->oscillation_step = -1;
        if (o->oscillation < -0x30)
            o->oscillation_step = 1;
        node->rotation_x = o->oscillation;
        node->rotation_z = o->oscillation;
    }
    node->rotation_x = (node->rotation_x & 0xfff);
    node->rotation_z = (node->rotation_z & 0xfff);
    node->translation_x = o->col.x - g_camera.world_x;
    node->translation_y = o->col.y - g_camera.world_y;
    node->translation_z = o->col.z - g_camera.world_z;
    g_model_draw.depth_bucket = 0x480 - (g_map_render.depth_anchor_z - o->col.z) / 0x100;
    g_model_draw.world_z = o->col.z;
    if (o->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --o->col.flash_clut_ticks;
    }
    g_model_draw.target_frame = g_cur_render_frame;
    model_render_node(node, &g_model_identity);
    g_model_draw.clut_override = 0;
    hierarchy_col_box_update(&o->col, &o->nodes[1], 4);
}

/* Exact 0x800FDDC8..0x800FDE7C: tug target damage/counter transition. */
/* Original: SCUBA_FUN_800fddc8. */
/* Original: SCUBA:SCUBA_800FDDC8. */
static void scuba_tug_target_damage(void *raw_o, void *raw_hit)
{
    SCUBA_TUG_TARGET *o = (SCUBA_TUG_TARGET *)raw_o;
    COLLISION *hit = (COLLISION *)raw_hit;
    if (o->sink_ticks != 0)
        return;
    o->col.flash_clut_ticks = 1;
    o->col.hit.health -= hit->hit.damage;
    scuba_blast_spr_create(hit->x, hit->y, hit->z);
    if (hit->object_type == 0x64)
        hit->hit.damage = 0;
    if (o->col.hit.health <= 0)
    {
        o->sink_ticks = 1;
        --g_mission.counts[9];
    }
}

/* Exact 0x800FD9F8..0x800FDB64: type-0x6D tug target constructor. */
/* Original: SCUBA_FUN_800fd9f8. */
/* Original: SCUBA:SCUBA_800FD9F8. */
void scuba_tug_target_create(EFFECT *effect)
{
    SCUBA_TUG_TARGET *object = (SCUBA_TUG_TARGET *)object_create(sizeof(*object), scuba_tug_target_update);
    const sint16 *idxs = tug_model_offsets;
    sint32 n;
    effect->type = 0;
    if (object == 0)
        return;
    for (n = 0; n < 5; n++)
    {
        MODEL_NODE *node = &object->nodes[n];
        object->node_table[n] = node;
        node->model_id = idxs[n] == -1 ? 0 : g_scuba_tug_model_base + idxs[n];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_scuba_tug_initial_pose, object->node_table, 5, 1);
    object->nodes[0].scale = 0x1c00;
    object->col.x = effect->x;
    object->col.y = effect->y;
    object->col.z = effect->z + 0x4000;
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = scuba_tug_target_damage;
    object->col.hit.health = 0x3c;
    object->oscillation_step = 2;
    object->owner_effect = effect;
    object->nodes[0].rotation_x = 0;
    object->nodes[0].rotation_z = 0;
    object->nodes[0].rotation_y = (random_range(0x200) + 0x300);
}

/* Exact 0x800FC690..0x800FC784: calculate a 1/256 route step and advance. */
/* Original: SCUBA_FUN_800fc690. */
/* Original: SCUBA:SCUBA_800FC690. */
static void scuba_route_step_advance(SCUBA_VECTOR *pos, const SCUBA_VECTOR *target, SCUBA_VECTOR *step)
{
    FIXED_DDA_LINE line;
    fixed_dda_initialize(pos->x, pos->y, pos->z, target->x, target->y, target->z, &line);
    step->x = line.step.x / 0x100;
    step->y = line.step.y / 0x100;
    step->z = line.step.z / 0x100;
    pos->x += step->x;
    pos->y += step->y;
    pos->z += step->z;
}

/* Exact 0x800FC784..0x800FC7C4: initialize an effect-defined route. */
/* Original: SCUBA_FUN_800fc784. */
/* Original: SCUBA:SCUBA_800FC784. */
static void scuba_route_initialize(SCUBA_ROUTE *route, EFFECT *effect)
{
    route->count = route_waypoints_collect(effect->vals[0], -1, route->points, sizeof(route->points) / sizeof(route->points[0]));
    route->dir = 1;
}

/* Exact 0x800FC7C4..0x800FC8E4: choose/advance the current waypoint. */
/* Original: SCUBA_FUN_800fc7c4. */
/* Original: SCUBA:SCUBA_800FC7C4. */
static SCUBA_VECTOR *scuba_route_waypoint_advance(SCUBA_ROUTE *route, const SCUBA_VECTOR *pos)
{
    EFFECT *point = route->points[route->idx];
    SCUBA_VECTOR *target = &scuba_route_target;
    target->x = point->x;
    target->y = point->y;
    target->z = point->z;
    if (target->x - 0x1000 < pos->x && pos->x < target->x + 0x1000 && target->y - 0x1000 < pos->y && pos->y < target->y + 0x1000 && target->z - 0x1000 < pos->z && pos->z < target->z + 0x1000)
    {
        sint16 idx = route->idx + route->dir;
        route->idx = idx;
        if (idx == route->count)
            route->idx = 0;
        if (route->idx < 0)
            route->idx = (route->count - 1);
        point = route->points[route->idx];
        target->x = point->x;
        target->y = point->y;
        target->z = point->z;
    }
    return target;
}

/* Exact 0x800FC8E4..0x800FC920: collision probe without inflicting damage. */
/* Original: SCUBA_FUN_800fc8e4. */
/* Original: SCUBA:SCUBA_800FC8E4. */
static void scuba_player_col_probe(void *raw_hit, void *raw_player)
{
    COLLISION *hit = (COLLISION *)raw_hit;
    SCUBA_PLAYER *player = (SCUBA_PLAYER *)raw_player;
    sint16 damage = hit->hit.damage;
    hit->hit.damage = 0;
    scuba_player_damage_apply(hit, player);
    hit->hit.damage = damage;
}

/* 0x800FBF30 / 0x800FC230 / 0x800FC644. */
/* Original: SCUBA_FUN_800fbf30. */
/* Original: SCUBA:SCUBA_800FBF30. */
static void scuba_projectile_trail_update(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint16 val = object->rotation_step_x - 3;
    object->rotation_step_x = val < 8 ? 8 : val;
    if (object->light_delta < -0x7f)
        object->animation.next = 0;
    if (object->light_delta < -0x40)
        object->light_step = -2;
}

/* Original: SCUBA_FUN_800fc230. */
/* Original: SCUBA:SCUBA_800FC230. */
static void scuba_spr_animation_stop(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    object->animation.next = 0;
}

/* 0x800FBF90: fired underwater projectile callback. */
/* Original: SCUBA_FUN_800fbf90. */
/* Original: SCUBA:SCUBA_800FBF90. */
static void scuba_projectile_update(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint32 mask = object->col.object_type == 0x64 ? 3 : 7;
    sint32 floor_y;
    if (object->col.hit.damage <= 0)
    {
        object->animation.next = 0;
        return;
    }
    if ((g_render_state.frame & mask) == 0)
    {
        SPRITE *trail = world_spr_create(object->col.x, object->col.y, object->col.z, projectile_trail_animation);
        sint32 v;
        trail->scale_x = 0x200;
        trail->scale_step = 0xaa;
        trail->frame_callback = scuba_projectile_trail_update;
        trail->rotation_step_x = 0x55;
        trail->light_step = -4;
        trail->force_light = 1;
        trail->rotation_x = ((g_render_state.frame << 6) & 0x3ff);
        trail->velocity_x = object->velocity_x;
        trail->velocity_y = object->velocity_y;
        trail->velocity_z = object->velocity_z;
        v = object->velocity_x;
        object->velocity_x = v + (v < 0 ? (v + 3) >> 2 : v >> 2);
        v = object->velocity_y;
        object->velocity_y = v + (v < 0 ? (v + 3) >> 2 : v >> 2);
        v = object->velocity_z;
        object->velocity_z = v + (v < 0 ? (v + 3) >> 2 : v >> 2);
    }
    if (object->behavior.projectile.target_x != 0 || object->behavior.projectile.target_y != 0 || object->behavior.projectile.target_z != 0)
    {
        if ((g_render_state.frame & 3) == 0)
        {
            sint32 v = object->velocity_x;
            object->velocity_x = v - (v < 0 ? (v + 3) >> 2 : v >> 2);
            v = object->velocity_y;
            object->velocity_y = v - (v < 0 ? (v + 3) >> 2 : v >> 2);
            v = object->velocity_z;
            object->velocity_z = v - (v < 0 ? (v + 3) >> 2 : v >> 2);
        }
        homing_projectile_steer(object);
    }
    floor_y = map_floor_height_at(object->col.x, object->col.y, object->col.z);
    if (swept_col_test(object->col.x - object->velocity_x, object->col.y - object->velocity_y, object->col.z - object->velocity_z, &object->col.x, &object->col.y, &object->col.z) != 0 || floor_y < object->col.y)
    {
        object->frame_callback = scuba_spr_animation_stop;
        scuba_blast_spr_create(object->col.x, object->col.y - 0x1800, object->col.z);
    }
}

/* 0x800FC398: homing correction applied after the generic projectile path. */
/* Original: SCUBA_FUN_800fc398. */
/* Original: SCUBA:SCUBA_800FC398. */
static void scuba_homing_projectile_update(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint32 vx = object->velocity_x, vy = object->velocity_y, vz = object->velocity_z;
    scuba_projectile_update(object);
    object->velocity_x = vx;
    object->velocity_y = vy;
    object->velocity_z = vz;
    if (object->col.x < g_player_world.x && object->velocity_x < 0x300)
        object->velocity_x += 0x10;
    if (g_player_world.x < object->col.x && object->velocity_x >= -0x2ff)
        object->velocity_x -= 0x10;
    if (object->col.y < g_player_world.y && object->velocity_y < 0x300)
        object->velocity_y += 0x10;
    if (g_player_world.y < object->col.y && object->velocity_y >= -0x2ff)
        object->velocity_y -= 0x10;
    if (object->col.z < g_player_world.z && object->velocity_z < 0x300)
        object->velocity_z += 0x10;
    if (g_player_world.z < object->col.z && object->velocity_z >= -0x2ff)
        object->velocity_z -= 0x10;
}

/* 0x800FC238: aimed projectile constructor. */
/* Original: SCUBA_FUN_800fc238. */
/* Original: SCUBA:SCUBA_800FC238. */
SPRITE *scuba_projectile_create(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, sint32 lifetime)
{
    FIXED_DDA_LINE line;
    sint32 val;
    SPRITE *object;
    fixed_dda_initialize(x0, y0, z0, x1, y1, z1, &line);
    object = world_spr_create(x0, y0, z0, projectile_animation);
    object->light_delta = -0x18;
    object->force_light = 1;
    object->velocity_x = line.step.x / 0x100;
    object->velocity_y = line.step.y / 0x100;
    val = line.step.z;
    if (val < 0)
        val += 0xff;
    object->velocity_z = val >> 8;
    object->frame_callback = scuba_homing_projectile_update;
    object->col.receives_mask = 2;
    object->rotation_step_x = 0x66;
    object->lifetime = lifetime;
    object->col.hit.damage = 0x66;
    object->col.player_contact_callback = scuba_player_damage_apply;
    col_box_set(object, 0x10, 0x10, 0x10);
    sound_play_positional(0x9e, 0, 0x7f, x1, y1, z1);
    return object;
}

/* Exact 0x800FCBA4..0x800FD04C: routed submarine update, aim and fire. */
/* Original: SCUBA_FUN_800fcba4. */
/* Original: SCUBA:SCUBA_800FCBA4. */
static void scuba_submarine_update(void *raw_object)
{
    SCUBA_SUBMARINE *object = (SCUBA_SUBMARINE *)raw_object;
    SPRITE *shot;
    FIXED_DDA_LINE line;
    sint32 vx, vy, vz, horizontal;
    const SOUND_VOLUME_PAIR *volume;
    if (world_object_is_visible(object->col.x, object->col.y, object->col.z, 0x13800) == 0)
    {
        object->owner_effect->type = 0x65;
        sound_voice_stop(&object->sound_handle);
        object_destroy(object);
        return;
    }
    if (g_sound_handles_invalidated != 0)
        object->sound_handle = -1;
    if (object->sound_handle == -1)
        object->sound_handle = sound_play_positional(0xa0, 3, 0xa0, object->col.x, object->col.y, object->col.z);
    volume = sound_spatial_volume_calculate(0xa0, object->col.x, object->col.y, object->col.z);
    if (volume != 0)
        sound_voice_spatial_volume_update(volume, object->sound_handle);
    object->nodes[3].rotation_z = ((g_render_state.frame << 8) & 0xfff);
    scuba_route_step_advance((SCUBA_VECTOR *)&object->col.x, scuba_route_waypoint_advance(&object->body_route, (SCUBA_VECTOR *)&object->col.x), &object->body_route.step);
    scuba_route_step_advance(&object->aim_pos, scuba_route_waypoint_advance(&object->aim_route, &object->aim_pos), &object->aim_route.step);
    fixed_dda_initialize(object->col.x, object->col.y, object->col.z, object->aim_pos.x, object->aim_pos.y, object->aim_pos.z, &line);
    vx = line.step.x / 0x600;
    vy = line.step.y / 0x600;
    vz = line.step.z / 0x600;
    object->target_yaw = (fixed_angle_from_vector(vx, vz) * 4);
    horizontal = (abs(vx) < abs(vz)) ? abs(vz) + abs(vx) / 2 : abs(vx) + abs(vz) / 2;
    object->target_pitch = ((-fixed_angle_from_vector(vy, horizontal) * 4) & 0xfff);
    object->nodes[0].rotation_y = angle_approach_wrapped(object->target_yaw, 0x40, object->nodes[0].rotation_y);
    object->nodes[0].rotation_x = angle_approach_wrapped(object->target_pitch, 0x40, object->nodes[0].rotation_x);
    object->nodes[0].translation_x = object->col.x - g_camera.world_x;
    object->nodes[0].translation_y = object->col.y - g_camera.world_y;
    object->nodes[0].translation_z = object->col.z - g_camera.world_z;
    vx = g_map_render.depth_anchor_z - object->col.z;
    g_model_draw.depth_bucket = 0x480 - (vx < 0 ? (vx + 0xff) >> 8 : vx >> 8);
    g_model_draw.world_z = object->col.z;
    if (object->col.flash_clut_ticks != 0)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --object->col.flash_clut_ticks;
    }
    g_model_draw.target_frame = g_cur_render_frame;
    model_render_node(&object->nodes[0], &scuba_identity);
    hierarchy_col_box_update(&object->col, &object->nodes[4], 4);
    g_model_draw.clut_override = 0;
    if (object->fire_countdown == 0)
    {
        vx = player_angle_from_point(object->col.x, object->col.z, 1) * 4 - object->nodes[0].rotation_y;
        if (vx < 0)
            vx = -vx;
        if (vx < 0x200)
        {
            object->fire_countdown = (random_range(0x1f) + 0x28);
            shot = scuba_projectile_create(object->nodes[3].world_x, object->nodes[3].world_y + 0x1000, object->nodes[3].world_z, object->col.x, object->col.y + 0x1000, object->col.z, 0x78);
            shot->col.x += shot->velocity_x * 0x8c;
            shot->col.y += shot->velocity_y * 0x8c;
            shot->col.z += shot->velocity_z * 0x8c;
        }
    }
    else
        --object->fire_countdown;
}

/* Exact 0x800FD04C..0x800FD108: large diver-enemy hit/death callback. */
/* Original: SCUBA_FUN_800fd04c. */
/* Original: SCUBA:SCUBA_800FD04C. */
static void scuba_submarine_damage(void *raw_object, void *raw_hit)
{
    SCUBA_SUBMARINE *object = (SCUBA_SUBMARINE *)raw_object;
    COLLISION *hit = (COLLISION *)raw_hit;
    object->col.hit.health = (object->col.hit.health - hit->hit.damage);
    if (hit->object_type != 2)
    {
        scuba_blast_spr_create(object->col.x, object->col.y, object->col.z);
        hit->hit.damage = 0;
    }
    object->col.flash_clut_ticks = 1;
    if (object->col.hit.health <= 0)
    {
        sound_voice_stop(&object->sound_handle);
        timed_callback_create(scuba_death_particle_emit, object->col.x, object->col.y, object->col.z, 0x28, 2);
        object_destroy(object);
    }
}

/* Exact 0x800FC920..0x800FCBA4: type-0x65 routed submarine constructor. */
/* Original: SCUBA_FUN_800fc920. */
/* Original: SCUBA:SCUBA_800FC920. */
void scuba_submarine_create(EFFECT *effect)
{
    static const sint8 models[8] = {3, 2, 0, 1, -1, -1, -1, -1};
    SCUBA_SUBMARINE *object;
    sint32 n;
    effect->type = 0;
    object = (SCUBA_SUBMARINE *)runtime_heap_allocate(sizeof(*object));
    object->col.object.update = scuba_submarine_update;
    linked_list_append(&g_object_list, object);
    object->col.object_type = 0x65;
    for (n = 0; n < 8; n++)
    {
        MODEL_NODE *node = &object->nodes[n];
        object->node_table[n] = node;
        node->model_id = models[n] < 0 ? 0 : g_mini_sub_model_base + models[n];
        node->pose_enabled = 0x3f;
    }
    object->nodes[0].scale = 0x0d55;
    model_pose_init(&g_mini_sub_initial_pose, object->node_table, 8, 1);
    object->owner_effect = effect;
    object->col.z = g_player_world.z + 0x5000;
    if (random_range(1) != 0)
    {
        object->col.y = g_player_world.y;
        object->col.x = ((object->col.z - g_camera.world_z) * 200) / 320 + g_camera.world_x;
    }
    else
    {
        object->col.x = g_camera.world_x;
        object->col.y = -((object->col.z - g_camera.world_z) * 150) / 355 + g_camera.world_y;
    }
    object->col.x = object->aim_pos.x = effect->x;
    object->col.y = object->aim_pos.y = effect->y;
    object->col.z = object->aim_pos.z = effect->z;
    object->col.receives_mask = 3;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = scuba_submarine_damage;
    object->col.player_contact_callback = scuba_player_col_probe;
    object->col.hit.health = 0x0f;
    scuba_route_initialize(&object->body_route, effect);
    scuba_route_initialize(&object->aim_route, effect);
    for (n = 0; n < 0xc0; n++)
        scuba_route_step_advance(&object->aim_pos, scuba_route_waypoint_advance(&object->aim_route, &object->aim_pos), &object->aim_route.step);
    object->sound_handle = -1;
}

/* 0x800FDF54: small underwater particle. Incoming a0/a1/a2 are forwarded
 * unchanged to FUN_80090758 by the original instructions. */
/* Original: SCUBA_FUN_800fdf54. */
/* Original: SCUBA:SCUBA_800FDF54. */
static SPRITE *scuba_particle_create(sint32 x, sint32 y, sint32 z)
{
    SPRITE *effect = world_spr_create(x, y, z, particle_animation);
    effect->light_delta = -0x18;
    effect->light_step = -2;
    effect->scale_x = 1;
    effect->scale_step = 0x80;
    effect->velocity_x = random_range(0x100) - 0x80;
    effect->velocity_y = (~random_range(2)) << 8;
    effect->acceleration_y = -0x10;
    return effect;
}

/* Exact 0x800FE1A8..0x800FE74C: routed diver update/render/fire logic. */
/* Original: SCUBA_FUN_800fe1a8. */
/* Original: SCUBA:SCUBA_800FE1A8. */
static void scuba_diver_enemy_update(void *raw_object)
{
    SCUBA_DIVER *object = (SCUBA_DIVER *)raw_object;
    static const uint8 frames[8] = {0x18, 0x12, 0, 0x0c, 6, 0x0c, 0, 0x12};
    EFFECT *point;
    sint32 state;
    state = mission_enemy_update_visibility(&object->col, object->owner_effect, object->col.object_type, g_spr_pal.cluts[CLUT_SCUBA_DIVER]);
    sint32 target_x, target_y, target_z, delta, step, frame;
    if (state == 0)
        return;
    point = object->route_points[object->route_idx];
    target_x = point->x;
    target_y = point->y;
    target_z = point->z;
    if (target_x - 0x1000 < object->col.x && object->col.x < target_x + 0x1000 && target_z - 0x1000 < object->col.z && object->col.z < target_z + 0x1000 && target_y - 0x1000 < object->col.y && object->col.y < target_y + 0x1000)
    {
        sint16 idx = object->route_idx + object->route_dir;
        object->route_idx = idx;
        if (idx == object->route_count - 1)
            object->route_dir = -1;
        if (object->route_idx == 0)
            object->route_dir = 1;
        point = object->route_points[object->route_idx];
        target_x = point->x;
        target_y = point->y;
        target_z = point->z;
        fixed_dda_initialize(object->col.x, object->col.y, object->col.z, target_x, target_y, target_z, &object->path);
        object->path.step.x *= object->route_speed;
        object->path.step.y *= object->route_speed;
        object->path.step.z *= object->route_speed;
        if (object->half_speed != 0)
        {
            object->path.step.x /= 2;
            object->path.step.y /= 2;
            object->path.step.z /= 2;
        }
        delta = target_x - object->col.x;
        if (delta < 0)
            delta += 0xfff;
        step = target_z - object->col.z;
        if (step < 0)
            step += 0xfff;
        object->desired_dir = ((fixed_angle_from_vector(delta >> 12, step >> 12) & 0x3ff) / 0x80);
    }
    if (object->fire_countdown != 0)
        --object->fire_countdown;
    else
    {
        object->fire_countdown = (random_range(0xc8) + 0x12c);
        object->desired_dir = object->prev_target_dir;
    }
    if (object->fire_countdown >= 0x65)
    {
        fixed_dda_advance(&object->path);
        object->prev_target_dir = object->desired_dir;
    }
    else
        object->desired_dir = player_angle_from_point(object->col.x, object->col.z, 0x80);
    object->col.x = object->path.pos.x / 0x100;
    object->col.y = object->path.pos.y / 0x100;
    object->col.z = object->path.pos.z / 0x100;
    if ((g_render_state.frame & 3) == 0 && object->dir != object->desired_dir)
    {
        sint16 cur = object->dir, target = object->desired_dir;
        step = target < cur ? -1 : 1;
        delta = cur - target;
        if (delta < 0)
            delta = -delta;
        if (delta >= 4)
            step = -step;
        object->dir = ((cur + step) & 7);
    }
    if ((g_render_state.frame & 7) == 0)
        ++object->animation_frame;
    if (object->animation_frame >= 6)
        object->animation_frame = 0;
    object->nodes[1].scale = 0xa3;
    object->nodes[1].rotation_y = ((object->dir << 9) & 0xfff);
    object->nodes[1].translation_x = object->col.x - g_camera.world_x;
    object->nodes[1].translation_y = object->col.y - (g_camera.world_y + 0xc00);
    object->nodes[1].translation_z = object->col.z - g_camera.world_z;
    g_model_draw.target_frame = 0;
    model_render_node(&object->nodes[1], &scuba_identity);
    if (object->vram_desc.region != 0 && state != 2)
    {
        frame = 0x2d400 + frames[object->dir] + object->animation_frame;
        g_next_frame_object->light_delta = 0;
        render_world_spr(frame, object->col.x, object->col.y + player_bob_offset_update(&object->vertical_animation), object->col.z, object->dir < 4, 0x1000, 0x1000, &object->col.prim, 0, 2, &object->vram_desc, 0);
        object_embedded_hit_flash_apply(object);
        if ((g_render_state.frame & 0x40) != 0)
        {
            if ((g_render_state.frame & 0x3f) == 0)
                sound_play_positional(random_range(3) + 0x98, 0, 0x7f, object->col.x, object->col.y, object->col.z);
            if ((g_render_state.frame & 3) == 0 && (g_render_state.frame & 0x3f) < 0x10)
                scuba_particle_create(object->nodes[3].world_x, object->nodes[3].world_y, object->nodes[3].world_z);
        }
    }
    if (object->fire_countdown == 0x14)
        scuba_projectile_create(object->nodes[4].world_x, object->nodes[4].world_y, object->nodes[4].world_z, object->nodes[2].world_x, object->nodes[2].world_y, object->nodes[2].world_z, 0x64);
}

/* Exact 0x800FDFD4..0x800FE1A8: type-0x6E routed diver constructor. */
/* Original: SCUBA_FUN_800fdfd4. */
/* Original: SCUBA:SCUBA_800FDFD4. */
void scuba_diver_enemy_create(EFFECT *effect)
{
    SCUBA_DIVER *object;
    sint32 n;
    sint16 type = effect->type;
    if (g_mission.enemy_updates >= 6)
        speech_random_request(5);
    object = (SCUBA_DIVER *)object_create(sizeof(*object), scuba_diver_enemy_update);
    setPolyFT4(&object->col.prim);
    setShadeTex(&object->col.prim, 0);
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_OBJECT;
    object->col.hit_object_callback = destroyable_object_damage_and_reward;
    col_box_set(object, 0x64, 0x30, 0x30);
    object->col.hit.health = 4;
    object->col.object_type = type;
    object->col.frame_desc = &object->vram_desc;
    object->animation.owner = object;
    object->col.y = effect->y;
    col_box_set(object, 0x40, 0x40, 0x10);
    object->route_points[0] = effect;
    object->owner_effect = effect;
    effect->type = 0;
    object->route_count = (route_waypoints_collect(effect->vals[0], 0, object->route_points, sizeof(object->route_points) / sizeof(object->route_points[0])) + 1);
    object->route_dir = 1;
    object->fire_countdown = 0x15;
    if (type != 0x44)
    {
        object->col.x = effect->x;
        object->col.z = effect->z;
    }
    object->route_speed = 1;
    for (n = 0; n < 6; n++)
    {
        object->node_table[n] = &object->nodes[n];
        object->nodes[n].pose_enabled = 0x3f;
    }
    model_pose_init(&initial_pose, object->node_table, 6, 1);
}

/* Original: SCUBA_FUN_800fc644. */
/* Original: SCUBA:SCUBA_800FC644. */
static void scuba_distant_water_particle_update(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint16 val = (uint16)object->light_delta + object->behavior.water_light_step;
    object->light_delta = val;
    if (val >= -0x59)
        object->behavior.water_light_step = -2;
    if (object->light_delta < -0x7f)
        object->animation.next = 0;
}

/* Exact 0x800FDF00..0x800FDF54: type-0x70 periodic bubble emitter. */
/* Original: SCUBA_FUN_800fdf00. */
/* Original: SCUBA:SCUBA_800FDF00. */
void scuba_bubble_emitter_update(EFFECT *effect)
{
    sint16 delay;
    delay = effect->vals[2];
    if (delay != 0)
    {
        effect->vals[2] = (delay - 1);
        return;
    }
    effect->vals[2] = effect->vals[1];
    scuba_particle_create(effect->x, effect->y + 0x4000, effect->z);
}

/* Exact 0x800FE74C..0x800FE75C: deliberate type-0x6F no-op callback. */
/* Original: SCUBA_FUN_800fe74c. */
/* Original: SCUBA:SCUBA_800FE74C. */
void scuba_noop_effect_update(EFFECT *effect)
{
}

/* 0x800FC528: periodic distant water particle. */
/* Original: SCUBA_FUN_800fc528. */
/* Original: SCUBA:SCUBA_800FC528. */
static void scuba_distant_water_particle_create(void)
{
    const uint32 depth = (random_range(0x100) + 0x12cu) << 8;
    sint32 z = g_camera.world_z + depth;
    sint32 x = random_range(0x140) * depth / 320 + g_camera.world_x - 0xa000;
    SPRITE *effect;
    if (g_camera.prev_x < g_camera.world_x)
        x = (x + 0x6000u);
    if (g_camera.world_x < g_camera.prev_x)
        x = (x - 0x6000u);
    effect = world_spr_create(x, g_camera.world_y, z, water_particle_animation);
    effect->light_delta = -0x7f;
    effect->scale_x = (random_range(0x1000) + 0x1000);
    effect->scale_y = 0x7000;
    effect->rotation_x = ((random_range(0x40) - 0x20) & 0x3ff);
    effect->frame_callback = scuba_distant_water_particle_update;
    effect->behavior.water_light_step = 1;
    effect->force_light = 1;
}

/* 0x800FEEF4: remove type 0x71 during room 0x14 and progressively move the
 * original 3x58-cell strip. */
/* Original: SCUBA_FUN_800feef4. */
/* Original: SCUBA:SCUBA_800FEEF4. */
static void scuba_wreck_strip_update(void)
{
    if (g_session.stage == 0x14 && g_mission.counts[0x0a] == 0)
    {
        void *object = 0;
        while ((object = object_find_next_by_type(object, 0x71)) != 0)
            object_destroy(object);
        if (scuba_particle_count < 0x12c)
        {
            building_map_deform(0x120000, 0xe8000, 0x3a, 3, 2, -0x40, -0x800);
            scuba_particle_count++;
        }
    }
}

/* Original: SCUBA_FUN_800fee68. */
/* Original: SCUBA:SCUBA_800FEE68. */
static void scuba_bubble_trail_update(void *raw_object)
{
    COLLISION *object = (COLLISION *)raw_object;
    if ((g_render_state.frame & 7) == 0)
    {
        SPRITE *effect = scuba_particle_create(object->x, object->y, object->z);
        if (effect != 0)
            effect->acceleration_y = -8;
    }
}

/* Original: SCUBA_FUN_800feeb8. */
/* Original: SCUBA:SCUBA_800FEEB8. */
void scuba_mission_complete_player_update(void *raw_player)
{
    SCUBA_PLAYER *player = (SCUBA_PLAYER *)raw_player;
    player_mission_complete_update(&player->base);
    scuba_player_update(player);
    g_camera.frozen = 1;
}

/* 0x800FB054..0x800FBE08: exact primary diver update. */
/* Original: SCUBA_FUN_800fb054. */
/* Original: SCUBA:SCUBA_800FB054. */
static void scuba_player_update(void *raw_player)
{
    SCUBA_PLAYER *player = (SCUBA_PLAYER *)raw_player;
    sint32 old_x, old_y, old_z, input, frame_base;
    uint16 raw_dir;
    FIXED_DDA_LINE line;
    sint32 desired, actual, limit, motion;
    sint16 dir;
    scuba_wreck_strip_update();
    old_x = player->base.col.x;
    old_y = player->base.col.y;
    old_z = player->base.col.z;
    col_box_set(player, 0x60, 0x20, 6);
    input = g_input.held;
    raw_dir = input & 0xf000;
    if (player->base.col.object.update == scuba_mission_complete_player_update)
        input = 8;
    player->desired_velocity.x = player->desired_velocity.y = player->desired_velocity.z = 0;
    player->base.prev_dir = player->base.dir;
    player->desired_dir = player_animation_start(raw_dir);
    if (player->hit_turn_ticks != 0)
    {
        player->desired_dir = ((g_render_state.frame >> 1) & 7);
        --player->hit_turn_ticks;
    }
    if ((g_render_state.frame & 1) == 0)
    {
        sint16 cur = player->base.dir, target = player->desired_dir;
        if (cur != target)
        {
            sint32 turn = target < cur ? -1 : 1;
            desired = cur - target;
            if (desired < 0)
                desired = -desired;
            if (desired >= 4)
                turn = -turn;
            player->base.dir = ((cur + turn) & 7);
        }
    }
    player->nodes[1].rotation_x = 0x800;
    dir = player->base.dir;
    frame_base = dir_frames[dir] + 0x2b800;
    if ((input & 8) != 0)
    {
        frame_base += 0xc;
        player->desired_velocity.y = -0x200;
        player->nodes[1].rotation_x = 0x880;
    }
    else if ((input & 2) != 0)
    {
        frame_base += 0x18;
        player->desired_velocity.y = 0x200;
        player->nodes[1].rotation_x = 0x780;
    }
    if ((raw_dir & 0xa000) != 0)
        player->desired_velocity.x = player->base.dir_x * 0x200;
    if ((input & 0x5000) != 0)
        player->desired_velocity.z = player->base.dir_z * 0x200;
#define APPROACH_COMPONENT(component) \
    do \
    { \
        actual = player->velocity.component; \
        desired = player->desired_velocity.component; \
        if (actual < desired) \
            actual += 0x20; \
        if (desired < actual) \
            actual -= 0x20; \
        player->velocity.component = actual; \
    } while (0)
    APPROACH_COMPONENT(x);
    APPROACH_COMPONENT(y);
    APPROACH_COMPONENT(z);
#undef APPROACH_COMPONENT
    player->base.col.x += player->velocity.x;
    player->base.col.y += player->velocity.y;
    player->base.col.z += player->velocity.z;
    if (g_camera.limit_top != -1)
    {
        limit = g_camera.limit_top - 0x2000;
        if (player->base.col.object.update == scuba_mission_complete_player_update)
            limit -= 0x5f00;
        if (player->base.col.y < limit)
            player->base.col.y = limit;
    }
    if (g_camera.limit_bottom != -1 && g_camera.limit_bottom < player->base.col.y)
        player->base.col.y = g_camera.limit_bottom;
    if (g_camera.room_player_near_z != 0 && player->base.col.z < g_camera.room_player_near_z)
        player->base.col.z = g_camera.room_player_near_z;
    if (g_camera.player_far_z_limit != -1 && player->base.col.z >= g_camera.player_far_z_limit - 0x2600)
        player->base.col.z = g_camera.player_far_z_limit - 0x2600;
    if (g_camera.player_near_z_limit != -1 && player->base.col.z < g_camera.player_near_z_limit)
        player->base.col.z = g_camera.player_near_z_limit;
    map_motion_resolve(old_x, old_y, old_z, &player->base.col.x, &player->base.col.y, &player->base.col.z, 0, 0, 0x40, 0x20, 0x0a);
    if (player->animation_countdown == 0)
    {
        motion = player->velocity.x;
        if (motion < 0)
            motion = -motion;
        actual = player->velocity.y;
        if (actual < 0)
            actual = -actual;
        if (motion < actual)
            motion = actual;
        actual = player->velocity.z;
        if (actual < 0)
            actual = -actual;
        if (motion < actual)
            motion = actual;
        ++player->animation_frame;
        motion = motion < 0 ? motion + 0x3f : motion;
        player->animation_countdown = (0xb - (motion >> 6));
        if (player->animation_countdown <= 0)
            player->animation_countdown = 1;
    }
    else
        --player->animation_countdown;
    if (player->animation_frame >= 0x0c)
        player->animation_frame = 0;
    limit = -((player->base.col.z - g_camera.world_z) * 0x8c) / 320 + g_camera.world_x;
    if (player->base.col.x < limit)
    {
        player->base.col.x = limit;
        player->velocity.x = 0;
    }
    limit = ((player->base.col.z - g_camera.world_z) * 0x8c) / 320 + g_camera.world_x;
    if (limit < player->base.col.x)
    {
        player->base.col.x = limit;
        player->velocity.x = 0;
    }
    render_world_spr((frame_base + player->animation_frame), player->base.col.x, player->base.col.y, player->base.col.z, player->base.dir < 4, 0x1000, 0x1000, &player->base.col.prim, 0, 2, &player->base.vram_desc, 0);
    object_embedded_hit_flash_apply(player);
    if (g_cd_file_io_enabled == 0)
        player_debug_movement_update();
    g_player_world.x = player->base.col.x;
    g_player_world.y = player->base.col.y;
    g_player_world.z = player->base.col.z;
    g_player_world.delta_x = player->base.col.x - old_x;
    g_player_world.delta_y = player->base.col.y - old_y;
    g_player_world.delta_z = player->base.col.z - old_z;
    camera_update(player->base.col.x, player->base.col.y, player->base.col.z, player->desired_dir);
    g_camera.prev_player_x = g_player_world.x = player->base.col.x;
    g_camera.prev_player_y = g_player_world.y = player->base.col.y;
    g_camera.prev_player_z = g_player_world.z = player->base.col.z;
    if ((g_render_state.frame & 0xf) == 0 && g_map_render.background_resource != 0)
        scuba_distant_water_particle_create();
    player->nodes[1].scale = 0xa3;
    player->nodes[1].rotation_y = ((player->base.dir << 9) & 0xfff);
    player->nodes[1].translation_x = player->base.col.x - g_camera.world_x;
    player->nodes[1].translation_y = player->base.col.y - (g_camera.world_y + 0xc00);
    player->nodes[1].translation_z = player->base.col.z - g_camera.world_z;
    g_model_draw.target_frame = 0;
    model_render_node(&player->nodes[1], &scuba_identity);
    if ((g_render_state.frame & 0x40) != 0)
    {
        if ((g_render_state.frame & 0x3f) == 0)
            sound_play_positional(random_range(3) + 0x98, 0, 0x7f, player->base.col.x, player->base.col.y, player->base.col.z);
        if ((g_render_state.frame & 3) == 0 && (g_render_state.frame & 0x3f) < 0x20)
        {
            scuba_particle_create(player->nodes[3].world_x, player->nodes[3].world_y, player->nodes[3].world_z);
            if (g_invulnerability_cheat_enabled == 0 && player->base.col.object.update != scuba_mission_complete_player_update)
            {
                --player->base.air;
                if (player->base.air < 0)
                    player->base.air = 0;
            }
        }
    }
    if (player->base.dir != player->base.prev_dir)
        player->turn_flash_ticks = 1;
    if (player->turn_flash_ticks != 0)
    {
        SPRITE *effect;
        --player->turn_flash_ticks;
        effect = world_spr_create(player->nodes[5].world_x, player->nodes[5].world_y, player->nodes[5].world_z, turn_flash_animation);
        effect->scale_x = 0x2000;
        scuba_particle_create(player->nodes[5].world_x, player->nodes[5].world_y, player->nodes[5].world_z);
    }
    if ((input & 0x80) != 0)
    {
        if (player->fire_latch == 0 && object_count_by_type(0x64) < 2)
        {
            SPRITE *shot;
            COLLISION *target;
            sint32 steps;
            steps = fixed_dda_initialize(player->nodes[4].world_x, player->nodes[4].world_y, player->nodes[4].world_z, player->nodes[2].world_x, player->nodes[2].world_y, player->nodes[2].world_z, &line);
            shot = world_spr_create(player->nodes[2].world_x, player->nodes[2].world_y, player->nodes[2].world_z, projectile_animation);
            shot->col.object_type = 0x64;
            shot->light_delta = -0x18;
            shot->light_step = -2;
            shot->force_light = 1;
            shot->velocity_x = (line.step.x / 0x100) * 2;
            shot->velocity_y = (line.step.y / 0x100) * 2;
            actual = line.step.z;
            if (actual < 0)
                actual += 0xff;
            shot->velocity_z = (actual >> 8) * 2;
            shot->rotation_step_x = 0x66;
            shot->frame_callback = scuba_projectile_update;
            shot->col.sends_mask = 1;
            shot->col.receives_mask = 4;
            shot->col.hit.damage = 1;
            col_box_set(shot, 0x40, 0x10, 0x50);
            shot->col.box_y /= 2;
            sound_play_positional(0x9e, 0, 0x7f, player->base.col.x, player->base.col.y, player->base.col.z);
            player->fire_latch = 1;
            target = player_special_projectile_create(player->base.dir << 7);
            if (target != 0)
            {
                shot->behavior.projectile.target_x = target->x;
                shot->behavior.projectile.target_y = target->y - 0x2000;
                shot->light_delta = -0x0c;
                shot->lifetime = 0x96;
                shot->behavior.projectile.target_z = target->z;
            }
        }
    }
    else
        player->fire_latch = 0;
    if (player->base.damage_cooldown != 0)
        --player->base.damage_cooldown;
    if (player->base.damage_cooldown > 0 && player->base.damage_cooldown < 0x46 && (g_render_state.frame & 3) == 0)
        player->base.col.flash_clut_ticks = 1;
    if ((input & 0x20) != 0)
    {
        if (player->base.special_fire_latch == 0)
        {
            INVENTORY_SLOT *slot = &player->base.inventory[g_sel_weapon_slot];
            player->base.special_fire_latch = 1;
            if ((player->base.animation.flags & 1) == 0 && slot->count != 0)
            {
                if (slot->type == 0x76 && object_count_by_type(0x76) <= 0)
                {
                    SPRITE *item = world_spr_create(player->base.col.x, player->base.col.y, player->base.col.z, bubble_item_animation);
                    item->acceleration_y = -0x0a;
                    item->col.object_type = 0x76;
                    item->frame_callback = scuba_bubble_trail_update;
                    sound_play_positional(0xa3, 0, 0xc8, player->base.col.x, player->base.col.y, player->base.col.z);
                }
                if (slot->type == 0x78 && slot->count != 0 && object_count_by_type(0x79) < 4)
                {
                    PLAYER_ACTION *item = player_action_object_create(player->base.col.x, player->base.col.y, player->base.col.z, 0x79);
                    item->col.receives_mask = 0;
                    --slot->count;
                }
            }
        }
    }
    else
        player->base.special_fire_latch = 0;
    if ((((input & 0xf) == 0xf) && g_input.held != player->base.prev_buttons) || (player->base.effect_latch != 0 && (g_render_state.frame & 0xf) == 0))
    {
        COLLISION *item = (COLLISION *)object_find_next_by_type(0, 0x79);
        if (item != 0)
        {
            player->base.effect_latch = 1;
            scuba_blast_effect_create(item);
            object_destroy(item);
        }
        else
            player->base.effect_latch = 0;
    }
    if (player->base.air >= 0x41 || (g_render_state.frame & 8) != 0)
        hud_bar_render(&player->air_bar, player->base.air, 0, 0);
    if (player->base.air <= 0 && player->base.col.object.update != scuba_mission_complete_player_update)
    {
        player->base.air = 0;
        if (--player->base.health <= 0)
            scuba_player_damage_apply(0, player);
    }
    player->base.prev_buttons = input;
}

/* Exact 0x800FD274..0x800FD47C: drifting hazard movement and render. */
/* Original: SCUBA_FUN_800fd274. */
/* Original: SCUBA:SCUBA_800FD274. */
static void scuba_drifting_hazard_update(void *raw_object)
{
    SCUBA_DRIFTING_HAZARD *object = (SCUBA_DRIFTING_HAZARD *)raw_object;
    sint32 old_x = object->col.x, old_y = object->col.y;
    sint32 old_z = object->col.z;
    sint32 blocked = 0, ceiling;
    object->col.y += object->velocity_y;
    object->velocity_y += 4;
    if (object->col.y >= g_player_world.y - 0x2000)
    {
        ceiling = g_camera.world_y + ((object->col.z - g_camera.world_z) * 32) / 355;
        if (object->col.y >= ceiling)
            blocked = 1;
    }
    object->col.y += 0x4000;
    if (map_motion_resolve(old_x, old_y + 0x4000, old_z, &object->col.x, &object->col.y, &object->col.z, 0, 0, 0x10, 0x50, 5) != 0)
        blocked = 1;
    object->col.y -= 0x4000;
    if (blocked)
    {
        scuba_blast_effect_create(&object->col);
        object_destroy(object);
        return;
    }
    object->node.rotation_y = ((object->node.rotation_y + object->rotation_step_y) & 0xfff);
    object->node.rotation_x = ((object->node.rotation_x + object->rotation_step_x) & 0xfff);
    object->node.translation_x = object->col.x - g_camera.world_x;
    object->node.translation_y = object->col.y - g_camera.world_y;
    object->node.translation_z = object->col.z - g_camera.world_z;
    object->node.model_id = g_depth_charge_model_id;
    ceiling = g_map_render.depth_anchor_z - object->col.z;
    g_model_draw.depth_bucket = 0x480 - (ceiling < 0 ? (ceiling + 0xff) >> 8 : ceiling >> 8);
    g_model_draw.world_z = object->col.z;
    g_model_draw.target_frame = g_cur_render_frame;
    model_render_node(&object->node, &scuba_identity);
}

/* Exact 0x800FD108..0x800FD274: type-0x67 drifting hazard emitter. */
/* Original: SCUBA_FUN_800fd108. */
/* Original: SCUBA:SCUBA_800FD108. */
void scuba_drifting_hazard_create(EFFECT *effect)
{
    SCUBA_DRIFTING_HAZARD *object;
    sint32 half_x, half_z, x, y, z;
    if ((g_render_state.frame & 0x3f) != 0)
        return;
    if ((g_render_state.frame & 0x7f) == 0)
        sound_play_positional(0x9f, 6, 0x7f, effect->x, effect->y, effect->z);
    half_x = (sint16)effect->vals[0] * 0x4000;
    half_z = (sint16)effect->vals[1] * 0x4000;
    x = effect->x + random_range(half_x * 2) - half_x;
    y = g_camera.world_y - (((effect->z - g_camera.world_z) * 96) / 355);
    z = effect->z + random_range(half_z * 2) - half_z;
    scuba_type67_spawn_sequence = g_depth_charge_model_id + 1;
    object = (SCUBA_DRIFTING_HAZARD *)object_create(sizeof(*object), scuba_drifting_hazard_update);
    object->col.x = x;
    object->col.y = y;
    object->col.z = z;
    object->node.rotation_y = random_range(0xfff);
    object->rotation_step_y = random_range(1) * 0x10 - 8;
    object->rotation_step_x = 8;
    object->col.object_type = 0x67;
}

/* Exact 0x800FD73C..0x800FD960: proximity mine update/render. */
/* Original: SCUBA_FUN_800fd73c. */
/* Original: SCUBA:SCUBA_800FD73C. */
static void scuba_mine_update(void *raw_object)
{
    SCUBA_MINE *object = (SCUBA_MINE *)raw_object;
    sint32 dx, dy, dz;
    uint8 flash = 0;
    sint16 phase;
    if (world_object_is_visible(object->col.x, object->col.y, object->col.z, 0x4000) == 0)
    {
        EFFECT *effect = object->owner_effect;
        object_destroy(object);
        effect->type = 0x69;
        return;
    }
    phase = (object->phase + object->phase_step);
    object->phase = phase;
    if (phase < -0x10)
        object->phase_step = 3;
    if (object->phase >= 0x21)
        object->phase_step = -3;
    dx = g_player_world.x - object->col.x;
    if (dx < 0)
        dx = -dx;
    dy = g_player_world.y - object->col.y;
    if (dy < 0)
        dy = -dy;
    dz = g_player_world.z - object->col.z;
    if (dz < 0)
        dz = -dz;
    if (dx < 0x5000 && dy < 0x5000 && dz < 0x5000)
    {
        sint16 timer = object->proximity_ticks + 1;
        object->proximity_ticks = timer;
        flash = 0x60;
        if (timer == 1 || timer == 0x1e || timer == 0x2d)
            sound_play_positional(0xa2, 6, 0x7f, object->col.x, object->col.y, object->col.z);
        if (timer >= 0x3d)
        {
            scuba_blast_effect_create(&object->col);
            object_destroy(object);
            return;
        }
    }
    else
        object->proximity_ticks = 0;
    g_next_frame_object->light_delta = (object->phase + flash);
    render_world_spr(0x2cc00, object->col.x, object->col.y + player_bob_offset_update(&object->vertical_animation), object->col.z, 0, 0x1000, 0x1000, 0, 0, 0, 0, 0);
    object_hit_flash_apply(object);
}

/* Exact 0x800FD690..0x800FD73C: type-0x69 mine constructor. */
/* Original: SCUBA_FUN_800fd690. */
/* Original: SCUBA:SCUBA_800FD690. */
void scuba_mine_create(EFFECT *effect)
{
    SCUBA_MINE *object;
    effect->type = 0;
    object = (SCUBA_MINE *)object_create(sizeof(*object), scuba_mine_update);
    object->owner_effect = effect;
    object->col.x = effect->x;
    object->col.y = effect->y;
    object->col.z = effect->z;
    object->col.hit.health = 0x0a;
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = scuba_target_damage;
    col_box_set(object, 0x40, 0x40, 0x40);
    object->phase_step = -4;
    object->col.box_y /= 2;
}

/* 0x800FEB90: renderer/update for model attribute 0x71. */
/* Original: SCUBA_FUN_800feb90. */
/* Original: SCUBA:SCUBA_800FEB90. */
static void scuba_model_target_update(void *raw_object)
{
    SCUBA_MODEL_TARGET *object = (SCUBA_MODEL_TARGET *)raw_object;
    if (world_object_is_visible(object->col.x, object->col.y, object->col.z, 0x4600) != 0)
    {
        sint32 delta = g_map_render.depth_anchor_z - object->col.z;
        sint32 depth = delta < 0 ? (delta + 0xff) >> 8 : delta >> 8;
        g_model_draw.model_idx = object->model_id;
        g_model_draw.depth_bucket = 0x490 - depth;
        g_model_draw.world_x = object->col.x;
        g_model_draw.world_y = object->col.y;
        g_model_draw.world_z = object->col.z;
        if (object->fire_countdown < 6)
            g_model_draw.world_z += 0x1000;
        g_model_draw.shade = 0x80;
        g_model_draw.rotation_y = 0;
        g_model_draw.rotation_x = 0;
        g_model_draw.rotation_z = 0;
        g_model_draw.translation_z = 0;
        g_model_draw.translation_x = 0;
        model_render();
        if (object->fire_countdown == 0)
        {
            object->fire_countdown = (random_range(0x3c) + 0x78);
            scuba_projectile_create(object->col.x, object->col.y, object->col.z, object->col.x, object->col.y, object->col.z - 0x2000, 0x64);
        }
        --object->fire_countdown;
    }
}

/* 0x800FEACC: SLES overlay dispatch for map model attribute 0x71. */
/* Original: FUN_800FEACC. */
/* Original: SCUBA:SCUBA_800FEACC. */
void scuba_model_target_create(sint32 x, sint32 y, sint32 z, sint32 model)
{
    SCUBA_MODEL_TARGET *object = (SCUBA_MODEL_TARGET *)object_create(sizeof(*object), scuba_model_target_update);
    object->model_id = model;
    object->col.x = x;
    object->col.y = y;
    object->col.z = z + 0x2000u;
    col_box_set(object, 0x30, 0x30, 0x30);
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = scuba_target_damage;
    object->col.hit.health = 3;
    object->col.object_type = 0x71;
    object->col.box_y /= 2;
}

/* Original range: 0x800FAE70..0x800FB054 */ /* Original: SCUBA:SCUBA_800FAE70. */
void scuba_player_create(sint32 x, sint32 y, sint32 z)
{
    SCUBA_PLAYER *player = (SCUBA_PLAYER *)object_create(sizeof(*player), scuba_player_update);
    sint32 idx;
    player->base.facing_x = 1;
    player->base.air = 0x100;
    SetPolyFT4(&player->base.col.prim);
    setShadeTex(&player->base.col.prim, 0);
    player->base.col.sends_mask = 2;
    player->base.col.prim.p[0].attr = g_player_spr_clut;
    col_box_set(player, 0x60, 0x20, 6);
    player->base.col.x = x;
    player->base.col.y = y;
    player->base.col.z = z;
    spr_vram_allocate(&player->base.vram_desc, 0x6c, 0x40, 1);
    player->base.health = 0x400;
    g_player = &player->base;
    player->base.vram_desc.clut = g_player_spr_clut;
    player_inventory_add(0x76, -1, &player->base);
    player_inventory_add(0x78, 3, &player->base);
    for (idx = 0; idx < 6; idx++)
    {
        player->node_table[idx] = &player->nodes[idx];
        player->nodes[idx].pose_enabled = 0x3f;
    }
    model_pose_init(&initial_pose, player->node_table, 6, 1);
    g_player_world.x = x;
    g_player_world.y = y;
    g_player_world.z = z;
    hud_bar_initialize(&player->air_bar, 0x10, 0x52, 0x40, 0x100);
    scuba_particle_count = 0;
    g_invulnerability_cheat_enabled = 0;
    g_weapon_cheat_enabled = 0;
}
