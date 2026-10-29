#include "spatial.h"
#include "game_math.h"
#include "sprite.h"
#include "render.h"
#include "airship.h"
#include "game_sound.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "camera.h"
#include "hq.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "runtime_heap.h"
#include "tank.h"
#include "truck.h"

/* Original: DAT_800D4054. */
MODEL_POSE_RESOURCE g_tank_initial_pose;

/* Original: DAT_800D404E. */
sint16 g_tank_model_base;

/* Types. */
/* Resident PAL tank chain used by JUNGLE/M23.CC, exact ranges
 * 0x800ADB84..0x800AE7BC. */

typedef struct
{
    COLLISION col;
    sint32 velocity_x;
    sint32 target_velocity_x;
    sint32 target_heading;
    sint32 velocity_z;
    sint32 target_velocity_z;
    sint32 acceleration;
    sint16 fire_ticks;
    sint16 movement_sound_handle;
    MODEL_NODE nodes[11];
    MODEL_NODE *node_table[11];
    EFFECT *owner_effect;
    sint16 heading_step;
    EFFECT *route_points[16];
    sint16 route_count;
    sint16 route_dir;
    sint16 route_idx;
    sint16 damage_smoke_active;
    /* Original 0x5EC DDA position followed by the three movement steps */
    FIXED_DDA_LINE movement_line;
} TANK_ACTOR;

/* Original data: SLES_004.74:0x800CDA2C..0x800CDA36 */
static const sint8 tank_model_offsets[11] = {
    2, 1, 0, -1, -1, -1, -1, -1, -1, -1, -1,
};

/* Functions. */
/* Original: FUN_800AE68C. */
static void tank_damage(void *raw_col, void *raw_hit)
{
    TANK_ACTOR *actor = raw_col;
    COLLISION *hit = raw_hit;
    void *blast;
    sint32 depth;
    if ((hit->receives_mask & 2) != 0)
        return;
    actor->col.flash_clut_ticks = 1;
    if (object_damage_apply(actor, hit))
    {
        linked_list_unlink(&g_object_list, actor);
        sound_voice_stop(&actor->movement_sound_handle);
        g_camera.depth_offset_target = 0;
        --g_mission.counts[12];
        depth = 0x480 - div_trunc((g_map_render.depth_anchor_z + 0x2600u - ((uint32)actor->col.z + actor->col.box_z)), 0x100);
        blast = vehicle_destruction_actor_create(actor->col.x, actor->col.y, actor->col.z, &actor->nodes[0], 11, depth);
        hierarchy_col_box_update(blast, &actor->nodes[7], 4);
        sound_play_positional(0x67, -5, 0x8c, actor->col.x, actor->col.y, actor->col.z);
    }
    else if (actor->col.hit.health < 201)
        actor->damage_smoke_active = 1;
}

/* Original: FUN_800ADD80. */
static void tank_update(void *raw_actor)
{
    TANK_ACTOR *actor = raw_actor;
    EFFECT *point;
    const SOUND_VOLUME_PAIR *volume;
    FIXED_DDA_LINE line;
    sint32 tx, tz, dx, dz, m, angle, depth;
    if (!world_object_is_visible(actor->col.x, actor->col.y, actor->col.z, 0x89800))
    {
        sound_voice_stop(&actor->movement_sound_handle);
        object_destroy(actor);
        actor->owner_effect->type = 0x36;
        g_camera.depth_offset_target = 0;
    }
    point = actor->route_points[actor->route_idx];
    tx = point->x;
    tz = point->z;
    if ((tx - 0x4000) < actor->col.x && actor->col.x < (tx + 0x4000) && (tz - 0x4000) < actor->col.z && actor->col.z < (tz + 0x4000))
    {
        actor->target_velocity_x = 0;
        actor->target_velocity_z = 0;
    }
    if ((tx - 0x800) < actor->col.x && actor->col.x < (tx + 0x800) && (tz - 0x800) < actor->col.z && actor->col.z < (tz + 0x800))
    {
        actor->route_idx += actor->route_dir;
        if (actor->route_idx == actor->route_count - 1)
            actor->route_dir = -1;
        if (actor->route_idx == 0)
            actor->route_dir = 1;
        point = actor->route_points[actor->route_idx];
        fixed_dda_initialize(actor->col.x, actor->col.y, actor->col.z, point->x, actor->col.y, point->z, &actor->movement_line);
        angle = fixed_angle_from_vector(div_trunc(actor->movement_line.step.x, 0x800), div_trunc(actor->movement_line.step.z, 0x800)) << 2;
        actor->target_heading = angle;
        actor->heading_step = 1;
        actor->movement_line.step.x = div_trunc(actor->movement_line.step.x, 16);
        actor->movement_line.step.z = div_trunc(actor->movement_line.step.z, 16);
        actor->target_velocity_x = div_trunc(actor->movement_line.step.x * actor->acceleration, 16);
        actor->target_velocity_z = div_trunc(actor->movement_line.step.z * actor->acceleration, 16);
        actor->velocity_x = actor->velocity_z = 0;
        sound_play_positional(0x65, 0, 0x7f, actor->col.x, actor->col.y, actor->col.z);
        INVENTORY_SLOT *inventory = player_inventory_find(0x0b, g_player);
        if (inventory == 0 || inventory->count == 0)
        {
            PLAYER_ACTION *item = player_action_object_create(actor->col.x, actor->col.y, actor->col.z, 0x0b);
            item->lifetime = 0x258;
        }
        if (g_player->health < 0x100)
        {
            PLAYER_ACTION *item = player_action_object_create(actor->col.x - 0x4000u, actor->col.y, actor->col.z, 0x15);
            item->lifetime = 0x258;
        }
    }
    if (actor->fire_ticks >= 11 && actor->fire_ticks < 90)
    {
        fixed_dda_initialize(actor->nodes[1].world_x, actor->nodes[1].world_y, actor->nodes[1].world_z, g_player_world.x, g_player_world.y - 0x3000u, g_player_world.z, &line);
        angle = (fixed_angle_from_vector(div_trunc(line.step.x, 0x800), div_trunc(line.step.z, 0x800)) << 2) - actor->nodes[0].rotation_y;
        actor->nodes[1].rotation_y = angle_approach_wrapped(angle & 0xfff, 0x18, actor->nodes[1].rotation_y);
        if (actor->fire_ticks == 0x59)
            sound_play_positional(0x66, 0, 0x78, actor->col.x, actor->col.y, actor->col.z);
    }
    if (actor->nodes[0].rotation_y == actor->target_heading)
    {
        fixed_dda_advance(&actor->movement_line);
        actor->col.x = (uint32)actor->col.x + actor->velocity_x;
        actor->col.z = (uint32)actor->col.z + actor->velocity_z;
        dx = abs_s32(div_trunc(actor->movement_line.step.x, 0x100));
        dz = abs_s32(div_trunc(actor->movement_line.step.z, 0x100));
        if (actor->velocity_x < actor->target_velocity_x)
            actor->velocity_x += dx;
        if (actor->target_velocity_x < actor->velocity_x)
            actor->velocity_x -= dx;
        if (actor->velocity_z < actor->target_velocity_z)
            actor->velocity_z += dz;
        if (actor->target_velocity_z < actor->velocity_z)
            actor->velocity_z -= dz;
    }
    else
    {
        actor->nodes[0].rotation_y = angle_approach_wrapped(actor->target_heading, actor->heading_step, actor->nodes[0].rotation_y);
        if (actor->heading_step < 12 && (g_render_state.frame & 3) == 0)
            ++actor->heading_step;
    }
    actor->nodes[0].translation_x = (uint32)actor->col.x - g_camera.world_x;
    actor->nodes[0].translation_y = (uint32)actor->col.y - g_camera.world_y;
    actor->nodes[0].translation_z = (uint32)actor->col.z - g_camera.world_z;
    fixed_dda_initialize(actor->nodes[4].world_x, actor->nodes[4].world_y, actor->nodes[4].world_z, g_player_world.x, g_player_world.y, g_player_world.z, &line);
    m = abs_s32(line.step.x);
    dz = abs_s32(line.step.z);
    m = m < dz ? dz + (uint32)div_pow2_trunc(m, 1) : m + (uint32)div_pow2_trunc(dz, 1);
    angle = fixed_angle_from_vector(div_trunc(line.step.y, 0x800), -abs_s32(div_trunc(m, 0x800))) << 2;
    actor->nodes[4].rotation_x = angle_approach_wrapped((angle - 0x400) & 0xffc, 12, actor->nodes[4].rotation_x);
    g_model_draw.target_frame = 0;
    model_render_begin();
    hierarchy_col_box_update(&actor->col, &actor->nodes[7], 4);
    depth = g_map_render.depth_anchor_z + 0x2600u - ((uint32)actor->col.z + actor->col.box_z);
    g_model_draw.depth_bucket = 0x480 - div_trunc(depth, 0x100);
    g_model_draw.world_z = actor->col.z;
    if (actor->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --actor->col.flash_clut_ticks;
    }
    model_render_node(&actor->nodes[0], &g_model_identity);
    g_model_draw.clut_override = 0;
    model_render_end();
    if (actor->fire_ticks == 1)
    {
        PROJECTILE *projectile;
        sound_play_positional(0x67, 0, 0x96, actor->col.x, actor->col.y, actor->col.z);
        projectile = projectile_create(actor->nodes[5].world_x, actor->nodes[5].world_y, actor->nodes[5].world_z, actor->nodes[6].world_x, actor->nodes[6].world_y, actor->nodes[6].world_z);
        projectile->col.hit.damage = 0x80;
        projectile->floor_callback = explosive_projectile_trail_update;
        projectile->impact_callback = explosive_projectile_impact;
        projectile->col.sends_mask |= 1;
        projectile->col.object_type = 0;
        projectile->col.receives_mask |= 4;
        speech_random_request(6);
    }
    if (actor->fire_ticks == 0)
        actor->fire_ticks = random_range(100) + 130;
    --actor->fire_ticks;
    if (actor->damage_smoke_active && (g_render_state.frame & 7) == 0)
        timed_callback_create(expl_debris_particle_create, actor->col.x, actor->col.y, actor->col.z, 8, 2);
    if (actor->movement_sound_handle != -1)
    {
        volume = sound_spatial_volume_calculate(0x7f, actor->col.x, actor->col.y, actor->col.z);
        if (volume)
            sound_voice_spatial_volume_update(volume, actor->movement_sound_handle);
    }
    g_camera.depth_offset_target = actor->col.z < g_player_world.z ? 0x40 : 0;
    hud_bar_render(&g_boss_health_bar, actor->col.hit.health, 0, 0);
}

/* Original: FUN_800ADB84. */
void tank_create(EFFECT *effect)
{
    TANK_ACTOR *actor;
    sint32 i;
    effect->type = 0;
    actor = runtime_heap_allocate(sizeof(*actor));
    actor->col.object.update = tank_update;
    linked_list_append(&g_object_list, actor);
    for (i = 0; i < 11; i++)
    {
        MODEL_NODE *node = &actor->nodes[i];
        actor->node_table[i] = node;
        node->model_id = tank_model_offsets[i] == -1 ? 0 : g_tank_model_base + tank_model_offsets[i];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_tank_initial_pose, actor->node_table, 11, 1);
    actor->owner_effect = effect;
    actor->col.z = effect->z;
    actor->col.x = effect->x;
    actor->col.y = effect->y - 0xe00u;
    actor->col.receives_mask = 0x43;
    actor->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    actor->col.hit_callback = tank_damage;
    actor->col.player_contact_callback = player_apply_damage;
    actor->col.hit.health = 0x258;
    actor->nodes[0].scale = 0xc00;
    actor->fire_ticks = 0xfa;
    actor->route_points[0] = effect;
    actor->route_count = route_waypoints_collect(effect->vals[0], 0, actor->route_points, sizeof(actor->route_points) / sizeof(actor->route_points[0])) + 1;
    actor->route_dir = 1;
    actor->movement_sound_handle = sound_play_positional(0x64, 0, 0x7f, actor->col.x, actor->col.y, actor->col.z);
    actor->acceleration = 3;
    hud_bar_initialize(&g_boss_health_bar, -0x28, -0x60, 0x50, 0x258);
}
