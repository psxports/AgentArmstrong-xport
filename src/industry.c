#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "game_math.h"
#include "render.h"
#include "game_sound.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "camera.h"
#include "mechanism.h"
#include "industry.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"

/* Original data: IND/INDUSTRY.BIN:0x800FB830..0x800FB883 */
static const ANIM_COMMAND projectile_animation[11] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {4, 17430, 0}, 0}, {ANIM_STEP_FRAME, {4, 17431, 0}, 0}, {ANIM_STEP_FRAME, {4, 17432, 0}, 0}, {ANIM_STEP_FRAME, {4, 17433, 0}, 0}, {ANIM_STEP_FRAME, {4, 17434, 0}, 0}, {ANIM_STEP_FRAME, {4, 17435, 0}, 0}, {ANIM_STEP_FRAME, {4, 17436, 0}, 0}, {ANIM_STEP_FRAME, {4, 17437, 0}, 0}, {ANIM_STEP_FRAME, {4, 17438, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Types. */
/* IND/INDUSTRY.BIN: stage-27 type-0x7F closure, exact PAL
 * 0x800FB5C4..0x800FB830. */

typedef struct
{
    COLLISION col;
    MODEL_NODE node;
    sint32 floor_y;
    sint32 velocity_y;
    uint16 bounce_ticks;
    uint16 destruction_armed;
    uint16 particle_ticks;
} BIO_GOO_ACTOR;

typedef struct
{
    COLLISION col;
    MODEL_NODE nodes[2];
    uint16 activation_ticks;
    uint16 target_rotation;
    sint16 active;
} ROBOT_PRESS_ACTOR;

typedef struct
{
    COLLISION col;
    EFFECT *src_effect;
    uint16 objective_decremented;
} INDUSTRY_GUN_CONTROLLER;

/* Variables. */

/* Functions. */
/* Original: INDUSTRY:INDUSTRY_800FB1F4. */
static void bio_goo_damage(void *raw_col, void *raw_hit)
{
    COLLISION *col = (COLLISION *)raw_col;
    COLLISION *hit = (COLLISION *)raw_hit;
    BIO_GOO_ACTOR *o = (BIO_GOO_ACTOR *)col;
    if (o->col.y != o->floor_y - 0x12c00 && object_damage_apply(o, hit))
    {
        o->destruction_armed = 1;
        o->velocity_y = 0x700;
        o->bounce_ticks = 0;
        o->col.receives_mask = 0;
    }
}

/* Original: INDUSTRY:INDUSTRY_800FAE9C. */
static void bio_goo_update(void *raw_o)
{
    BIO_GOO_ACTOR *o = (BIO_GOO_ACTOR *)raw_o;
    MISSION_ENEMY_ACTOR *enemy = 0;
    void *next = 0;
    sint32 depth;
    while ((next = object_find_next_by_type(next, 0x7e)) != 0)
    {
        enemy = (MISSION_ENEMY_ACTOR *)next;
        if (o->col.x - 0x4000 < enemy->col.x && enemy->col.x < o->col.x + 0x4000 && o->col.z - 0x2000 < enemy->col.z)
        {
            if (o->velocity_y != 0x600)
                sound_play_positional(0x47, 0, 0x96, o->col.x, o->col.y, o->col.z);
            o->velocity_y = 0x600;
            break;
        }
    }
    if (o->bounce_ticks)
        --o->bounce_ticks;
    else
    {
        o->col.y += o->velocity_y;
        if (o->col.y < o->floor_y - 0x12c00)
            o->col.y = o->floor_y - 0x12c00;
    }
    if (o->bounce_ticks == 1)
        o->particle_ticks = 60;
    if (o->particle_ticks)
    {
        --o->particle_ticks;
        continuous_weapon_particle_create(o->col.x, o->floor_y + 0x8000, o->col.z, g_spr_pal.cluts[CLUT_SIGNAL_FLARE]);
    }
    if (o->floor_y < o->col.y)
    {
        if (!o->bounce_ticks)
            sound_play_positional(0x46, 0, 0x96, o->col.x, o->col.y, o->col.z);
        if (o->destruction_armed)
        {
            object_destroy(o);
            large_object_destruction_effect_create(&o->col);
            --g_mission.counts[15];
            return;
        }
        o->velocity_y = -0x200;
        o->bounce_ticks = 60;
        o->col.y = o->floor_y;
        next = 0;
        while ((next = object_find_next_by_type(next, 0x7e)) != 0)
        {
            MISSION_ENEMY_ACTOR *enemy = (MISSION_ENEMY_ACTOR *)next;
            VRAM_SPRITE *desc = enemy->col.frame_desc;
            if (o->col.x - 0x4000 < enemy->col.x && enemy->col.x < o->col.x + 0x4000 && o->col.z - 0x2000 < enemy->col.z)
            {
                airfield_enemy_configure(enemy);
                if (desc->region)
                    spr_vram_defer_release(desc);
            }
        }
    }
    o->node.translation_x = o->col.x - g_camera.world_x;
    o->node.translation_y = o->col.y - g_camera.world_y;
    o->node.translation_z = o->col.z - g_camera.world_z;
    g_model_draw.target_frame = g_cur_render_frame;
    depth = g_map_render.depth_anchor_z - o->col.z;
    g_model_draw.depth_bucket = 0x480 - depth / 0x100;
    g_model_draw.world_z = o->col.z;
    if (o->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --o->col.flash_clut_ticks;
    }
    model_render_node(&o->node, &g_model_identity);
    g_model_draw.clut_override = 0;
}

/* Original: INDUSTRY:INDUSTRY_800FADD4. */
void bio_goo_create(EFFECT *e)
{
    BIO_GOO_ACTOR *o = (BIO_GOO_ACTOR *)object_create(sizeof(*o), bio_goo_update);
    o->node.rotation_y = 0x800;
    o->node.model_id = g_model_selections.bio_goo;
    o->col.x = e->x;
    o->col.y = e->y - 0x10000;
    o->col.z = e->z;
    o->floor_y = e->y + 0x4000;
    e->type = 0;
    o->col.receives_mask = 1;
    o->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    o->col.hit_callback = bio_goo_damage;
    col_box_set(o, 0xc0, 0x80, 0xc0);
    o->col.hit.health = 100;
    o->col.box_y /= 2;
}

/* Original: INDUSTRY:INDUSTRY_800FB520. */
static void robot_press_damage(void *raw_col, void *raw_hit)
{
    COLLISION *col = (COLLISION *)raw_col;
    COLLISION *hit = (COLLISION *)raw_hit;
    ROBOT_PRESS_ACTOR *o = (ROBOT_PRESS_ACTOR *)col;
    EFFECT *e = 0;
    if (!object_damage_apply(o, hit))
        return;
    while ((e = effect_find_next(0x27, e)) != 0)
    {
        if (o->col.x - 0x4000 < e->x && e->x < o->col.x + 0x4000)
        {
            e->type = 0;
            break;
        }
    }
    large_object_destruction_effect_create(&o->col);
    object_destroy(o);
    --g_mission.counts[17];
}

/* Original: INDUSTRY:INDUSTRY_800FB32C. */
static void robot_press_update(void *raw_o)
{
    ROBOT_PRESS_ACTOR *o = (ROBOT_PRESS_ACTOR *)raw_o;
    COLLISION *nearby = 0;
    sint32 depth;
    while ((nearby = (COLLISION *)object_find_next_by_type(nearby, 0x92)) != 0)
    {
        if (o->col.x - 0x4000 < nearby->x && nearby->x < o->col.x + 0x4000 && o->col.z - 0x2000 < nearby->z)
            break;
    }
    if (nearby)
    {
        o->target_rotation = 0x400;
        if (!o->active)
            sound_play_positional(0x48, 0, 0x96, o->col.x, o->col.y, o->col.z);
        o->active = 1;
        o->activation_ticks = 100;
    }
    else
    {
        o->target_rotation = 0;
        o->active = 0;
    }
    if (o->activation_ticks)
    {
        o->nodes[0].rotation_y = angle_approach_wrapped(0x400, 0x10, o->nodes[0].rotation_y);
        --o->activation_ticks;
    }
    else
        o->nodes[0].rotation_y = angle_approach_wrapped(0, 0x10, o->nodes[0].rotation_y);
    o->nodes[0].translation_x = o->col.x - g_camera.world_x;
    o->nodes[0].translation_y = o->col.y - g_camera.world_y;
    o->nodes[0].translation_z = o->col.z - g_camera.world_z;
    g_model_draw.target_frame = g_cur_render_frame;
    depth = g_map_render.depth_anchor_z - o->nodes[1].world_z;
    g_model_draw.depth_bucket = 0x470 - depth / 0x100;
    g_model_draw.world_z = o->col.z;
    if (o->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --o->col.flash_clut_ticks;
    }
    model_render_node(&o->nodes[0], &g_model_identity);
    g_model_draw.clut_override = 0;
}

/* Original: INDUSTRY:INDUSTRY_800FB25C. */
void robot_press_create(EFFECT *e)
{
    ROBOT_PRESS_ACTOR *o = (ROBOT_PRESS_ACTOR *)object_create(sizeof(*o), robot_press_update);
    model_attach_child(&o->nodes[1], &o->nodes[0]);
    o->nodes[1].translation_x = 0x2000;
    o->nodes[1].model_id = g_model_selections.robot_press;
    o->col.x = e->x - 0x2000;
    o->col.y = e->y + 0x8000;
    o->col.z = e->z - 0x2000;
    e->type = 0;
    o->col.receives_mask = 1;
    o->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    o->col.hit_callback = robot_press_damage;
    col_box_set(o, 0x40, 0x80, 0x40);
    o->col.hit.health = 150;
    o->col.box_y /= 2;
}

/* Original: INDUSTRY:INDUSTRY_800FB600. */
static void industry_gun_update(void *raw_gun)
{
    INDUSTRY_GUN_CONTROLLER *gun = (INDUSTRY_GUN_CONTROLLER *)raw_gun;
    EFFECT *src = gun->src_effect;
    sint32 depth, left, right, y, min_y;
    uint32 extent;
    SPRITE *shot;
    sint16 switch_idx = src->vals[3];
    XASSERT(!(switch_idx < 0 || switch_idx >= sizeof(g_mechanism_switch_states)));
    if (!g_mechanism_switch_states[switch_idx])
    {
        if (!gun->objective_decremented)
        {
            --g_mission.counts[16];
            gun->objective_decremented = 1;
        }
        if (!(g_render_state.frame & 0x40))
            return;
    }
    depth = ((uint32)src->z - g_camera.world_z);
    extent = g_render_state.half_width + 0x60u;
    /* Original left and right bounds use separate low-word products */
    left = (g_camera.world_x + (uint32)div_trunc(mul_lo(-extent, depth), 320));
    right = (g_camera.world_x + (uint32)div_trunc(mul_lo(extent, depth), 320));
    if (src->x < left || right < src->x)
    {
        src->vals[0] = 0;
        return;
    }
    if (src->vals[0])
    {
        --src->vals[0];
        return;
    }
    y = (src->y - 0x2800u);
    min_y = (g_camera.world_y + (uint32)div_trunc(mul_lo(-0x80, depth), 355));
    if (y < min_y)
        y = min_y;
    src->vals[0] = g_mechanism_switch_states[switch_idx] ? 7 : random_range(8) + 8;
    shot = world_spr_create(src->x, y, src->z, projectile_animation);
    shot->velocity_y = 0xb00;
    shot->scale_x = 0x1400;
    shot->scale_y = 0x2400;
    col_box_set(shot, 0x46, 0x46, 0x48);
    shot->col.receives_mask = 2;
    shot->col.player_contact_callback = player_apply_damage;
    shot->col.hit.damage = 0x400;
}

/* Original: INDUSTRY:INDUSTRY_800FB5C4. */
void industry_gun_create(EFFECT *effect)
{
    INDUSTRY_GUN_CONTROLLER *gun = (INDUSTRY_GUN_CONTROLLER *)object_create(sizeof(*gun), industry_gun_update);
    gun->src_effect = effect;
    effect->type = 0;
}
