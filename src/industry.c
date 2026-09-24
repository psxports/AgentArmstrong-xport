#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "game_sound.h"
#include "code_module.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "industry.h"
#include "mechanoid.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"

/* Types. */
/* IND/INDUSTRY.BIN: stage-27 type-0x7F closure, exact PAL
 * 0x800FB5C4..0x800FB830. */

typedef struct
{
    COLLISION collision;      /* +0x000 */
    sint16 health;            /* +0x078 */
    sint16 flash_clut_ticks;  /* +0x07A */
    MODEL_NODE node;          /* +0x07C */
    sint32 floor_y;           /* +0x0DC */
    sint32 velocity_y;        /* +0x0E0 */
    uint16 bounce_ticks;      /* +0x0E4 */
    uint16 destruction_armed; /* +0x0E6 */
    uint16 particle_ticks;    /* +0x0E8 */
    uint8 field_0ea[2];       /* +0x0EA */
} BIO_GOO_ACTOR;

typedef struct
{
    COLLISION collision;     /* +0x000 */
    sint16 health;           /* +0x078 */
    sint16 flash_clut_ticks; /* +0x07A */
    MODEL_NODE nodes[2];     /* +0x07C */
    uint8 field_13c[8];      /* +0x13C */
    uint16 activation_ticks; /* +0x144 */
    uint8 field_146[4];      /* +0x146 */
    uint16 target_rotation;  /* +0x14A */
    sint16 active;           /* +0x14C */
    uint8 field_14e[2];      /* +0x14E */
} ROBOT_PRESS_ACTOR;

typedef struct
{
    uint8 field_000[0x7C];        /* +0x000 */
    EFFECT *source_effect;        /* +0x07C */
    uint16 objective_decremented; /* +0x080 */
    uint8 field_082[2];           /* +0x082 */
} INDUSTRY_GUN_CONTROLLER;

#if defined(AP_32BIT)
typedef char BioGooActor_size_ec[sizeof(BIO_GOO_ACTOR) == 0xEC ? 1 : -1];
typedef char RobotPressActor_size_150[sizeof(ROBOT_PRESS_ACTOR) == 0x150 ? 1 : -1];
typedef char IndustryGunController_size_84[sizeof(INDUSTRY_GUN_CONTROLLER) == 0x84 ? 1 : -1];
typedef char RobotPressActor_nodes_at_7c[offsetof(ROBOT_PRESS_ACTOR, nodes) == 0x7C ? 1 : -1];
#endif

/* Variables. */
static SPRITE *trace_spawn;

/* Functions. */
/* Original: INDUSTRY_800FB1F4. */
static void bio_goo_damage(COLLISION *collision, COLLISION *hit)
{
    BIO_GOO_ACTOR *o = (BIO_GOO_ACTOR *)collision;
    if (o->collision.y != o->floor_y - 0x12c00 && object_damage_apply(o, hit))
    {
        o->destruction_armed = 1;
        o->velocity_y = 0x700;
        o->bounce_ticks = 0;
        o->collision.receives_mask = 0;
    }
}

/* Original: INDUSTRY_800FAE9C. */
static void bio_goo_update(BIO_GOO_ACTOR *o)
{
    MISSION_ENEMY_ACTOR *enemy = 0;
    void *next = 0;
    sint32 depth;
    while ((next = object_find_next_by_type(next, 0x7e)) != 0)
    {
        enemy = (MISSION_ENEMY_ACTOR *)next;
        if (o->collision.x - 0x4000 < enemy->collision.x && enemy->collision.x < o->collision.x + 0x4000 && o->collision.z - 0x2000 < enemy->collision.z)
        {
            if (o->velocity_y != 0x600)
                sound_play_positional(0x47, 0, 0x96, o->collision.x, o->collision.y, o->collision.z);
            o->velocity_y = 0x600;
            break;
        }
    }
    if (o->bounce_ticks)
        --o->bounce_ticks;
    else
    {
        o->collision.y += o->velocity_y;
        if (o->collision.y < o->floor_y - 0x12c00)
            o->collision.y = o->floor_y - 0x12c00;
    }
    if (o->bounce_ticks == 1)
        o->particle_ticks = 60;
    if (o->particle_ticks)
    {
        --o->particle_ticks;
        continuous_weapon_particle_create(o->collision.x, o->floor_y + 0x8000, o->collision.z, g_signal_flare_sprite_clut);
    }
    if (o->floor_y < o->collision.y)
    {
        if (!o->bounce_ticks)
            sound_play_positional(0x46, 0, 0x96, o->collision.x, o->collision.y, o->collision.z);
        if (o->destruction_armed)
        {
            object_destroy(o);
            large_object_destruction_effect_create(&o->collision);
            --g_objective_counts[15];
            return;
        }
        o->velocity_y = -0x200;
        o->bounce_ticks = 60;
        o->collision.y = o->floor_y;
        next = 0;
        while ((next = object_find_next_by_type(next, 0x7e)) != 0)
        {
            MISSION_ENEMY_ACTOR *enemy = (MISSION_ENEMY_ACTOR *)next;
            VRAM_SPRITE *descriptor = (VRAM_SPRITE *)enemy->collision.frame_descriptor;
            if (o->collision.x - 0x4000 < enemy->collision.x && enemy->collision.x < o->collision.x + 0x4000 && o->collision.z - 0x2000 < enemy->collision.z)
            {
                airfield_enemy_configure(enemy);
                if (descriptor->region_token)
                    sprite_vram_defer_release(descriptor);
            }
        }
    }
    o->node.translation_x = o->collision.x - g_camera_world_x;
    o->node.translation_y = o->collision.y - g_camera_world_y;
    o->node.translation_z = o->collision.z - g_camera_world_z;
    g_model_render_frame = g_current_render_frame;
    depth = g_render_row_world_z - o->collision.z;
    g_render_depth_bucket = 0x480 - depth / 0x100;
    g_current_model_world_z = o->collision.z;
    if (o->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --o->flash_clut_ticks;
    }
    model_render_node(&o->node, (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
}

/* Original: INDUSTRY_800FADD4. */
void bio_goo_create(EFFECT *e)
{
    BIO_GOO_ACTOR *o = (BIO_GOO_ACTOR *)object_create(sizeof(*o), (FUNC_COLLISION_UPDATE)bio_goo_update);
    o->node.rotation_y = 0x800;
    o->node.model_id = g_bio_goo_model_id;
    o->collision.x = e->x;
    o->collision.y = e->y - 0x10000;
    o->collision.z = e->z;
    o->floor_y = e->y + 0x4000;
    e->type = 0;
    o->collision.receives_mask = 1;
    o->collision.callback_18 = (FUNC_COLLISION_CALLBACK)bio_goo_damage;
    collision_box_set(o, 0xc0, 0x80, 0xc0);
    o->health = 100;
    o->collision.box_y /= 2;
#ifdef XPORT_NATIVE
    if (g_stage_index == 31 && getenv("OA_STAGE31_TRACE"))
    {
        uint8 before = g_objective_counts[15];
        FILE *f = fopen("../status/bio-goo-native.log", "w");
        if (f)
        {
            fprintf(f, "BIO_GOO_NATIVE_CTOR xyz=%d,%d,%d health=%d model=%d floor=%d speed=%d flags=%d\n", o->collision.x, o->collision.y, o->collision.z, o->health, o->node.model_id, o->floor_y, o->velocity_y, o->collision.receives_mask);
            fclose(f);
        }
        o->collision.y = o->floor_y - 0x100;
        o->velocity_y = 0x600;
        o->bounce_ticks = 0;
        bio_goo_update(o);
        f = fopen("../status/bio-goo-native.log", "a");
        if (f)
        {
            fprintf(f, "BIO_GOO_NATIVE_BOUNCE y=%d speed=%d timer=%u\n", o->collision.y, o->velocity_y, (unsigned)o->bounce_ticks);
            fclose(f);
        }
        bio_goo_damage(&o->collision, &o->collision);
        f = fopen("../status/bio-goo-native.log", "a");
        if (f)
        {
            fprintf(f, "BIO_GOO_NATIVE_DAMAGE armed=%u speed=%d timer=%u flags=%d\n", (unsigned)o->destruction_armed, o->velocity_y, (unsigned)o->bounce_ticks, o->collision.receives_mask);
            fclose(f);
        }
        o->collision.y = o->floor_y + 1;
        bio_goo_update(o);
        f = fopen("../status/bio-goo-native.log", "a");
        if (f)
        {
            fprintf(f, "BIO_GOO_NATIVE_DEATH counter15_before=%u counter15_after=%u\n", (unsigned)before, (unsigned)g_objective_counts[15]);
            fclose(f);
        }
    }
#endif
}

/* Original: INDUSTRY_800FB520. */
static void robot_press_damage(COLLISION *collision, COLLISION *hit)
{
    ROBOT_PRESS_ACTOR *o = (ROBOT_PRESS_ACTOR *)collision;
    EFFECT *e = 0;
    if (!object_damage_apply(o, hit))
        return;
    while ((e = effect_find_next(0x27, e)) != 0)
    {
        if (o->collision.x - 0x4000 < e->x && e->x < o->collision.x + 0x4000)
        {
            e->type = 0;
            break;
        }
    }
    large_object_destruction_effect_create(&o->collision);
    object_destroy(o);
    --g_objective_counts[17];
}

/* Original: INDUSTRY_800FB32C. */
static void robot_press_update(ROBOT_PRESS_ACTOR *o)
{
    COLLISION *nearby = 0;
    sint32 depth;
    while ((nearby = (COLLISION *)object_find_next_by_type(nearby, 0x92)) != 0)
    {
        if (o->collision.x - 0x4000 < nearby->x && nearby->x < o->collision.x + 0x4000 && o->collision.z - 0x2000 < nearby->z)
            break;
    }
    if (nearby)
    {
        o->target_rotation = 0x400;
        if (!o->active)
            sound_play_positional(0x48, 0, 0x96, o->collision.x, o->collision.y, o->collision.z);
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
    o->nodes[0].translation_x = o->collision.x - g_camera_world_x;
    o->nodes[0].translation_y = o->collision.y - g_camera_world_y;
    o->nodes[0].translation_z = o->collision.z - g_camera_world_z;
    g_model_render_frame = g_current_render_frame;
    depth = g_render_row_world_z - o->nodes[1].world_z;
    g_render_depth_bucket = 0x470 - depth / 0x100;
    g_current_model_world_z = o->collision.z;
    if (o->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --o->flash_clut_ticks;
    }
    model_render_node(&o->nodes[0], (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
}

/* Original: INDUSTRY_800FB25C. */
void robot_press_create(EFFECT *e)
{
    ROBOT_PRESS_ACTOR *o = (ROBOT_PRESS_ACTOR *)object_create(sizeof(*o), (FUNC_COLLISION_UPDATE)robot_press_update);
    model_attach_child(&o->nodes[1], &o->nodes[0]);
    o->nodes[1].translation_x = 0x2000;
    o->nodes[1].model_id = g_robot_press_model_id;
    o->collision.x = e->x - 0x2000;
    o->collision.y = e->y + 0x8000;
    o->collision.z = e->z - 0x2000;
    e->type = 0;
    o->collision.receives_mask = 1;
    o->collision.callback_18 = (FUNC_COLLISION_CALLBACK)robot_press_damage;
    collision_box_set(o, 0x40, 0x80, 0x40);
    o->health = 150;
    o->collision.box_y /= 2;
#ifdef XPORT_NATIVE
    if (g_stage_index == 32 && getenv("OA_STAGE32_TRACE"))
    {
        COLLISION *near = (COLLISION *)object_find_next_by_type(0, 0x92);
        EFFECT *link = 0;
        sint32 nx = 0, nz = 0, lx = 0;
        uint8 before = g_objective_counts[17];
        FILE *f = fopen("../status/robot-press-native.log", "w");
        if (f)
        {
            fprintf(f, "ROBOT_PRESS_NATIVE_CTOR xyz=%d,%d,%d health=%d model=%d child_offset=%d flags=%d\n", o->collision.x, o->collision.y, o->collision.z, o->health, o->nodes[1].model_id, o->nodes[1].translation_x, o->collision.receives_mask);
            fclose(f);
        }
        if (near)
        {
            nx = near->x;
            nz = near->z;
            near->x = o->collision.x;
            near->z = o->collision.z;
        }
        robot_press_update(o);
        f = fopen("../status/robot-press-native.log", "a");
        if (f)
        {
            fprintf(f, "ROBOT_PRESS_NATIVE_UPDATE angle=%d timer=%u active=%d target=%u\n", o->nodes[0].rotation_y, (unsigned)o->activation_ticks, o->active, (unsigned)o->target_rotation);
            fclose(f);
        }
        if (near)
        {
            near->x = nx;
            near->z = nz;
            object_destroy(near);
        }
        link = effect_find_next(0x27, 0);
        if (link)
        {
            lx = link->x;
            link->x = o->collision.x;
        }
        robot_press_damage(&o->collision, &o->collision);
        f = fopen("../status/robot-press-native.log", "a");
        if (f)
        {
            fprintf(f, "ROBOT_PRESS_NATIVE_DEATH link_cleared=%d counter17_before=%u counter17_after=%u\n", link ? link->type : -1, (unsigned)before, (unsigned)g_objective_counts[17]);
            fclose(f);
        }
        if (link)
            link->x = lx;
    }
#endif
}

/* Original: INDUSTRY_800FB600. */
static void industry_gun_controller_update(INDUSTRY_GUN_CONTROLLER *controller)
{
    EFFECT *source = controller->source_effect;
    sint32 depth, width, y;
    SPRITE *shot;
    if (!g_trigger_states[(uint16)source->values[3]])
    {
        if (!controller->objective_decremented)
        {
            --g_objective_counts[16];
            controller->objective_decremented = 1;
        }
        if (!(g_frame_counter & 0x40))
            return;
    }
    depth = source->z - g_camera_world_z;
    width = div_trunc((g_screen_half_width + 0x60) * depth, 320);
    if (source->x < g_camera_world_x - width || g_camera_world_x + width < source->x)
    {
        source->values[0] = 0;
        return;
    }
    if ((sint16)source->values[0])
    {
        --source->values[0];
        return;
    }
    y = source->y - 0x2800;
    if (y < div_trunc(-0x80 * depth, 355) + g_camera_world_y)
        y = div_trunc(-0x80 * depth, 355) + g_camera_world_y;
    source->values[0] = g_trigger_states[(uint16)source->values[3]] ? 7 : (uint16)(random_range(8) + 8);
    shot = world_sprite_create(source->x, y, source->z, (const sint32 *)win_code_module_address(0x800fb830u));
    shot->velocity_y = 0xb00;
    shot->scale_x = 0x1400;
    shot->scale_y = 0x2400;
    collision_box_set(shot, 0x46, 0x46, 0x48);
    shot->collision.receives_mask = 2;
    shot->collision.callback_14 = (FUNC_COLLISION_CALLBACK)player_apply_damage;
    shot->damage = 0x400;
    trace_spawn = shot;
}

/* Original: INDUSTRY_800FB5C4. */
void industry_gun_controller_create(EFFECT *effect)
{
    INDUSTRY_GUN_CONTROLLER *controller = (INDUSTRY_GUN_CONTROLLER *)object_create(sizeof(*controller), (FUNC_COLLISION_UPDATE)industry_gun_controller_update);
    controller->source_effect = effect;
    effect->type = 0;
#ifdef XPORT_NATIVE
    if (g_stage_index == 27 && getenv("OA_STAGE27_TRACE"))
    {
        sint32 ox = g_camera_world_x, oy = g_camera_world_y, oz = g_camera_world_z, ow = g_screen_half_width;
        uint8 gate = (uint8)effect->values[3];
        FILE *f;
        g_trigger_states[gate] = 1;
        g_camera_world_x = effect->x;
        g_camera_world_y = effect->y;
        g_camera_world_z = effect->z - 0x10000;
        g_screen_half_width = 0xa0;
        trace_spawn = 0;
        industry_gun_controller_update(controller);
        f = fopen("../status/the-pits-native.log", "w");
        if (f && trace_spawn)
        {
            fprintf(f, "THE_PITS_NATIVE source=%d,%d,%d gate=%u cooldown=%d shot=%d,%d,%d vy=%d scale=%d,%d health=%d flags=%d\n", effect->x, effect->y, effect->z, (unsigned)gate, (sint16)effect->values[0], trace_spawn->collision.x, trace_spawn->collision.y, trace_spawn->collision.z, trace_spawn->velocity_y, trace_spawn->scale_x, trace_spawn->scale_y, trace_spawn->damage, trace_spawn->collision.receives_mask);
            fclose(f);
        }
        g_camera_world_x = ox;
        g_camera_world_y = oy;
        g_camera_world_z = oz;
        g_screen_half_width = ow;
        object_destroy(controller);
    }
#endif
}
