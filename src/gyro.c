#include <stddef.h>
#include "airship.h"
#include "game_sound.h"
#include "collision.h"
#include "global.h"
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

/* Types. */
typedef struct
{
    COLLISION collision;        /* +0x000 */
    sint16 health;              /* +0x078 */
    sint16 flash_clut_ticks;    /* +0x07A */
    sint32 target_x;            /* +0x07C */
    sint32 velocity_x;          /* +0x080 */
    sint32 target_velocity_x;   /* +0x084 */
    sint32 field_088;           /* +0x088 */
    sint32 target_y;            /* +0x08C */
    sint32 velocity_y;          /* +0x090 */
    sint32 field_094;           /* +0x094 */
    sint32 target_z;            /* +0x098 */
    sint32 velocity_z;          /* +0x09C */
    uint8 field_0a0[0x0c];      /* +0x0A0 */
    sint16 attack_countdown;    /* +0x0AC */
    sint16 flight_sound_handle; /* +0x0AE */
    sint16 weapon_sound_handle; /* +0x0B0 */
    sint16 muzzle_side;         /* +0x0B2 */
    sint16 attack_side;         /* +0x0B4 */
    sint16 attack_variant;      /* +0x0B6 */
    sint16 turn_step;           /* +0x0B8 */
    sint16 destroyed_ticks;     /* +0x0BA */
    sint16 startup_delay;       /* +0x0BC */
    sint16 target_refresh;      /* +0x0BE */
    MODEL_NODE nodes[12];       /* +0x0C0 */
    uint8 field_540[0x120];     /* +0x540 */
    sint32 turn_delta;          /* +0x660 */
    EFFECT *owner_effect;       /* +0x664 */
    sint32 patrol_min_x;        /* +0x668 */
    sint32 patrol_max_x;        /* +0x66C */
    uint8 field_670[4];         /* +0x670 */
} GYRO_ACTOR;

#if defined(AP_32BIT)
    #define GYRO_OFFSET_ASSERT(field, offset) typedef char GyroActor_##field##_at_##offset[(offsetof(GYRO_ACTOR, field) == 0x##offset) ? 1 : -1]
GYRO_OFFSET_ASSERT(target_x, 07c);
GYRO_OFFSET_ASSERT(attack_countdown, 0ac);
GYRO_OFFSET_ASSERT(nodes, 0c0);
GYRO_OFFSET_ASSERT(turn_delta, 660);
GYRO_OFFSET_ASSERT(owner_effect, 664);
GYRO_OFFSET_ASSERT(patrol_min_x, 668);
typedef char GyroActor_size_674[(sizeof(GYRO_ACTOR) == 0x674) ? 1 : -1];
    #undef GYRO_OFFSET_ASSERT
#endif

/* Variables. */
static const sint16 gyro_nodes[] = {2, 0, 0, 0, 0, 1, 0, -73, 32, 1, 0, -12, -12, -128, 1, -2, -46, 48, 62, 1, -2, -42, 48, 126, 1, -2, 46, 48, 62, 1, -2, 42, 48, 126, 1, -2, -72, -72, 140, 1, -2, -72, -72, -140, 1, -2, 72, 72, 140, 1, -2, 72, 72, -140, 1, 0, 0, 21, 152, 1, -1};

/* Functions. */
/* Resident PAL 0x8009541C..0x80095514, first referenced by GYRO.BIN. */
/* Original: FUN_8009541C. */
void model_build_from_config(MODEL_NODE *nodes, const sint16 *config, sint32 model_base)
{
    MODEL_NODE *node = nodes;
    while (*config != -1)
    {
        sint16 model = *config++, parent;
        if (model != -2)
            node->model_id = model_base + model;
        node->translation_x = *config++ << 8;
        node->translation_y = *config++ << 8;
        node->translation_z = *config++ << 8;
        parent = *config++;
        if (parent)
            model_attach_child(node, &nodes[parent - 1]);
        ++node;
    }
}

static void gyro_projectile(MODEL_NODE *node, sint32 scale, sint32 callbacks)
{
    PROJECTILE *p = projectile_create(node->world_x, node->world_y, node->world_z, node[1].world_x, node[1].world_y, node[1].world_z);
    p->damage = 0x80;
    p->collision.object_type = 0;
    p->collision.sends_mask |= 1;
    p->collision.receives_mask |= 4;
    p->motion[3] *= scale;
    p->motion[4] *= scale;
    p->motion[5] *= scale;
    if (callbacks)
    {
        p->floor_callback = (FUNC_COLLISION_UPDATE)explosive_projectile_trail_update;
        p->impact_callback = (FUNC_COLLISION_UPDATE)explosive_projectile_impact;
    }
}

/* Original: GYRO_800FAF58. */
void gyro_boss_update(GYRO_ACTOR *o)
{
    EFFECT *effect = o->owner_effect;
    MODEL_NODE *node;
    sint32 v, target, delta, step, angle;
    void *sound;
    if (o->startup_delay)
    {
        --o->startup_delay;
        if (o->startup_delay == 0)
            speech_random_request(6);
        return;
    }
    if ((355 * (o->collision.y - g_camera_world_y)) / (o->collision.z - g_camera_world_z) < -400)
    {
        sound_voice_stop(&o->flight_sound_handle);
        sound_voice_stop(&o->weapon_sound_handle);
        object_destroy(o);
        g_camera_depth_offset_target = 0;
        effect->type = 50;
        return;
    }
    if (o->flight_sound_handle == -1)
        o->flight_sound_handle = sound_play_positional(0x60, 0, 0x96, o->collision.x, o->collision.y, o->collision.z);
    else
    {
        sound = sound_spatial_volume_calculate(0x96, o->collision.x, o->collision.y, o->collision.z);
        if (sound)
            sound_voice_spatial_volume_update(sound, o->flight_sound_handle);
    }
    if (o->attack_countdown && --o->attack_countdown != 0)
    {
        if (o->attack_countdown == 290)
            sound_play_positional(0x62, 0, 0xb4, o->collision.x, o->collision.y, o->collision.z);
        if (o->attack_countdown >= 260)
        {
            o->field_088 = 0xf80;
            if (o->patrol_max_x < o->collision.x)
                o->target_x = effect->x - 0x10000;
            if (o->collision.x < o->patrol_min_x)
                o->target_x = effect->x + 0x10000;
            o->target_velocity_x = o->collision.x < o->target_x ? 0x800 : -0x800;
        }
        else if (o->attack_countdown == 259)
        {
            o->field_088 = 0xec0;
            o->target_velocity_x = 0;
            o->turn_step = 10;
        }
    }
    else
    {
        o->attack_countdown = 600;
        sound_voice_stop(&o->weapon_sound_handle);
        o->attack_side ^= 1;
        o->turn_step = 16;
        if (!o->attack_side)
            o->attack_variant ^= 1;
    }
    if (o->velocity_x < o->target_velocity_x)
        o->velocity_x += 0x40;
    if (o->target_velocity_x < o->velocity_x)
        o->velocity_x -= 0x40;
    o->collision.x += o->velocity_x;
    if (o->target_refresh)
        --o->target_refresh;
    else
    {
        target = effect->y + ((random_range(0x80) - 0x40) << 8);
        target -= o->attack_side ? 0x4800 : 0x3000;
        if (o->attack_countdown >= 501)
            target -= 0x4000;
        o->target_y = target;
        o->target_refresh = 16;
    }
    if (o->destroyed_ticks)
    {
        ++o->destroyed_ticks;
        if ((g_frame_counter & 0x1f) == 0)
            sound_play_positional(9, 0, 0x93, o->collision.x, o->collision.y, o->collision.z);
        if (o->destroyed_ticks >= 400)
        {
            o->target_y = 0;
            o->field_088 = -0x200;
        }
        else
            o->target_y = effect->y - 0x1000;
        if (o->destroyed_ticks >= 701)
        {
            sound_voice_stop(&o->flight_sound_handle);
            object_destroy(o);
            --g_objective_counts[7];
            return;
        }
    }
    if (o->target_y < o->collision.y)
        o->velocity_y -= 0x20;
    if (o->collision.y < o->target_y)
        o->velocity_y += 0x20;
    if (o->velocity_y > 0x300)
        o->velocity_y = 0x300;
    if (o->velocity_y < -0x300)
        o->velocity_y = -0x300;
    o->collision.y += o->velocity_y;
    if (o->attack_side)
    {
        g_camera_depth_offset_target = 0x80;
        o->target_z = effect->z - 0x14a00;
    }
    else
    {
        o->target_z = effect->z + 0x8000;
        if (o->attack_variant)
            o->target_z += 0xc000;
        g_camera_depth_offset_target = 0;
    }
    if (o->destroyed_ticks)
    {
        g_camera_depth_offset_target = 0;
        o->target_z = effect->z + 0x8000;
    }
    if (o->target_z < o->collision.z)
        o->velocity_z -= 0x20;
    if (o->collision.z < o->target_z)
        o->velocity_z += 0x20;
    if (o->velocity_z > 0x200)
        o->velocity_z = 0x200;
    if (o->velocity_z < -0x200)
        o->velocity_z = -0x200;
    o->collision.z += o->velocity_z;
    if (o->destroyed_ticks)
    {
        if ((g_frame_counter & 7) == 0)
        {
            timed_callback_create(expl_debris_particle_create, o->collision.x, o->collision.y, o->collision.z, 8, 2);
            ++o->turn_delta;
        }
    }
    else
    {
        angle = angle_to_player(o->collision.x, o->collision.z, 1) * 4;
        delta = angle - o->nodes[0].rotation_y;
        if (delta < 0)
            delta = -delta;
        o->turn_delta = 0;
        step = delta;
        if (step > o->turn_step)
            step = o->turn_step;
        if (o->nodes[0].rotation_y != angle)
            o->turn_delta = angle > o->nodes[0].rotation_y ? step : -step;
        if (delta < 0x801)
            o->nodes[0].rotation_y = (sint16)((o->nodes[0].rotation_y + o->turn_delta) & 0xfff);
        else
            o->turn_delta = -o->turn_delta;
    }
    o->nodes[0].rotation_x = (sint16)((o->nodes[0].rotation_x + (o->velocity_x / -5)) & 0xfff);
    o->nodes[0].rotation_x &= 0xfff;
    o->nodes[0].world_x = o->collision.x - g_camera_world_x;
    o->nodes[0].world_y = o->collision.y - g_camera_world_y;
    o->nodes[0].world_z = o->collision.z - g_camera_world_z;
    o->nodes[11].rotation_y = 0x600;
    v = (g_frame_counter & 0xf) << 8;
    o->nodes[1].rotation_y = o->nodes[2].rotation_x = o->nodes[11].rotation_z = (sint16)v;
    g_model_render_frame = g_current_render_frame;
    g_render_depth_bucket = 0x480 - (g_render_row_world_z - o->collision.z) / 0x100;
    g_current_model_world_z = o->collision.z;
    if (o->flash_clut_ticks)
    {
        --o->flash_clut_ticks;
        g_model_clut_override = g_hit_flash_clut;
    }
    model_render_node(o->nodes, (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
    if (!o->attack_variant || o->attack_side)
    {
        if ((g_frame_counter & 1) == 0)
        {
            node = &o->nodes[3 + (o->muzzle_side ^ 1) * 2];
            o->muzzle_side ^= 1;
            if (o->attack_countdown < 150)
            {
                speech_random_request(6);
                if (o->weapon_sound_handle == -1)
                    o->weapon_sound_handle = sound_play_positional(0x61, 12, 0xb4, o->collision.x, o->collision.y, o->collision.z);
                if ((uint16)(o->attack_countdown - 148) < 2)
                    o->target_velocity_x = g_player_world_x < o->collision.x ? -0x400 : 0x400;
                if (o->attack_countdown < 100)
                    o->field_088 = 0xf80;
                gyro_projectile(node, 4, 0);
                g_render_depth_bucket = 0x480 - (g_render_row_world_z - node->world_z) / 0x100;
                render_world_sprite(((g_frame_counter >> 2) & 1) | 0x1e000, node->world_x, node->world_y, node->world_z, 0, 0x2200, 0x2200, 0, g_render_depth_bucket, 0, 0, 0);
            }
        }
    }
    else if (o->attack_countdown < 120)
    {
        sint32 zarg = (g_player_world_z - o->collision.z) / 0x1000;
        if (zarg < 0)
            zarg = -zarg;
        o->field_088 = 4 * (fixed_angle_from_vector((o->collision.y + 0x3000 - g_player_world_y) / 0x1000, zarg) & 0x3ff);
        if (o->attack_countdown == 30 || o->attack_countdown == 50 || o->attack_countdown == 70 || o->attack_countdown == 90)
        {
            sound_play_positional(0x63, 0, 0xdc, o->collision.x, o->collision.y, o->collision.z);
            node = &o->nodes[3 + (o->muzzle_side ^ 1) * 2];
            o->muzzle_side ^= 1;
            gyro_projectile(node, 2, 1);
            speech_random_request(10);
        }
    }
    hierarchy_collision_box_update(o, &o->nodes[7], 4);
    hud_bar_render(g_boss_health_bar, o->health, 0, 0);
}

/* Original: GYRO_800FBB80. */
void gyro_boss_damage(GYRO_ACTOR *o, COLLISION *source)
{
    if ((source->receives_mask & 2) == 0)
    {
        o->flash_clut_ticks = 1;
        if (object_damage_apply(o, source))
        {
            o->destroyed_ticks = 1;
            o->collision.receives_mask = 0;
            o->attack_countdown = 15000;
            sound_voice_stop(&o->weapon_sound_handle);
            sound_voice_stop(&o->flight_sound_handle);
            o->flight_sound_handle = sound_play_positional(0x60, -12, 0x6e, o->collision.x, o->collision.y, o->collision.z);
            speech_random_request(4);
        }
    }
}

/* Original: GYRO_800FADD4. */
void gyro_boss_create(EFFECT *effect)
{
    GYRO_ACTOR *o = (GYRO_ACTOR *)runtime_heap_allocate(0x674);
    sint32 z = g_player_world_z - 0x4000;
    o->collision.update = (FUNC_COLLISION_UPDATE)gyro_boss_update;
    linked_list_append(g_object_list, o);
    effect->type = 0;
    o->owner_effect = effect;
    o->collision.z = z;
    o->collision.x = g_camera_world_x + (-250 * (z - g_camera_world_z)) / 320;
    o->collision.y = o->target_y = effect->y - 0x4000;
    o->collision.receives_mask = 1;
    o->collision.callback_18 = (FUNC_COLLISION_CALLBACK)gyro_boss_damage;
    o->health = 0x4b0;
    o->nodes[0].scale = 0x1100;
    o->nodes[0].rotation_y = 0x400;
    model_build_from_config(o->nodes, gyro_nodes, g_gyro_model_base);
    o->attack_countdown = 0x258;
    o->nodes[0].rotation_x = 0xf80;
    o->flight_sound_handle = -1;
    o->weapon_sound_handle = -1;
    o->patrol_max_x = effect->x + 0x10a00;
    o->patrol_min_x = effect->x - 0x10a00;
    o->turn_step = 0x10;
    o->startup_delay = 500;
    o->target_refresh = 200;
    hud_bar_initialize(g_boss_health_bar, -40, -96, 80, 0x4b0);
}
