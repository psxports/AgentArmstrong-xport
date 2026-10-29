#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "game_math.h"
#include "effect_update.h"
#include "render.h"
#include "airship.h"
#include "game_sound.h"
#include "collision.h"
#include "map_render.h"
#include "camera.h"
#include "gyro.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "sprite_renderer.h"

/* Original: DAT_800D406C. */
sint16 g_gyro_model_base;

/* Types. */
typedef struct
{
    COLLISION col;
    sint32 target_x;
    sint32 velocity_x;
    sint32 target_velocity_x;
    sint32 target_pitch;
    sint32 target_y;
    sint32 velocity_y;
    sint32 target_z;
    sint32 velocity_z;
    sint16 attack_countdown;
    sint16 flight_sound_handle;
    sint16 weapon_sound_handle;
    sint16 muzzle_side;
    sint16 attack_side;
    sint16 attack_variant;
    sint16 turn_step;
    sint16 destroyed_ticks;
    sint16 startup_delay;
    sint16 target_refresh;
    MODEL_NODE nodes[12];
    sint32 turn_delta;
    EFFECT *owner_effect;
    sint32 patrol_min_x;
    sint32 patrol_max_x;
} GYRO_ACTOR;

/* Variables. */
static const sint16 gyro_nodes[] = {2, 0, 0, 0, 0, 1, 0, -73, 32, 1, 0, -12, -12, -128, 1, -2, -46, 48, 62, 1, -2, -42, 48, 126, 1, -2, 46, 48, 62, 1, -2, 42, 48, 126, 1, -2, -72, -72, 140, 1, -2, -72, -72, -140, 1, -2, 72, 72, 140, 1, -2, 72, 72, -140, 1, 0, 0, 21, 152, 1, -1};

/* Functions. */
/* Original: GYRO:GYRO_800FAF58. */
static void gyro_boss_update(void *raw_actor)
{
    GYRO_ACTOR *actor = raw_actor;
    EFFECT *effect = actor->owner_effect;
    MODEL_NODE *node;
    sint32 v, target, delta, step, angle;
    const SOUND_VOLUME_PAIR *sound;
    if (actor->startup_delay)
    {
        --actor->startup_delay;
        if (actor->startup_delay == 0)
            speech_random_request(6);
        return;
    }
    v = 355u * ((uint32)actor->col.y - g_camera.world_y);
    delta = (uint32)actor->col.z - g_camera.world_z;
    if (!delta)
    {
#if defined(_DEBUG)
        fputs("GYRO PROJECTION BREAK 7\n", stderr);
#endif
        abort();
    }
    if (v == (-2147483647 - 1) && delta == -1)
    {
#if defined(_DEBUG)
        fputs("GYRO PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    if (v / delta < -400)
    {
        sound_voice_stop(&actor->flight_sound_handle);
        sound_voice_stop(&actor->weapon_sound_handle);
        object_destroy(actor);
        g_camera.depth_offset_target = 0;
        effect->type = 50;
        return;
    }
    if (actor->flight_sound_handle == -1)
        actor->flight_sound_handle = sound_play_positional(0x60, 0, 0x96, actor->col.x, actor->col.y, actor->col.z);
    else
    {
        sound = sound_spatial_volume_calculate(0x96, actor->col.x, actor->col.y, actor->col.z);
        if (sound)
            sound_voice_spatial_volume_update(sound, actor->flight_sound_handle);
    }
    if (actor->attack_countdown && --actor->attack_countdown != 0)
    {
        if (actor->attack_countdown == 290)
            sound_play_positional(0x62, 0, 0xb4, actor->col.x, actor->col.y, actor->col.z);
        if (actor->attack_countdown >= 260)
        {
            actor->target_pitch = 0xf80;
            if (actor->patrol_max_x < actor->col.x)
                actor->target_x = effect->x - 0x10000u;
            if (actor->col.x < actor->patrol_min_x)
                actor->target_x = effect->x + 0x10000u;
            if (actor->col.x < actor->target_x)
                actor->target_velocity_x = 0x800;
            if (actor->target_x < actor->col.x)
                actor->target_velocity_x = -0x800;
        }
        else if (actor->attack_countdown == 259)
        {
            actor->target_pitch = 0xec0;
            actor->target_velocity_x = 0;
            actor->turn_step = 10;
        }
    }
    else
    {
        actor->attack_countdown = 600;
        sound_voice_stop(&actor->weapon_sound_handle);
        actor->attack_side ^= 1;
        actor->turn_step = 16;
        if (!actor->attack_side)
            actor->attack_variant ^= 1;
    }
    if (actor->velocity_x < actor->target_velocity_x)
        actor->velocity_x += 0x40;
    if (actor->target_velocity_x < actor->velocity_x)
        actor->velocity_x -= 0x40;
    actor->col.x = (uint32)actor->col.x + actor->velocity_x;
    if (actor->target_refresh)
        --actor->target_refresh;
    else
    {
        target = effect->y + ((random_range(0x80) - 0x40u) << 8);
        target = target - (actor->attack_side ? 0x4800u : 0x3000u);
        if (actor->attack_countdown >= 501)
            target = target - 0x4000u;
        actor->target_y = target;
        actor->target_refresh = 16;
    }
    if (actor->destroyed_ticks)
    {
        ++actor->destroyed_ticks;
        if ((g_render_state.frame & 0x1f) == 0)
            sound_play_positional(9, 0, 0x93, actor->col.x, actor->col.y, actor->col.z);
        if (actor->destroyed_ticks >= 400)
        {
            actor->target_y = 0;
            actor->target_pitch = -0x200;
        }
        else
            actor->target_y = effect->y - 0x1000u;
        if (actor->destroyed_ticks >= 701)
        {
            sound_voice_stop(&actor->flight_sound_handle);
            object_destroy(actor);
            --g_mission.counts[7];
            return;
        }
    }
    if (actor->target_y < actor->col.y)
        actor->velocity_y -= 0x20;
    if (actor->col.y < actor->target_y)
        actor->velocity_y += 0x20;
    if (actor->velocity_y > 0x300)
        actor->velocity_y = 0x300;
    if (actor->velocity_y < -0x300)
        actor->velocity_y = -0x300;
    actor->col.y = (uint32)actor->col.y + actor->velocity_y;
    if (actor->attack_side)
    {
        g_camera.depth_offset_target = 0x80;
        actor->target_z = effect->z - 0x14a00u;
    }
    else
    {
        actor->target_z = effect->z + 0x8000u;
        if (actor->attack_variant)
            actor->target_z = actor->target_z + 0xc000u;
        g_camera.depth_offset_target = 0;
    }
    if (actor->destroyed_ticks)
    {
        g_camera.depth_offset_target = 0;
        actor->target_z = effect->z + 0x8000u;
    }
    if (actor->target_z < actor->col.z)
        actor->velocity_z -= 0x20;
    if (actor->col.z < actor->target_z)
        actor->velocity_z += 0x20;
    if (actor->velocity_z > 0x200)
        actor->velocity_z = 0x200;
    if (actor->velocity_z < -0x200)
        actor->velocity_z = -0x200;
    actor->col.z = (uint32)actor->col.z + actor->velocity_z;
    if (actor->destroyed_ticks)
    {
        if ((g_render_state.frame & 7) == 0)
        {
            timed_callback_create(expl_debris_particle_create, actor->col.x, actor->col.y, actor->col.z, 8, 2);
            actor->turn_delta = actor->turn_delta + 1u;
        }
    }
    else
    {
        angle = player_angle_from_point(actor->col.x, actor->col.z, 1) << 2;
        delta = angle - actor->nodes[0].rotation_y;
        if (delta < 0)
            delta = -delta;
        actor->turn_delta = 0;
        step = delta;
        if (step > actor->turn_step)
            step = actor->turn_step;
        if (actor->nodes[0].rotation_y != angle)
            actor->turn_delta = angle > actor->nodes[0].rotation_y ? step : -step;
        if (delta >= 0x801)
            actor->turn_delta = -actor->turn_delta;
    }
    actor->nodes[0].rotation_y = ((uint32)actor->nodes[0].rotation_y + actor->turn_delta) & 0xfff;
    if (actor->nodes[0].rotation_x < actor->target_pitch)
        actor->nodes[0].rotation_x += 4;
    if (actor->target_pitch < actor->nodes[0].rotation_x)
        actor->nodes[0].rotation_x -= 4;
    actor->nodes[0].rotation_x &= 0xfff;
    actor->nodes[0].translation_x = (uint32)actor->col.x - g_camera.world_x;
    actor->nodes[0].translation_y = (uint32)actor->col.y - g_camera.world_y;
    actor->nodes[0].translation_z = (uint32)actor->col.z - g_camera.world_z;
    actor->nodes[0].rotation_z = (actor->velocity_x / -5) & 0xfff;
    actor->nodes[11].rotation_y = 0x600;
    v = (g_render_state.frame & 0xf) << 8;
    actor->nodes[1].rotation_y = v;
    actor->nodes[2].rotation_x = v;
    actor->nodes[11].rotation_z = v;
    g_model_draw.target_frame = g_cur_render_frame;
    g_model_draw.depth_bucket = 0x480 - ((sint32)((uint32)g_map_render.depth_anchor_z - actor->col.z)) / 0x100;
    g_model_draw.world_z = actor->col.z;
    if (actor->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --actor->col.flash_clut_ticks;
    }
    model_render_node(actor->nodes, &g_model_identity);
    g_model_draw.clut_override = 0;
    if (!actor->attack_variant || actor->attack_side)
    {
        if ((g_render_state.frame & 1) == 0)
        {
            if (actor->attack_countdown < 150)
            {
                PROJECTILE *projectile;
                speech_random_request(6);
                if (actor->weapon_sound_handle == -1)
                    actor->weapon_sound_handle = sound_play_positional(0x61, 12, 0xb4, actor->col.x, actor->col.y, actor->col.z);
                if (actor->attack_countdown >= 148)
                    actor->target_velocity_x = g_player_world.x < actor->col.x ? -0x400 : 0x400;
                if (actor->attack_countdown < 100)
                    actor->target_pitch = 0xf80;
                actor->muzzle_side ^= 1;
                node = &actor->nodes[3 + actor->muzzle_side * 2];
                projectile = projectile_create(node->world_x, node->world_y, node->world_z, node[1].world_x, node[1].world_y, node[1].world_z);
                projectile->col.hit.damage = 0x80;
                projectile->col.object_type = 0;
                projectile->motion.step.x = projectile->motion.step.x * 4u;
                projectile->motion.step.y = projectile->motion.step.y * 4u;
                projectile->motion.step.z = projectile->motion.step.z * 4u;
                projectile->col.sends_mask |= 1;
                projectile->col.receives_mask |= 4;
                g_model_draw.depth_bucket = 0x480 - ((sint32)((uint32)g_map_render.depth_anchor_z - node->world_z)) / 0x100;
                render_world_spr(((g_render_state.frame >> 2) & 1) | 0x1e000, node->world_x, node->world_y, node->world_z, 0, 0x2200, 0x2200, 0, g_model_draw.depth_bucket, 0, 0, 0);
            }
        }
    }
    else if (actor->attack_countdown < 120)
    {
        sint32 zarg = ((sint32)((uint32)g_player_world.z - actor->col.z)) / 0x1000;
        if (zarg < 0)
            zarg = -zarg;
        actor->target_pitch = 4 * (fixed_angle_from_vector(((sint32)(actor->col.y + 0x3000u - g_player_world.y)) / 0x1000, zarg) & 0x3ff);
        if (actor->attack_countdown == 30 || actor->attack_countdown == 50 || actor->attack_countdown == 70 || actor->attack_countdown == 90)
        {
            PROJECTILE *projectile;
            sound_play_positional(0x63, 0, 0xdc, actor->col.x, actor->col.y, actor->col.z);
            node = &actor->nodes[3 + (actor->muzzle_side ^ 1) * 2];
            actor->muzzle_side ^= 1;
            projectile = projectile_create(node->world_x, node->world_y, node->world_z, node[1].world_x, node[1].world_y, node[1].world_z);
            projectile->col.hit.damage = 0x80;
            projectile->floor_callback = explosive_projectile_trail_update;
            projectile->impact_callback = explosive_projectile_impact;
            projectile->col.object_type = 0;
            projectile->col.sends_mask |= 1;
            projectile->motion.step.x = projectile->motion.step.x * 2u;
            projectile->col.receives_mask |= 4;
            projectile->motion.step.z = projectile->motion.step.z * 2u;
            projectile->motion.step.y = projectile->motion.step.y * 2u;
            speech_random_request(10);
        }
    }
    hierarchy_col_box_update(&actor->col, &actor->nodes[7], 4);
    hud_bar_render(&g_boss_health_bar, actor->col.hit.health, 0, 0);
}

/* Original: GYRO:GYRO_800FBB80. */
static void gyro_boss_damage(void *raw_actor, void *raw_src)
{
    GYRO_ACTOR *actor = raw_actor;
    COLLISION *src = raw_src;
    if ((src->receives_mask & 2) == 0)
    {
        actor->col.flash_clut_ticks = 1;
        if (object_damage_apply(actor, src))
        {
            actor->destroyed_ticks = 1;
            actor->col.receives_mask = 0;
            actor->attack_countdown = 15000;
            sound_voice_stop(&actor->weapon_sound_handle);
            sound_voice_stop(&actor->flight_sound_handle);
            actor->flight_sound_handle = sound_play_positional(0x60, -12, 0x6e, actor->col.x, actor->col.y, actor->col.z);
            speech_random_request(4);
        }
    }
}

/* Original: GYRO:GYRO_800FADD4. */
void gyro_boss_create(EFFECT *effect)
{
    GYRO_ACTOR *actor = runtime_heap_allocate(sizeof(*actor));
    uint32 z;
    actor->col.object.update = gyro_boss_update;
    linked_list_append(&g_object_list, actor);
    z = g_player_world.z - 0x4000u;
    effect->type = 0;
    actor->owner_effect = effect;
    actor->col.z = z;
    actor->col.x = (uint32)g_camera.world_x + ((sint32) - (250u * (z - g_camera.world_z)) / 320);
    actor->col.y = actor->target_y = effect->y - 0x4000u;
    actor->col.receives_mask = 1;
    actor->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    actor->col.hit_callback = gyro_boss_damage;
    actor->col.hit.health = 0x4b0;
    actor->nodes[0].scale = 0x1100;
    actor->nodes[0].rotation_y = 0x400;
    model_build_from_config(actor->nodes, gyro_nodes, g_gyro_model_base);
    actor->attack_countdown = 0x258;
    actor->nodes[0].rotation_x = 0xf80;
    actor->flight_sound_handle = -1;
    actor->weapon_sound_handle = -1;
    actor->patrol_max_x = effect->x + 0x10a00u;
    actor->patrol_min_x = effect->x - 0x10a00u;
    actor->turn_step = 0x10;
    actor->startup_delay = 500;
    actor->target_refresh = 200;
    hud_bar_initialize(&g_boss_health_bar, -40, -96, 80, 0x4b0);
}
