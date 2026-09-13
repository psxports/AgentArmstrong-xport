#include <stddef.h>
#include <string.h>
#include "app.h"
#include "audio/game_sound.h"
#include "camera.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "stubs.h"
#include "truck.h"

/* Types. */
/* Semantic native-port module; original address comments are preserved. */
/* COMMON0/TRUCK.BIN, PAL 0x800FADD0..0x800FB8A4.
 * Direct translation of the three functions used by DOCKS/M1.CC. */

typedef struct
{
    COLLISION collision;        /* +0x000 */
    sint16 health;              /* +0x078 */
    sint16 flash_clut_ticks;    /* +0x07A */
    sint32 origin_x;            /* +0x07C */
    sint32 target_x;            /* +0x080 */
    sint32 velocity_x;          /* +0x084 */
    sint32 target_velocity_x;   /* +0x088 */
    sint32 travel_direction_x;  /* +0x08C */
    sint16 wait_ticks;          /* +0x090 */
    sint16 waiting;             /* +0x092 */
    sint16 movement_ticks;      /* +0x094 */
    sint16 engine_sound_ticks;  /* +0x096 */
    sint16 braking_sound_ticks; /* +0x098 */
    uint8 field_09a[2];         /* +0x09A */
    sint16 stop_sound_ticks;    /* +0x09C */
    uint8 field_09e[2];         /* +0x09E */
    sint32 acceleration_x;      /* +0x0A0 */
    uint8 field_0a4[0x10];      /* +0x0A4 */
    sint32 collision_min_y;     /* +0x0B4 */
    uint8 field_0b8[0x0C];      /* +0x0B8 */
    sint32 collision_min_x;     /* +0x0C4 */
    uint8 field_0c8[4];         /* +0x0C8 */
    sint32 collision_max_x;     /* +0x0CC */
    uint8 field_0d0[0x0C];      /* +0x0D0 */
    sint32 collision_min_z;     /* +0x0DC */
    sint32 collision_max_z;     /* +0x0E0 */
    uint8 field_0e4[4];         /* +0x0E4 */
    EFFECT *owner_effect;       /* +0x0E8 */
    EFFECT projectile_effect;   /* +0x0EC */
    MODEL_NODE nodes[7];        /* +0x110 */
} TRUCK_ACTOR;

#if defined(AP_32BIT)
typedef char TruckActor_size_3b0[sizeof(TRUCK_ACTOR) == 0x3B0 ? 1 : -1];
typedef char TruckActor_effect_at_ec[offsetof(TRUCK_ACTOR, projectile_effect) == 0xEC ? 1 : -1];
typedef char TruckActor_nodes_at_110[offsetof(TRUCK_ACTOR, nodes) == 0x110 ? 1 : -1];
#endif

/* Resident PAL 0x800AE7BC..0x800AEA54, reached by TRUCK.BIN destruction. */

typedef struct
{
    COLLISION collision; /* +0x00 */
    uint8 field_78[4];   /* +0x78 */
    MODEL_NODE *node;    /* +0x7C */
    uint16 age;          /* +0x80 */
    sint16 tag;          /* +0x82 */
    sint32 velocity_y;   /* +0x84 */
    sint16 screen_depth; /* +0x88 */
    uint8 field_8a[2];   /* +0x8A */
} TANK_DESTRUCTION_ACTOR;

#if defined(AP_32BIT)
typedef char TankDestructionActor_size_8c[sizeof(TANK_DESTRUCTION_ACTOR) == 0x8C ? 1 : -1];
#endif

/* Functions. */
/* Original: TRUCK_800FB128. */
static void truck_update(TRUCK_ACTOR *t)
{
    EFFECT *source = t->owner_effect;
    sint32 old_velocity = t->velocity_x, velocity, target_distance, abs_old, abs_new, speed;
    if (!world_object_is_visible(t->collision.x, t->collision.y, t->collision.z, 0x7d000))
    {
        object_destroy(t);
        source->type = 0x22;
    }
    if (t->engine_sound_ticks != 0)
        --t->engine_sound_ticks;
    else
    {
        sound_play_positional(0x53, 0, 0x7f, t->collision.x, t->collision.y, t->collision.z);
        t->engine_sound_ticks = 0x19;
    }
    target_distance = abs_s32(t->origin_x - t->collision.x);
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
            t->target_velocity_x = t->travel_direction_x * 2;
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
                sound_play_positional(0x51, 0, 0x7f, t->collision.x, t->collision.y, t->collision.z);
            if (t->stop_sound_ticks == 4)
                sound_play_positional(0x52, 0, 0x7f, t->collision.x, t->collision.y, t->collision.z);
            --t->stop_sound_ticks;
        }
        if (t->stop_sound_ticks == 0)
        {
            velocity = t->velocity_x;
            if (t->collision.x >= t->target_x)
            {
                if (velocity >= -0x3ff)
                    velocity -= 0x20;
            }
            else if (velocity < 0x400)
                velocity += 0x20;
            t->velocity_x = velocity;
            if ((g_frame_counter & 7) == 0)
            {
                SPRITE *p = world_sprite_create(t->nodes[5].world_x, t->nodes[5].world_y - 0x800, t->nodes[5].world_z, (const sint32 *)player_assets_executable_address(0x800c87b4u));
                p->acceleration_y = -10;
                p->scale_x = 0x800;
                p->scale_step = 0x40;
                p->field_a8 = 0x80;
                p->field_aa = -7;
                p->lifetime = 0x3c;
                p->rotation_step_x = 4;
                p->velocity_x = t->velocity_x + 0x300;
                p->clut_override = (uint16)g_white_signal_flare_clut;
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
            sound_play_positional(velocity < 0 ? 0x50 : 0x51, 0, 0x7f, t->collision.x, t->collision.y, t->collision.z);
            t->braking_sound_ticks = 0x80;
        }
    }
    t->collision.x += velocity;
    t->nodes[0].rotation_x = (sint16)(((-velocity + (velocity > 0 ? 0x7f : 0)) >> 5) & 0xffc);
    t->nodes[0].rotation_y = 0xc00;
    t->nodes[0].model_id = g_truck_model_base;
    t->nodes[0].translation_x = t->collision.x - g_camera_world_x;
    t->nodes[0].translation_y = t->collision.y - g_camera_world_y - 0x6000;
    t->nodes[0].translation_z = t->collision.z - g_camera_world_z;
    t->nodes[6].rotation_x = (sint16)(((velocity + (velocity < 0 ? 0x7f : 0)) >> 5) & 0xffc);
    g_render_depth_bucket = 0x480 - ((g_render_row_world_z - t->collision.z + (g_model_descriptors[g_truck_model_base].half_y << 8)) / 0x100);
    g_current_model_world_z = t->collision.z;
    t->nodes[1].rotation_x = (sint16)((t->nodes[1].rotation_x + velocity / 0x10) & 0xfff);
    t->nodes[4].rotation_x = t->nodes[3].rotation_x = t->nodes[2].rotation_x = t->nodes[1].rotation_x;
    if (t->flash_clut_ticks != 0)
    {
        --t->flash_clut_ticks;
        g_model_clut_override = g_hit_flash_clut;
    }
    g_model_render_frame = g_current_render_frame;
    model_render_node(&t->nodes[0], (MATRIX *)player_assets_executable_address(0x800c8decu));
    t->collision_min_x = t->collision.x - 0x9600;
    t->collision_max_x = t->collision.x + 0x9600;
    t->collision_max_z = t->collision.z + 0xc00;
    g_model_clut_override = 0;
    t->collision_min_z = t->collision.z - 0x2000;
    t->collision_min_y = t->collision.y - 0x8000;
    if (velocity < 0)
    {
        EFFECT *shot = &t->projectile_effect;
        shot->x = t->collision.x + 0x8000;
        shot->y = t->collision.y - 0x6400;
        shot->z = t->collision.z;
        {
            MISSION_ENEMY_ACTOR *object = mission_enemy_create(shot);
            if (object)
            {
                object->scale_transition = 0;
                object->vertical_velocity = -0x300;
                object->palette_offset = 0;
            }
        }
    }
    hud_bar_render(g_boss_health_bar, t->health, 0, 0);
}

/* Original: FUN_800AE844. */
static void vehicle_destruction_actor_update(TANK_DESTRUCTION_ACTOR *o)
{
    sint32 delta;
    o->collision.y += o->velocity_y;
    o->node->translation_x = o->collision.x - g_camera_world_x;
    o->node->translation_y = o->collision.y - g_camera_world_y;
    o->node->translation_z = o->collision.z - g_camera_world_z;
    g_model_render_frame = 0;
    model_render_begin();
    delta = g_render_row_world_z - o->collision.z;
    g_render_depth_bucket = 0x480 - delta / 0x100;
    g_render_depth_bucket = o->screen_depth;
    g_current_model_world_z = o->collision.z;
    model_render_node(o->node, (MATRIX *)player_assets_executable_address(0x800c8decu));
    model_render_end();
    ++o->age;
    if ((sint16)o->age > 0x100)
        object_destroy(o);
    if ((sint16)o->age == 0xfa)
    {
        sint32 half = o->collision.width / 2;
        expl_flash_create(o->collision.x - half, o->collision.y, o->collision.z - 0x4000);
        expl_flash_create(o->collision.x + half, o->collision.y, o->collision.z - 0x4000);
        sound_play_nonpositional(9, 0, 0x7f);
        camera_shake_start();
    }
    if ((g_frame_counter & 7) == 0)
    {
        sint32 x = o->collision.x + random_range(o->collision.box_x);
        sint32 y = o->collision.y + 0x4000 + random_range(o->collision.box_y);
        timed_callback_create(expl_debris_particle_create, x, y, o->collision.z, 0x0c, 2);
    }
}

/* Original: FUN_800AE7BC. */
void *vehicle_destruction_actor_create(sint32 x, sint32 y, sint32 z, MODEL_NODE *node, sint16 tag, sint16 depth)
{
    TANK_DESTRUCTION_ACTOR *o = (TANK_DESTRUCTION_ACTOR *)object_create(sizeof(*o), (FUNC_COLLISION_UPDATE)vehicle_destruction_actor_update);
    o->node = node;
    o->collision.x = x;
    o->collision.y = y;
    o->collision.z = z;
    o->tag = tag;
    o->screen_depth = depth;
    return o;
}

/* Original: TRUCK_800FB7B4. */
static void truck_damage(COLLISION *collision, COLLISION *source)
{
    TRUCK_ACTOR *t = (TRUCK_ACTOR *)collision;
    if (object_damage_apply(t, source) == 0)
        return;
    linked_list_unlink(g_object_list, t);
    {
        sint32 height = (g_render_row_world_z - t->collision.z + (g_model_descriptors[g_truck_model_base].half_y << 8));
        void *blast = vehicle_destruction_actor_create(t->collision.x, t->collision.y - 0x6000, t->collision.z, &t->nodes[0], 7, (sint16)(0x480 - height / 0x100));
        collision_box_set(blast, 0x8c, 0x64, 0x46);
    }
    ++g_objective_counts[0];
}

/* Original: TRUCK_800FADD4. */
void truck_create(EFFECT *effect)
{
    TRUCK_ACTOR *truck;
    OriginalModelDescriptor *descriptor;
    sint32 direction;
    if (g_player_world_y != effect->y + 0x2000)
    {
        effect->values[0] = 0;
        return;
    }
    effect->values[0] = (uint16)(effect->values[0] + 1);
    if ((sint16)effect->values[0] < 30)
        return;
    music_track_select(9, 1);
    truck = (TRUCK_ACTOR *)runtime_heap_allocate(sizeof(*truck));
    truck->collision.update = (FUNC_COLLISION_UPDATE)truck_update;
    linked_list_append(g_object_list, truck);
    setPolyFT4(truck->collision.prim);
    setShadeTex(truck->collision.prim, 0);
    truck->owner_effect = effect;
    truck->collision.x = effect->x + 0x25800;
    effect->type = 0;
    truck->collision.y = effect->y + 0x3600;
    descriptor = &g_model_descriptors[g_truck_model_base];
    truck->collision.z = effect->z - (descriptor->half_y << 8);
    truck->collision.z += descriptor->half_y << 8;
    truck->origin_x = effect->x;
    truck->target_x = effect->x;
    direction = effect->x < truck->collision.x ? -0x400 : 0x400;
    truck->target_velocity_x = direction;
    truck->velocity_x = direction;
    truck->travel_direction_x = direction;
    truck->wait_ticks = (sint16)0xea60;
    truck->acceleration_x = 4;
    truck->collision.receives_mask = 0x43;
    truck->collision.callback_18 = (FUNC_COLLISION_CALLBACK)truck_damage;
    truck->health = 0x190;
    truck->collision.callback_14 = player_apply_damage;
    collision_box_set(truck, 0x8c, 0x64, 0x46);
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
    truck->nodes[4].field_56 = 0x2000;
    model_attach_child(&truck->nodes[6], &truck->nodes[0]);
    model_attach_child(&truck->nodes[1], &truck->nodes[6]);
    model_attach_child(&truck->nodes[2], &truck->nodes[6]);
    model_attach_child(&truck->nodes[4], &truck->nodes[6]);
    model_attach_child(&truck->nodes[5], &truck->nodes[0]);
    truck->nodes[5].translation_z = -0x7400;
    truck->nodes[5].translation_y = 0x3800;
    truck->projectile_effect.type = 0x27;
    truck->projectile_effect.values[0] = 0x23;
    truck->projectile_effect.values[1] = effect->values[0];
    truck->projectile_effect.values[2] = effect->values[0];
    hud_bar_initialize(g_boss_health_bar, -0x28, -0x60, 0x50, 0x190);
}
