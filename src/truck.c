#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include "spatial.h"
#include "game_math.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "render.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "truck.h"

/* Original: DAT_800D4150. */
sint16 g_truck_model_base;

/* Types. */
/* Semantic native-port module; original address comments are preserved. */
/* COMMON0/TRUCK.BIN, PAL 0x800FADD0..0x800FB8A4.
 * Direct translation of the three functions used by DOCKS/M1.CC. */

typedef struct
{
    COLLISION col;
    sint32 origin_x;
    sint32 target_x;
    sint32 velocity_x;
    sint32 target_velocity_x;
    sint32 travel_dir_x;
    sint16 wait_ticks;
    sint16 waiting;
    sint16 movement_ticks;
    sint16 engine_sound_ticks;
    sint16 braking_sound_ticks;
    sint16 stop_sound_ticks;
    sint32 acceleration_x;
    sint32 col_min_y;
    sint32 col_min_x;
    sint32 col_max_x;
    sint32 col_min_z;
    sint32 col_max_z;
    EFFECT *owner_effect;
    EFFECT projectile_effect;
    MODEL_NODE nodes[7];
} TRUCK_ACTOR;

/* Resident PAL 0x800AE7BC..0x800AEA54, reached by TRUCK.BIN destruction. */

typedef struct
{
    COLLISION col;
    MODEL_NODE *node;
    sint16 age;
    sint16 tag;
    sint32 velocity_y;
    sint16 scr_depth;
} TANK_DESTRUCTION_ACTOR;

/* Original data: SLES_004.74:0x800C87B4..0x800C87C7 */
static const ANIM_COMMAND truck_particle_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {240, 164866, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Functions. */
/* Original: TRUCK:TRUCK_800FB128. */
static void truck_update(void *raw_t)
{
    TRUCK_ACTOR *t = (TRUCK_ACTOR *)raw_t;
    EFFECT *src = t->owner_effect;
    sint32 old_velocity = t->velocity_x, velocity, target_distance, abs_old, abs_new, speed;
    if (!world_object_is_visible(t->col.x, t->col.y, t->col.z, 0x7d000))
    {
        object_destroy(t);
        src->type = 0x22;
    }
    if (t->engine_sound_ticks != 0)
        --t->engine_sound_ticks;
    else
    {
        sound_play_positional(0x53, 0, 0x7f, t->col.x, t->col.y, t->col.z);
        t->engine_sound_ticks = 0x19;
    }
    target_distance = abs_s32(t->origin_x - t->col.x);
    if (t->wait_ticks != 0 && t->velocity_x == 0)
    {
        if (t->waiting == 0)
            t->waiting = 1;
    }
    if (t->waiting != 0)
    {
        if (--t->wait_ticks == 0)
        {
            t->waiting = 0;
            t->acceleration_x = 8;
            t->target_velocity_x = t->travel_dir_x * 2;
        }
        if (t->velocity_x == 0 && t->stop_sound_ticks == 0)
        {
            t->target_x = t->origin_x + ((t->target_x < t->origin_x ? -1 : 1) * ((random_range(0x80) + 0x40) << 8));
            t->stop_sound_ticks = 0x3c;
        }
        ++t->movement_ticks;
        if (t->stop_sound_ticks != 0)
        {
            if (t->stop_sound_ticks == 0x3c)
                sound_play_positional(0x51, 0, 0x7f, t->col.x, t->col.y, t->col.z);
            if (t->stop_sound_ticks == 4)
                sound_play_positional(0x52, 0, 0x7f, t->col.x, t->col.y, t->col.z);
            --t->stop_sound_ticks;
        }
        if (t->stop_sound_ticks == 0)
        {
            velocity = t->velocity_x;
            if (t->col.x >= t->target_x)
            {
                if (velocity >= -0x3ff)
                    velocity -= 0x20;
            }
            else if (velocity < 0x400)
                velocity += 0x20;
            t->velocity_x = velocity;
            if ((g_render_state.frame & 7) == 0)
            {
                SPRITE *p = world_spr_create(t->nodes[5].world_x, t->nodes[5].world_y - 0x800, t->nodes[5].world_z, truck_particle_animation);
                p->acceleration_y = -10;
                p->scale_x = 0x800;
                p->scale_step = 0x40;
                p->light_delta = 0x80;
                p->light_step = -7;
                p->lifetime = 0x3c;
                p->rotation_step_x = 4;
                p->velocity_x = t->velocity_x + 0x300;
                p->clut_override = g_white_signal_flare_clut;
            }
        }
    }
    else
    {
        velocity = t->velocity_x;
        if (target_distance <= 0x1ffff && t->wait_ticks != 0)
            t->target_velocity_x = 0;
        if (t->target_velocity_x < velocity)
            velocity -= t->acceleration_x;
        if (velocity < t->target_velocity_x)
            velocity += t->acceleration_x;
        t->velocity_x = velocity;
    }
    if (t->braking_sound_ticks != 0)
        --t->braking_sound_ticks;
    velocity = t->velocity_x;
    abs_old = abs_s32(old_velocity);
    abs_new = abs_s32(velocity);
    if (abs_new < abs_old)
    {
        speed = velocity / 0x100;
        if (speed < 0)
            speed = -speed;
        if (speed == 3 && t->braking_sound_ticks == 0)
        {
            sound_play_positional(velocity < 0 ? 0x50 : 0x51, 0, 0x7f, t->col.x, t->col.y, t->col.z);
            t->braking_sound_ticks = 0x80;
        }
    }
    t->col.x += velocity;
    t->nodes[0].rotation_x = (((-velocity + (velocity > 0 ? 0x7f : 0)) >> 5) & 0xffc);
    t->nodes[0].rotation_y = 0xc00;
    t->nodes[0].model_id = g_truck_model_base;
    t->nodes[0].translation_x = t->col.x - g_camera.world_x;
    t->nodes[0].translation_y = t->col.y - g_camera.world_y - 0x6000;
    t->nodes[0].translation_z = t->col.z - g_camera.world_z;
    t->nodes[6].rotation_x = (((velocity + (velocity < 0 ? 0x7f : 0)) >> 5) & 0xffc);
    g_model_draw.depth_bucket = 0x480 - ((g_map_render.depth_anchor_z - t->col.z + (g_model_catalog.descs[g_truck_model_base].half_y * 0x100)) / 0x100);
    g_model_draw.world_z = t->col.z;
    t->nodes[1].rotation_x = ((t->nodes[1].rotation_x + velocity / 0x10) & 0xfff);
    t->nodes[4].rotation_x = t->nodes[3].rotation_x = t->nodes[2].rotation_x = t->nodes[1].rotation_x;
    if (t->col.flash_clut_ticks != 0)
    {
        --t->col.flash_clut_ticks;
        g_model_draw.clut_override = g_spr_pal.hit_flash;
    }
    g_model_draw.target_frame = g_cur_render_frame;
    model_render_node(&t->nodes[0], &g_model_identity);
    t->col_min_x = t->col.x - 0x9600;
    t->col_max_x = t->col.x + 0x9600;
    t->col_max_z = t->col.z + 0xc00;
    g_model_draw.clut_override = 0;
    t->col_min_z = t->col.z - 0x2000;
    t->col_min_y = t->col.y - 0x8000;
    if (velocity < 0)
    {
        EFFECT *shot = &t->projectile_effect;
        shot->x = t->col.x + 0x8000;
        shot->y = t->col.y - 0x6400;
        shot->z = t->col.z;
        MISSION_ENEMY_ACTOR *object = mission_enemy_create(shot);
        if (object)
        {
            object->scale_transition = 0;
            object->vertical_velocity = -0x300;
            object->pal_offset = 0;
        }
    }
    hud_bar_render(&g_boss_health_bar, t->col.hit.health, 0, 0);
}

/* Original: FUN_800AE844. */
static void vehicle_destruction_actor_update(void *raw_o)
{
    TANK_DESTRUCTION_ACTOR *o = (TANK_DESTRUCTION_ACTOR *)raw_o;
    sint32 delta;
    o->col.y += o->velocity_y;
    o->node->translation_x = o->col.x - g_camera.world_x;
    o->node->translation_y = o->col.y - g_camera.world_y;
    o->node->translation_z = o->col.z - g_camera.world_z;
    g_model_draw.target_frame = 0;
    model_render_begin();
    delta = g_map_render.depth_anchor_z - o->col.z;
    g_model_draw.depth_bucket = 0x480 - delta / 0x100;
    g_model_draw.depth_bucket = o->scr_depth;
    g_model_draw.world_z = o->col.z;
    model_render_node(o->node, &g_model_identity);
    model_render_end();
    ++o->age;
    if (o->age > 0x100)
        object_destroy(o);
    if (o->age == 0xfa)
    {
        sint32 half = o->col.width / 2;
        expl_flash_create(o->col.x - half, o->col.y, o->col.z - 0x4000);
        expl_flash_create(o->col.x + half, o->col.y, o->col.z - 0x4000);
        sound_play_nonpositional(9, 0, 0x7f);
        disp_shake_start();
    }
    if ((g_render_state.frame & 7) == 0)
    {
        sint32 x = o->col.x + random_range(o->col.box_x);
        sint32 y = o->col.y + 0x4000 + random_range(o->col.box_y);
        timed_callback_create(expl_debris_particle_create, x, y, o->col.z, 0x0c, 2);
    }
}

/* Original: FUN_800AE7BC. */
void *vehicle_destruction_actor_create(sint32 x, sint32 y, sint32 z, MODEL_NODE *node, sint16 tag, sint16 depth)
{
    TANK_DESTRUCTION_ACTOR *o = (TANK_DESTRUCTION_ACTOR *)object_create(sizeof(*o), vehicle_destruction_actor_update);
    o->node = node;
    o->col.x = x;
    o->col.y = y;
    o->col.z = z;
    o->tag = tag;
    o->scr_depth = depth;
    return o;
}

/* Original: TRUCK:TRUCK_800FB7B4. */
static void truck_damage(void *raw_col, void *raw_src)
{
    COLLISION *col = (COLLISION *)raw_col;
    COLLISION *src = (COLLISION *)raw_src;
    TRUCK_ACTOR *t = (TRUCK_ACTOR *)col;
    if (object_damage_apply(t, src) == 0)
        return;
    linked_list_unlink(&g_object_list, t);
    sint32 height = (g_map_render.depth_anchor_z - t->col.z + (g_model_catalog.descs[g_truck_model_base].half_y * 0x100));
    void *blast = vehicle_destruction_actor_create(t->col.x, t->col.y - 0x6000, t->col.z, &t->nodes[0], 7, (0x480 - height / 0x100));
    col_box_set(blast, 0x8c, 0x64, 0x46);
    ++g_mission.counts[0];
}

/* Original: TRUCK:TRUCK_800FADD4. */
void truck_create(EFFECT *effect)
{
    TRUCK_ACTOR *truck;
    MODEL_DESC *desc;
    sint32 dir;
    if (g_player_world.y != effect->y + 0x2000)
    {
        effect->vals[0] = 0;
        return;
    }
    effect->vals[0] = (effect->vals[0] + 1);
    if (effect->signed_vals[0] < 30)
        return;
    music_track_select(9);
    truck = (TRUCK_ACTOR *)runtime_heap_allocate(sizeof(*truck));
    truck->col.object.update = truck_update;
    linked_list_append(&g_object_list, truck);
    setPolyFT4(&truck->col.prim);
    setShadeTex(&truck->col.prim, 0);
    truck->owner_effect = effect;
    truck->col.x = effect->x + 0x25800;
    effect->type = 0;
    truck->col.y = effect->y + 0x3600;
    XASSERT((g_truck_model_base) < g_model_catalog.count);
    desc = &g_model_catalog.descs[g_truck_model_base];
    truck->col.z = effect->z - (desc->half_y * 0x100);
    truck->col.z += desc->half_y * 0x100;
    truck->origin_x = effect->x;
    truck->target_x = effect->x;
    dir = effect->x < truck->col.x ? -0x400 : 0x400;
    truck->target_velocity_x = dir;
    truck->velocity_x = dir;
    truck->travel_dir_x = dir;
    truck->wait_ticks = 0xea60;
    truck->acceleration_x = 4;
    truck->col.receives_mask = 0x43;
    truck->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    truck->col.hit_callback = truck_damage;
    truck->col.hit.health = 0x190;
    truck->col.player_contact_callback = player_apply_damage;
    col_box_set(truck, 0x8c, 0x64, 0x46);
    truck->nodes[0].scale = 0x1100;
    truck->nodes[1].translation_x = -0x2000;
    truck->nodes[1].translation_y = 0x3000;
    truck->nodes[1].translation_z = 0x6200;
    truck->nodes[1].scale = 0x0e00;
    truck->nodes[1].model_id = g_truck_model_base + 1;
    memcpy(&truck->nodes[2], &truck->nodes[1], sizeof(truck->nodes[2]));
    truck->nodes[2].translation_z = -0x5a00;
    memcpy(&truck->nodes[3], &truck->nodes[1], sizeof(truck->nodes[3]));
    truck->nodes[3].translation_x = 0x2000;
    memcpy(&truck->nodes[4], &truck->nodes[3], sizeof(truck->nodes[4]));
    truck->nodes[4].translation_z = -0x5a00;
    model_attach_child(&truck->nodes[6], &truck->nodes[0]);
    model_attach_child(&truck->nodes[1], &truck->nodes[6]);
    model_attach_child(&truck->nodes[2], &truck->nodes[6]);
    model_attach_child(&truck->nodes[4], &truck->nodes[6]);
    model_attach_child(&truck->nodes[5], &truck->nodes[0]);
    truck->nodes[5].translation_z = -0x7400;
    truck->nodes[5].translation_y = 0x3800;
    truck->projectile_effect.type = 0x27;
    truck->projectile_effect.vals[0] = 0x23;
    truck->projectile_effect.vals[1] = effect->vals[0];
    truck->projectile_effect.vals[2] = effect->vals[0];
    hud_bar_initialize(&g_boss_health_bar, -0x28, -0x60, 0x50, 0x190);
}
