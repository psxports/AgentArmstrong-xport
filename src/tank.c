#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "airship.h"
#include "app.h"
#include "audio/game_sound.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "hq.h"
#include "mechanoid.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "runtime_heap.h"
#include "tank.h"
#include "truck.h"

/* Types. */
/* Resident PAL tank chain used by JUNGLE/M23.CC, exact ranges
 * 0x800ADB84..0x800AE7BC. */

typedef struct
{
    COLLISION collision;              /* +0x000 */
    sint16 health;                    /* +0x078 */
    sint16 flash_clut_ticks;          /* +0x07A */
    uint8 field_07c[4];               /* +0x07C */
    sint32 velocity_x;                /* +0x080 */
    sint32 target_velocity_x;         /* +0x084 */
    uint8 field_088[0x10];            /* +0x088 */
    sint32 target_heading;            /* +0x098 */
    uint8 field_09c[4];               /* +0x09C */
    sint32 velocity_z;                /* +0x0A0 */
    sint32 target_velocity_z;         /* +0x0A4 */
    sint32 acceleration;              /* +0x0A8 */
    uint8 field_0ac[4];               /* +0x0AC */
    sint16 fire_ticks;                /* +0x0B0 */
    sint16 movement_sound_handle;     /* +0x0B2 */
    sint16 field_0b4;                 /* +0x0B4 */
    uint8 field_0b6[6];               /* +0x0B6 */
    sint16 field_0bc;                 /* +0x0BC */
    uint8 field_0be[2];               /* +0x0BE */
    sint16 field_0c0;                 /* +0x0C0 */
    sint16 field_0c2;                 /* +0x0C2 */
    MODEL_NODE nodes[11];             /* +0x0C4 */
    uint8 field_4e4[0x60];            /* +0x4E4 */
    MODEL_NODE *node_table[11];       /* +0x544 */
    uint8 field_570[8];               /* +0x570 */
    EFFECT *owner_effect;             /* +0x578 */
    sint16 heading_step;              /* +0x57C */
    uint8 field_57e[2];               /* +0x57E */
    sint32 patrol_min_x;              /* +0x580 */
    sint32 patrol_max_x;              /* +0x584 */
    sint32 saved_node3_translation_z; /* +0x588 */
    EFFECT *route_points[16];         /* +0x58C */
    sint16 route_count;               /* +0x5CC */
    sint16 route_direction;           /* +0x5CE */
    sint16 route_index;               /* +0x5D0 */
    sint16 damage_smoke_active;       /* +0x5D2 */
    uint8 field_5d4[0x18];            /* +0x5D4 */
    sint32 movement_vector[3];        /* +0x5EC */
    sint32 route_delta_x;             /* +0x5F8 */
    uint8 field_5fc[4];               /* +0x5FC */
    sint32 route_delta_z;             /* +0x600 */
} TANK_ACTOR;

#if defined(AP_32BIT)
typedef char TankActor_size_604[sizeof(TANK_ACTOR) == 0x604 ? 1 : -1];
typedef char TankActor_nodes_at_c4[offsetof(TANK_ACTOR, nodes) == 0xC4 ? 1 : -1];
typedef char TankActor_table_at_544[offsetof(TANK_ACTOR, node_table) == 0x544 ? 1 : -1];
typedef char TankActor_routes_at_58c[offsetof(TANK_ACTOR, route_points) == 0x58C ? 1 : -1];
#endif

/* Macros. */
#define fixed_dda_advance(p) fixed_dda_advance((sint32 *)(p))

/* Functions. */
static sint32 qd(sint32 x, sint32 d)
{
    return x < 0 ? -((-x) / d) : x / d;
}

/* Original: FUN_800AE68C. */
static void tank_damage(COLLISION *collision, COLLISION *hit)
{
    TANK_ACTOR *o = (TANK_ACTOR *)collision;
    void *blast;
    sint32 depth;
    if ((hit->receives_mask & 2) != 0)
        return;
    o->flash_clut_ticks = 1;
    if (object_damage_apply(o, hit))
    {
        linked_list_unlink(g_object_list, o);
        sound_voice_stop(&o->movement_sound_handle);
        g_camera_depth_offset_target = 0;
        --g_objective_counts[12];
        depth = 0x480 - qd(g_render_row_world_z + 0x2600 - (o->collision.z + o->collision.box_z), 0x100);
        blast = vehicle_destruction_actor_create(o->collision.x, o->collision.y, o->collision.z, &o->nodes[0], 11, (sint16)depth);
        hierarchy_collision_box_update(blast, &o->nodes[7], 4);
        sound_play_positional(0x67, -5, 0x8c, o->collision.x, o->collision.y, o->collision.z);
    }
    else if (o->health < 201)
        o->damage_smoke_active = 1;
}

/* Original: FUN_800ADD80. */
static void tank_update(TANK_ACTOR *o)
{
    EFFECT *point;
    void *x;
    sint32 line[7], tx, tz, dx, dz, m, angle, depth;
    static sint32 traced;
#ifdef AP_WIN
    if (!traced && getenv("OA_STAGE23_TRACE"))
    {
        uint8 before = g_objective_counts[12];
        FILE *f = fopen("../status/spank-the-tank-native.log", "a");
        if (f)
        {
            fprintf(f, "TANK_NATIVE_UPDATE xyz=%d,%d,%d health=%d route=%d\n", o->collision.x, o->collision.y, o->collision.z, o->health, o->route_index);
            fclose(f);
        }
        o->collision.receives_mask = 0;
        tank_damage(&o->collision, &o->collision);
        f = fopen("../status/spank-the-tank-native.log", "a");
        if (f)
        {
            fprintf(f, "TANK_NATIVE_DAMAGE health=%d counter12_before=%u counter12_after=%u\n", o->health, (unsigned)before, (unsigned)g_objective_counts[12]);
            fclose(f);
        }
        traced = 1;
        return;
    }
#endif
    if (!world_object_is_visible(o->collision.x, o->collision.y, o->collision.z, 0x89800))
    {
        sound_voice_stop(&o->movement_sound_handle);
        object_destroy(o);
        o->owner_effect->type = 0x36;
        g_camera_depth_offset_target = 0;
        return;
    }
    point = o->route_points[o->route_index];
    tx = point->x;
    tz = point->z;
    if (tx - 0x4000 < o->collision.x && o->collision.x < tx + 0x4000 && tz - 0x4000 < o->collision.z && o->collision.z < tz + 0x4000)
    {
        o->target_velocity_x = 0;
        o->target_velocity_z = 0;
    }
    if (tx - 0x800 < o->collision.x && o->collision.x < tx + 0x800 && tz - 0x800 < o->collision.z && o->collision.z < tz + 0x800)
    {
        o->route_index += o->route_direction;
        if (o->route_index == o->route_count - 1)
            o->route_direction = -1;
        if (o->route_index == 0)
            o->route_direction = 1;
        point = o->route_points[o->route_index];
        fixed_dda_initialize(o->collision.x, o->collision.y, o->collision.z, point->x, o->collision.y, point->z, line);
        angle = fixed_angle_from_vector(qd(line[3], 0x800), qd(line[5], 0x800)) << 2;
        o->target_heading = angle;
        o->heading_step = 1;
        o->route_delta_x = qd(line[3], 16);
        o->route_delta_z = qd(line[5], 16);
        o->target_velocity_x = qd(o->route_delta_x * o->acceleration, 16);
        o->target_velocity_z = qd(o->route_delta_z * o->acceleration, 16);
        o->velocity_x = o->velocity_z = 0;
        sound_play_positional(0x65, 0, 0x7f, o->collision.x, o->collision.y, o->collision.z);
        if (!player_inventory_find(0x0b, g_player))
        {
            PLAYER_ACTION *item = player_action_object_create(o->collision.x, o->collision.y, o->collision.z, 0x0b);
            if (item)
                item->lifetime = 0x258;
        }
        if (g_player->health < 0x100)
        {
            PLAYER_ACTION *item = player_action_object_create(o->collision.x - 0x4000, o->collision.y, o->collision.z, 0x15);
            if (item)
                item->lifetime = 0x258;
        }
    }
    if ((uint16)((uint16)o->fire_ticks - 11) < 79)
    {
        fixed_dda_initialize(o->nodes[1].world_x, o->nodes[1].world_y, o->nodes[1].world_z, g_player_world_x, g_player_world_y - 0x3000, g_player_world_z, line);
        angle = (fixed_angle_from_vector(qd(line[3], 0x800), qd(line[5], 0x800)) << 2) - o->nodes[0].rotation_y;
        o->nodes[1].rotation_y = angle_approach_wrapped((sint16)(angle & 0xfff), 0x18, o->nodes[1].rotation_y);
        if (o->fire_ticks == 0x59)
            sound_play_positional(0x66, 0, 0x78, o->collision.x, o->collision.y, o->collision.z);
    }
    if (o->nodes[0].rotation_y == o->target_heading)
    {
        fixed_dda_advance(o->movement_vector);
        o->collision.x += o->velocity_x;
        o->collision.z += o->velocity_z;
        dx = abs_s32(qd(o->route_delta_x, 0x100));
        dz = abs_s32(qd(o->route_delta_z, 0x100));
        if (o->velocity_x < o->target_velocity_x)
            o->velocity_x += dx;
        if (o->target_velocity_x < o->velocity_x)
            o->velocity_x -= dx;
        if (o->velocity_z < o->target_velocity_z)
            o->velocity_z += dz;
        if (o->target_velocity_z < o->velocity_z)
            o->velocity_z -= dz;
    }
    else
    {
        o->nodes[0].rotation_y = angle_approach_wrapped((sint16)o->target_heading, o->heading_step, o->nodes[0].rotation_y);
        if (o->heading_step < 12 && (g_frame_counter & 3) == 0)
            ++o->heading_step;
    }
    o->nodes[0].translation_x = o->collision.x - g_camera_world_x;
    o->nodes[0].translation_y = o->collision.y - g_camera_world_y;
    o->nodes[0].translation_z = o->collision.z - g_camera_world_z;
    fixed_dda_initialize(o->nodes[4].world_x, o->nodes[4].world_y, o->nodes[4].world_z, g_player_world_x, g_player_world_y, g_player_world_z, line);
    m = abs_s32(line[3]);
    dz = abs_s32(line[5]);
    m = m < dz ? dz + m / 2 : m + dz / 2;
    angle = fixed_angle_from_vector(qd(line[4], 0x800), -abs_s32(qd(m, 0x800))) << 2;
    o->nodes[4].rotation_x = angle_approach_wrapped((sint16)((angle - 0x400) & 0xffc), 12, o->nodes[4].rotation_x);
    g_model_render_frame = 0;
    model_render_begin();
    hierarchy_collision_box_update(o, &o->nodes[7], 4);
    depth = g_render_row_world_z + 0x2600 - (o->collision.z + o->collision.box_z);
    g_render_depth_bucket = 0x480 - qd(depth, 0x100);
    g_current_model_world_z = o->collision.z;
    if (o->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --o->flash_clut_ticks;
    }
    model_render_node(&o->nodes[0], (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
    model_render_end();
    if (o->fire_ticks == 1)
    {
        PROJECTILE *p = projectile_create(o->nodes[5].world_x, o->nodes[5].world_y, o->nodes[5].world_z, o->nodes[6].world_x, o->nodes[6].world_y, o->nodes[6].world_z);
        sound_play_positional(0x67, 0, 0x96, o->collision.x, o->collision.y, o->collision.z);
        if (p)
        {
            p->damage = 0x80;
            p->floor_callback = (FUNC_COLLISION_UPDATE)explosive_projectile_trail_update;
            p->impact_callback = (FUNC_COLLISION_UPDATE)explosive_projectile_impact;
            p->collision.sends_mask |= 1;
            p->collision.receives_mask |= 4;
            p->collision.object_type = 0;
        }
        speech_random_request(6);
    }
    if (o->fire_ticks == 0)
        o->fire_ticks = (sint16)(random_range(100) + 130);
    --*(uint16 *)&o->fire_ticks;
    if (o->damage_smoke_active && (g_frame_counter & 7) == 0)
        timed_callback_create(expl_debris_particle_create, o->collision.x, o->collision.y, o->collision.z, 8, 2);
    if (o->movement_sound_handle != -1)
    {
        x = sound_spatial_volume_calculate(0x7f, o->collision.x, o->collision.y, o->collision.z);
        if (x)
            sound_voice_spatial_volume_update(x, o->movement_sound_handle);
    }
    g_camera_depth_offset_target = o->collision.z < g_player_world_z ? 0x40 : 0;
    hud_bar_render(g_boss_health_bar, o->health, 0, 0);
}

/* Original: FUN_800ADB84. */
void tank_create(EFFECT *e)
{
    TANK_ACTOR *o = (TANK_ACTOR *)runtime_heap_allocate(sizeof(*o));
    const sint8 *idx = (const sint8 *)player_assets_executable_address(0x800cda2c);
    sint32 i;
    e->type = 0;
    o->collision.update = (FUNC_COLLISION_UPDATE)tank_update;
    linked_list_append(g_object_list, o);
    for (i = 0; i < 11; i++)
    {
        MODEL_NODE *n = &o->nodes[i];
        o->node_table[i] = n;
        n->model_id = idx[i] == -1 ? 0 : g_tank_model_base + idx[i];
        n->render_flags = 0x3f;
    }
    model_initialize_pose(g_tank_initial_pose, o->node_table, 1);
    o->owner_effect = e;
    o->collision.z = e->z;
    o->collision.x = e->x;
    o->collision.y = e->y - 0xe00;
    o->collision.receives_mask = 0x43;
    o->collision.callback_18 = (FUNC_COLLISION_CALLBACK)tank_damage;
    o->collision.callback_14 = player_apply_damage;
    o->health = 0x258;
    o->nodes[0].scale = 0xc00;
    o->fire_ticks = 0xfa;
    o->field_0b4 = -1;
    o->patrol_max_x = e->x + 0x10a00;
    o->saved_node3_translation_z = o->nodes[3].translation_z;
    o->patrol_min_x = e->x - 0x10a00;
    o->field_0bc = 0x10;
    o->field_0c0 = 500;
    o->field_0c2 = 200;
    o->route_points[0] = e;
    o->route_count = (sint16)(route_waypoints_collect((sint16)e->values[0], 0, (void **)o->route_points) + 1);
    o->route_direction = 1;
    o->movement_sound_handle = (sint16)sound_play_positional(0x64, 0, 0x7f, o->collision.x, o->collision.y, o->collision.z);
    o->acceleration = 3;
    hud_bar_initialize(g_boss_health_bar, -0x28, -0x60, 0x50, 0x258);
#ifdef AP_WIN
    if (getenv("OA_STAGE23_TRACE"))
    {
        FILE *f = fopen("../status/spank-the-tank-native.log", "w");
        if (f)
        {
            fprintf(f, "TANK_NATIVE_CTOR xyz=%d,%d,%d health=%d route_count=%d damage=800ae68c update=800add80\n", o->collision.x, o->collision.y, o->collision.z, o->health, o->route_count);
            fclose(f);
        }
    }
#endif
}
