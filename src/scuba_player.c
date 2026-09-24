#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "airship.h"
#include "animation.h"
#include "game_sound.h"
#include "camera.h"
#include "code_module.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "hq.h"
#include "map.h"
#include "mechanoid.h"
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
#include "vram_alloc.h"

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
    uint8 field_00[0x10];
    sint16 count;     /* +0x10 */
    sint16 index;     /* +0x12 */
    sint16 direction; /* +0x14 */
    uint16 field_16;
    EFFECT *points[13]; /* +0x18 */
    SCUBA_VECTOR step;  /* +0x4C */
    uint32 field_58;
} SCUBA_ROUTE;

typedef struct
{
    COLLISION collision;
    sint16 health;           /* +0x78 */
    sint16 flash_clut_ticks; /* +0x7A */
    EFFECT *owner_effect;    /* +0x7C */
} SCUBA_SHIP_TARGET;

typedef struct
{
    COLLISION collision;
    sint16 health;           /* +0x078 */
    sint16 flash_clut_ticks; /* +0x07A */
    MODEL_NODE nodes[5];     /* +0x07C */
    uint8 field_25c[0x120];
    MODEL_NODE *node_table[5]; /* +0x37C */
    uint8 field_390[0x0C];
    sint16 oscillation_step; /* +0x39C */
    sint16 oscillation;      /* +0x39E */
    uint16 sink_ticks;       /* +0x3A0 */
    uint16 field_3a2;
    EFFECT *owner_effect; /* +0x3A4 */
} SCUBA_TUG_TARGET;

typedef struct
{
    COLLISION collision;
    sint16 health;           /* +0x078 */
    sint16 flash_clut_ticks; /* +0x07A */
    MODEL_NODE nodes[8];     /* +0x07C */
    uint8 field_37c[0x180];
    MODEL_NODE *node_table[8]; /* +0x4FC */
    uint8 field_51c[0x10];
    SCUBA_ROUTE body_route;    /* +0x52C */
    SCUBA_ROUTE aim_route;     /* +0x588 */
    SCUBA_VECTOR aim_position; /* +0x5E4 */
    uint8 field_5f0[0x1C];
    EFFECT *owner_effect; /* +0x60C */
    uint16 field_610;
    sint16 target_pitch;   /* +0x612 */
    sint16 target_yaw;     /* +0x614 */
    sint16 sound_handle;   /* +0x616 */
    sint16 fire_countdown; /* +0x618 */
    uint16 field_61a;
} SCUBA_SUBMARINE;

typedef struct
{
    COLLISION collision;
    sint16 health;           /* +0x078 */
    sint16 flash_clut_ticks; /* +0x07A */
    ANIM animation;          /* +0x07C */
    uint32 field_098;
    EFFECT *route_points[20];    /* +0x09C */
    VRAM_SPRITE vram_descriptor; /* +0x0EC */
    EFFECT *owner_effect;        /* +0x11C */
    sint32 path[6];              /* +0x120 */
    sint16 route_index;          /* +0x138 */
    sint16 route_count;          /* +0x13A */
    uint8 field_13c[0x0A];
    sint16 fire_countdown; /* +0x146 */
    uint8 field_148[4];
    sint16 route_direction; /* +0x14C */
    sint16 half_speed;      /* +0x14E */
    uint8 field_150[0x34];
    sint32 route_speed; /* +0x184 */
    uint8 field_188[0x0C];
    sint16 direction;                 /* +0x194 */
    uint16 desired_direction;         /* +0x196 */
    uint16 previous_target_direction; /* +0x198 */
    uint16 animation_frame;           /* +0x19A */
    MODEL_NODE nodes[6];              /* +0x19C */
    uint8 field_3dc[0x180];
    MODEL_NODE *node_table[6]; /* +0x55C */
    uint8 field_574[0x10];
    sint32 vertical_animation; /* +0x584 */
} SCUBA_DIVER;

typedef struct
{
    COLLISION collision;
    uint8 field_78[4];
    MODEL_NODE node;        /* +0x7C */
    sint32 velocity_y;      /* +0xDC */
    sint32 rotation_step_y; /* +0xE0 */
    sint32 rotation_step_x; /* +0xE4 */
} SCUBA_DRIFTING_HAZARD;

typedef struct
{
    COLLISION collision;
    sint16 health;           /* +0x78 */
    sint16 flash_clut_ticks; /* +0x7A */
    EFFECT *owner_effect;    /* +0x7C */
    sint16 phase;            /* +0x80 */
    sint16 phase_step;       /* +0x82 */
    sint16 proximity_ticks;  /* +0x84 */
    uint16 field_86;
    sint32 vertical_animation; /* +0x88 */
} SCUBA_MINE;

typedef struct
{
    COLLISION collision;
    sint16 health;           /* +0x78 */
    sint16 flash_clut_ticks; /* +0x7A */
    sint16 model_id;         /* +0x7C */
    sint16 fire_countdown;   /* +0x7E */
} SCUBA_MODEL_TARGET;

struct SCUBA_PLAYER
{
    COLLISION collision;
    sint16 field_078;
    sint16 flash_clut_ticks;
    uint8 field_07c[8];
    uint32 field_084;
    uint8 field_088[0x18];
    uint16 equipment_flags; /* +0x0A0 */
    uint8 field_0a2[6];
    VRAM_SPRITE vram_descriptor; /* +0x0A8 */
    uint8 field_0d8[0x30];
    uint32 previous_buttons; /* +0x108 */
    uint8 field_10c[4];
    sint32 direction_x; /* +0x110 */
    sint32 direction_z; /* +0x114 */
    uint8 field_118[0x0C];
    sint16 action_lock;           /* +0x124 */
    sint16 effect_latch;          /* +0x126 */
    INVENTORY_SLOT inventory[16]; /* +0x128 */
    sint16 direction;             /* +0x1A8 */
    sint16 previous_direction;    /* +0x1AA */
    uint16 field_1ac;
    sint16 item_button_latch; /* +0x1AE */
    uint8 field_1b0[0x14];
    sint32 health;          /* +0x1C4 */
    sint16 damage_cooldown; /* +0x1C8 */
    uint8 field_1ca[0x7E];
    uint8 particle_state[0x54]; /* +0x248 */
    sint16 animation_frame;     /* +0x29C */
    uint16 field_29e;
    SCUBA_VECTOR desired_velocity; /* +0x2A0 */
    SCUBA_VECTOR velocity;         /* +0x2AC */
    MODEL_NODE nodes[6];           /* +0x2B8 */
    uint8 field_4f8[0x180];
    MODEL_NODE *node_table[6]; /* +0x678 */
    uint8 field_690[0x10];
    sint16 animation_countdown; /* +0x6A0 */
    sint16 turn_flash_ticks;    /* +0x6A2 */
    sint16 desired_direction;   /* +0x6A4 */
    sint16 fire_latch;          /* +0x6A6 */
    sint16 hit_turn_ticks;      /* +0x6A8 */
    uint16 air;                 /* +0x6AA */
};

#if defined(AP_32BIT)
typedef char ScubaVector_size_0c[sizeof(SCUBA_VECTOR) == 0x0c ? 1 : -1];
typedef char ScubaRoute_size_5c[sizeof(SCUBA_ROUTE) == 0x5c ? 1 : -1];
typedef char ScubaShipTarget_size_80[sizeof(SCUBA_SHIP_TARGET) == 0x80 ? 1 : -1];
typedef char ScubaTugTarget_size_3a8[sizeof(SCUBA_TUG_TARGET) == 0x3a8 ? 1 : -1];
typedef char ScubaSubmarine_size_61c[sizeof(SCUBA_SUBMARINE) == 0x61c ? 1 : -1];
typedef char ScubaDiver_size_588[sizeof(SCUBA_DIVER) == 0x588 ? 1 : -1];
typedef char ScubaDriftingHazard_size_e8[sizeof(SCUBA_DRIFTING_HAZARD) == 0xe8 ? 1 : -1];
typedef char ScubaMine_size_8c[sizeof(SCUBA_MINE) == 0x8c ? 1 : -1];
typedef char ScubaModelTarget_size_80[sizeof(SCUBA_MODEL_TARGET) == 0x80 ? 1 : -1];
typedef char ScubaPlayer_size_6ac[sizeof(SCUBA_PLAYER) == 0x6ac ? 1 : -1];
typedef char ScubaPlayer_nodes_at_2b8[offsetof(SCUBA_PLAYER, nodes) == 0x2b8 ? 1 : -1];
typedef char ScubaPlayer_node_table_at_678[offsetof(SCUBA_PLAYER, node_table) == 0x678 ? 1 : -1];
#endif

/* Macros. */
#ifdef XPORT_NATIVE
static uint32 scuba_trace_seen;

static void scuba_trace(uint32 bit, const char *tag, void *raw)
{
    FILE *file;
    EFFECT *record = (EFFECT *)raw;
    const char *stage19 = getenv("OA_STAGE19_TRACE");
    if ((scuba_trace_seen & bit) != 0 || (getenv("OA_SUB_BASE_TRACE") == 0 && !stage19))
        return;
    file = fopen(stage19 ? "../status/stage19-native.log" : "../status/sub-base-native.log", scuba_trace_seen ? "a" : "w");
    if (file == 0)
        return;
    fprintf(file, "%s_NATIVE_%s tick=%u xyz=%d,%d,%d\n", stage19 ? "STAGE19" : "SUBBASE", tag, g_frame_counter, record->x, record->y, record->z);
    fclose(file);
    scuba_trace_seen |= bit;
}
#else
    #define scuba_trace(bit, tag, raw) ((void)0)
#endif

/* Variables. */
static sint32 scuba_particle_count;

static MATRIX scuba_identity = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};

static sint32 scuba_type67_spawn_sequence;

static sint32 scuba_route_target[3];

/* Functions. */
static void scuba_player_update(SCUBA_PLAYER *player);

static const sint32 *data32(uint32 address)
{
    return (const sint32 *)win_code_module_address(address);
}

/* 0x800FADD4: bubble/explosion sprite constructor. */
/* Original: SCUBA_FUN_800fadd4. */
/* Original: SCUBA_800FADD4. */
static SPRITE *scuba_blast_sprite_create(sint32 x, sint32 y, sint32 z)
{
    SPRITE *effect = world_sprite_create(x, y, z, data32(0x800ff0a8));
    effect->field_a8 = -0x10;
    effect->field_aa = -2;
    effect->scale_x = (uint16)(random_range(0x1000) + 0x1000);
    effect->field_ae = 1;
    sound_play_positional(0x9c, 0, 0x7f, x, y, z);
    return effect;
}

/* 0x800FDE7C: callback used by the death emitter. */
/* Original: SCUBA_FUN_800fde7c. */
/* Original: SCUBA_800FDE7C. */
static void *scuba_death_particle_emit(TIMED_CALLBACK *emitter)
{
    sint32 x = emitter->collision.x + (random_range(0xc0) - 0x60) * 0x100;
    sint32 y = emitter->collision.y + (random_range(0x80) - 0x40) * 0x100;
    return scuba_blast_sprite_create(x, y, emitter->collision.z - random_range(0x20) * 0x100);
}

/* 0x800FBE08..0x800FBF30: damage/death accounting. */
/* Original: SCUBA_FUN_800fbe08. */
/* Original: SCUBA_800FBE08. */
static void scuba_player_damage_apply(FLASHABLE *hit, SCUBA_PLAYER *player)
{
    if (g_invulnerability_cheat_enabled != 0)
        return;
    if (hit != 0)
    {
        sint16 damage = hit->field_78;
        if (player->damage_cooldown != 0)
            return;
        sound_play_positional(0xa6, 0, 0xc8, player->collision.x, player->collision.y, player->collision.z);
        if (damage == 0)
            player->health -= 0x80;
        else
        {
            if (hit->collision.object_type == 2)
                player->health -= 0xaa;
            player->health -= damage;
        }
        player->damage_cooldown = 0x78;
        player->hit_turn_ticks = 0x32;
    }
    if (player->health <= 0)
    {
        player->health = 0;
        timed_callback_create(scuba_death_particle_emit, player->collision.x, player->collision.y, player->collision.z, 0x28, 2);
        player_death_begin();
    }
}

/* 0x800FD538: hit conversion into the common blast/effect objects. */
/* Original: SCUBA_FUN_800fd538. */
/* Original: SCUBA_800FD538. */
static void scuba_blast_effect_create(COLLISION *source)
{
    SPRITE *blast = expl_flash_create(source->x, source->y, source->z);
    SPRITE *effect;
    blast->animation.start_address = (uint32)(intptr)data32(0x800ff1f4);
    blast->animation.next_address = (uint32)(intptr)data32(0x800ff1f4);
    blast->field_aa = -8;
    blast->rotation_step_x = 8;
    blast->scale_step = 0x200;
    effect = world_sprite_create(source->x, source->y, source->z, data32(0x800ff0a8));
    effect->scale_x = 0x2000;
    effect->collision.callback_14 = scuba_player_damage_apply;
    if (source->object_type != 0x79)
        effect->collision.receives_mask = 2;
    else
        effect->field_9c = 1;
    collision_box_set(effect, 0x80, 0x80, 0xc0);
    effect->collision.box_y /= 2;
    sound_play_positional(0xa1, 4, 0x96, source->x, source->y, source->z);
    camera_shake_start();
    if (source->object_type != 0x67)
    {
        effect->collision.object_type = 2;
        effect->damage = 4;
        effect->collision.receives_mask |= 4;
        effect->collision.sends_mask |= 1;
    }
}

/* Original: SCUBA_FUN_800fd960. */
/* Original: SCUBA_800FD960. */
static void scuba_target_damage(FLASHABLE *object, FLASHABLE *hit)
{
    object->flash_clut_ticks = 1;
    object->field_78 = (sint16)((uint16)object->field_78 - (uint16)hit->field_78);
    if (hit->collision.object_type == 0x64)
        hit->field_78 = 0;
    sound_play_positional(0xa2, 6, 0x7f, object->collision.x, object->collision.y, object->collision.z);
    if (object->field_78 <= 0)
    {
        scuba_blast_effect_create(&object->collision);
        object_destroy(object);
    }
}

/* Exact 0x800FEE18..0x800FEE68: shared damage plus objective counter 10. */
/* Original: SCUBA_FUN_800fee18. */
/* Original: SCUBA_800FEE18. */
static void scuba_ship_target_damage(SCUBA_SHIP_TARGET *object, FLASHABLE *hit)
{
    scuba_target_damage((FLASHABLE *)object, hit);
    if (object->health <= 0)
        --g_objective_counts[10];
}

/* Exact 0x800FED94..0x800FEE18: visible ship-target sprite/update. */
/* Original: SCUBA_FUN_800fed94. */
/* Original: SCUBA_800FED94. */
static void scuba_ship_target_update(SCUBA_SHIP_TARGET *object)
{
#ifdef XPORT_NATIVE
    {
        static sint32 seen;
        if (!seen && getenv("OA_STAGE20_TRACE"))
        {
            uint8 before = g_objective_counts[10];
            FILE *file = fopen("../status/ship-wrecking-native.log", "a");
            if (file)
            {
                fprintf(file, "SHIP_NATIVE_UPDATE xyz=%d,%d,%d health=%d\n", object->collision.x, object->collision.y, object->collision.z, object->health);
                fclose(file);
            }
            seen = 1;
            scuba_ship_target_damage(object, (FLASHABLE *)object);
            file = fopen("../status/ship-wrecking-native.log", "a");
            if (file)
            {
                fprintf(file, "SHIP_NATIVE_DAMAGE health=%d counter10_before=%u counter10_after=%u\n", object->health, before, g_objective_counts[10]);
                fclose(file);
            }
            return;
        }
    }
#endif
    if (world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0xc800))
    {
        render_world_sprite(0x2dc00, object->collision.x, object->collision.y, object->collision.z, 0, 0x1000, 0x1000, 0, 0, 2, 0, 0);
        object_hit_flash_apply(object);
    }
}

/* Exact 0x800FECE4..0x800FED94: type-0x73 ship target constructor. */
/* Original: SCUBA_FUN_800fece4. */
/* Original: SCUBA_800FECE4. */
void scuba_ship_target_create(EFFECT *effect)
{
    SCUBA_SHIP_TARGET *object = (SCUBA_SHIP_TARGET *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)scuba_ship_target_update);
    if (object == 0)
        return;
    object->collision.x = effect->x;
    object->collision.y = effect->y + 0x2000;
    object->collision.z = effect->z + 0x2100;
    object->health = 0x14;
    object->collision.receives_mask = 1;
    object->collision.callback_18 = (FUNC_COLLISION_CALLBACK)scuba_ship_target_damage;
    collision_box_set(object, 0x40, 0x80, 0x20);
    object->collision.box_y /= 2;
    effect->type = 0;
    object->owner_effect = effect;
#ifdef XPORT_NATIVE
    if (getenv("OA_STAGE20_TRACE"))
    {
        static sint32 count;
        FILE *file = fopen("../status/ship-wrecking-native.log", count ? "a" : "w");
        if (file)
        {
            fprintf(file, "SHIP_NATIVE_CTOR n=%d xyz=%d,%d,%d health=%d damage=800fee18 update=800fed94\n", count + 1, object->collision.x, object->collision.y, object->collision.z, object->health);
            fclose(file);
        }
        ++count;
    }
#endif
}

/* Exact 0x800FDB64..0x800FDDC8: tug target update/render controller. */
/* Original: SCUBA_FUN_800fdb64. */
/* Original: SCUBA_800FDB64. */
static void scuba_tug_target_update(SCUBA_TUG_TARGET *o)
{
    MODEL_NODE *node = &o->nodes[0];
#ifdef XPORT_NATIVE
    {
        static sint32 seen;
        if (!seen && getenv("OA_TUG_TRACE"))
        {
            FILE *file = fopen("../status/tug-o-war-native.log", "a");
            if (file)
            {
                fprintf(file, "TUG_NATIVE_UPDATE oscillation=%d step=%d xyz=%d,%d,%d\n", o->oscillation, o->oscillation_step, o->collision.x, o->collision.y, o->collision.z);
                fclose(file);
            }
            seen = 1;
        }
    }
#endif
    if (!world_object_is_visible(o->collision.x, o->collision.y, o->collision.z, 0x15e00))
        return;
    if (o->sink_ticks != 0)
    {
        if (o->sink_ticks == 0x64)
            timed_callback_create(scuba_death_particle_emit, o->collision.x, o->collision.y, o->collision.z, 0x28, 2);
        if (o->sink_ticks == 0x7d)
            scuba_blast_effect_create(&o->collision);
        if (o->sink_ticks >= 0x82)
        {
            object_destroy(o);
            return;
        }
        ++o->sink_ticks;
        o->collision.y += 0x200;
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
    node->rotation_x = (sint16)((uint16)node->rotation_x & 0xfff);
    node->rotation_z = (sint16)((uint16)node->rotation_z & 0xfff);
    node->translation_x = o->collision.x - g_camera_world_x;
    node->translation_y = o->collision.y - g_camera_world_y;
    node->translation_z = o->collision.z - g_camera_world_z;
    g_render_depth_bucket = 0x480 - (g_render_row_world_z - o->collision.z) / 0x100;
    g_current_model_world_z = o->collision.z;
    if (o->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --o->flash_clut_ticks;
    }
    g_model_render_frame = g_current_render_frame;
    model_render_node(node, (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
    hierarchy_collision_box_update(o, &o->nodes[1], 4);
}

/* Exact 0x800FDDC8..0x800FDE7C: tug target damage/counter transition. */
/* Original: SCUBA_FUN_800fddc8. */
/* Original: SCUBA_800FDDC8. */
static void scuba_tug_target_damage(SCUBA_TUG_TARGET *o, FLASHABLE *hit)
{
    if (o->sink_ticks != 0)
        return;
    o->flash_clut_ticks = 1;
    o->health -= hit->field_78;
    scuba_blast_sprite_create(hit->collision.x, hit->collision.y, hit->collision.z);
    if (hit->collision.object_type == 0x64)
        hit->field_78 = 0;
    if (o->health <= 0)
    {
        o->sink_ticks = 1;
        --g_objective_counts[9];
    }
}

/* Exact 0x800FD9F8..0x800FDB64: type-0x6D tug target constructor. */
/* Original: SCUBA_FUN_800fd9f8. */
/* Original: SCUBA_800FD9F8. */
void scuba_tug_target_create(EFFECT *effect)
{
    SCUBA_TUG_TARGET *object = (SCUBA_TUG_TARGET *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)scuba_tug_target_update);
    const sint16 *indices = (const sint16 *)win_code_module_address(0x800ff214);
    sint32 n;
    effect->type = 0;
    if (object == 0)
        return;
    for (n = 0; n < 5; n++)
    {
        MODEL_NODE *node = &object->nodes[n];
        object->node_table[n] = node;
        node->model_id = indices[n] == -1 ? 0 : g_scuba_tug_model_base + indices[n];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(g_scuba_tug_initial_pose, object->node_table, 1);
    object->nodes[0].scale = 0x1c00;
    object->collision.x = effect->x;
    object->collision.y = effect->y;
    object->collision.z = effect->z + 0x4000;
    object->collision.receives_mask = 1;
    object->collision.callback_18 = (FUNC_COLLISION_CALLBACK)scuba_tug_target_damage;
    object->health = 0x3c;
    object->oscillation_step = 2;
    object->owner_effect = effect;
    object->nodes[0].rotation_x = 0;
    object->nodes[0].rotation_z = 0;
    object->nodes[0].rotation_y = (sint16)(random_range(0x200) + 0x300);
#ifdef XPORT_NATIVE
    if (getenv("OA_TUG_TRACE"))
    {
        static sint32 count;
        FILE *file = fopen("../status/tug-o-war-native.log", count ? "a" : "w");
        if (file)
        {
            fprintf(file, "TUG_NATIVE_CTOR n=%d xyz=%d,%d,%d health=%d state=%d damage=800fddc8\n", count + 1, object->collision.x, object->collision.y, object->collision.z, object->health, object->sink_ticks);
            fclose(file);
        }
        ++count;
    }
#endif
}

/* Exact 0x800FC690..0x800FC784: calculate a 1/256 route step and advance. */
/* Original: SCUBA_FUN_800fc690. */
/* Original: SCUBA_800FC690. */
static void scuba_route_step_advance(SCUBA_VECTOR *position, const SCUBA_VECTOR *target, SCUBA_VECTOR *step)
{
    sint32 line[6];
    fixed_dda_initialize(position->x, position->y, position->z, target->x, target->y, target->z, line);
    step->x = line[3] / 0x100;
    step->y = line[4] / 0x100;
    step->z = line[5] / 0x100;
    position->x += step->x;
    position->y += step->y;
    position->z += step->z;
}

/* Exact 0x800FC784..0x800FC7C4: initialize an effect-defined route. */
/* Original: SCUBA_FUN_800fc784. */
/* Original: SCUBA_800FC784. */
static void scuba_route_initialize(SCUBA_ROUTE *route, EFFECT *effect)
{
    route->count = (sint16)route_waypoints_collect((sint16)effect->values[0], -1, (void **)route->points);
    route->direction = 1;
}

/* Exact 0x800FC7C4..0x800FC8E4: choose/advance the current waypoint. */
/* Original: SCUBA_FUN_800fc7c4. */
/* Original: SCUBA_800FC7C4. */
static SCUBA_VECTOR *scuba_route_waypoint_advance(SCUBA_ROUTE *route, const SCUBA_VECTOR *position)
{
    EFFECT *point = route->points[route->index];
    SCUBA_VECTOR *target = (SCUBA_VECTOR *)scuba_route_target;
    target->x = point->x;
    target->y = point->y;
    target->z = point->z;
    if (target->x - 0x1000 < position->x && position->x < target->x + 0x1000 && target->y - 0x1000 < position->y && position->y < target->y + 0x1000 && target->z - 0x1000 < position->z && position->z < target->z + 0x1000)
    {
        sint16 index = (sint16)((uint16)route->index + route->direction);
        route->index = index;
        if (index == route->count)
            route->index = 0;
        if (route->index < 0)
            route->index = (sint16)((uint16)route->count - 1);
        point = route->points[route->index];
        target->x = point->x;
        target->y = point->y;
        target->z = point->z;
    }
    return target;
}

/* Exact 0x800FC8E4..0x800FC920: collision probe without inflicting damage. */
/* Original: SCUBA_FUN_800fc8e4. */
/* Original: SCUBA_800FC8E4. */
static void scuba_player_collision_probe(FLASHABLE *hit, SCUBA_PLAYER *player)
{
    sint16 damage = hit->field_78;
    hit->field_78 = 0;
    scuba_player_damage_apply(hit, player);
    hit->field_78 = damage;
}

/* 0x800FBF30 / 0x800FC230 / 0x800FC644. */
/* Original: SCUBA_FUN_800fbf30. */
/* Original: SCUBA_800FBF30. */
static void scuba_projectile_trail_update(SPRITE *object)
{
    sint16 value = (sint16)((uint16)object->rotation_step_x - 3);
    object->rotation_step_x = value < 8 ? 8 : value;
    if (object->field_a8 < -0x7f)
        object->animation.next_address = 0;
    if (object->field_a8 < -0x40)
        object->field_aa = -2;
}

/* Original: SCUBA_FUN_800fc230. */
/* Original: SCUBA_800FC230. */
static void scuba_sprite_animation_stop(SPRITE *object)
{
    object->animation.next_address = 0;
}

/* 0x800FBF90: fired underwater projectile callback. */
/* Original: SCUBA_FUN_800fbf90. */
/* Original: SCUBA_800FBF90. */
static void scuba_projectile_update(SPRITE *object)
{
    sint32 mask = object->collision.object_type == 0x64 ? 3 : 7;
    sint32 floor_y;
    if (object->damage <= 0)
    {
        object->animation.next_address = 0;
        return;
    }
    if ((g_frame_counter & mask) == 0)
    {
        SPRITE *trail = world_sprite_create(object->collision.x, object->collision.y, object->collision.z, data32(0x800ff1b0));
        sint32 v;
        trail->scale_x = 0x200;
        trail->scale_step = 0xaa;
        trail->frame_callback = (FUNC_COLLISION_UPDATE)scuba_projectile_trail_update;
        trail->rotation_step_x = 0x55;
        trail->field_aa = -4;
        trail->field_ae = 1;
        trail->rotation_x = (sint16)((g_frame_counter << 6) & 0x3ff);
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
    if (object->field_cc != 0 || object->field_d0 != 0 || object->field_d4 != 0)
    {
        if ((g_frame_counter & 3) == 0)
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
    floor_y = map_floor_height_at(object->collision.x, object->collision.y, object->collision.z);
    if (swept_collision_test(object->collision.x - object->velocity_x, object->collision.y - object->velocity_y, object->collision.z - object->velocity_z, &object->collision.x, &object->collision.y, &object->collision.z) != 0 || floor_y < object->collision.y)
    {
        object->frame_callback = (FUNC_COLLISION_UPDATE)scuba_sprite_animation_stop;
        scuba_blast_sprite_create(object->collision.x, object->collision.y - 0x1800, object->collision.z);
    }
}

/* 0x800FC398: homing correction applied after the generic projectile path. */
/* Original: SCUBA_FUN_800fc398. */
/* Original: SCUBA_800FC398. */
static void scuba_homing_projectile_update(SPRITE *object)
{
    sint32 vx = object->velocity_x, vy = object->velocity_y, vz = object->velocity_z;
    scuba_projectile_update(object);
    object->velocity_x = vx;
    object->velocity_y = vy;
    object->velocity_z = vz;
    if (object->collision.x < g_player_world_x && object->velocity_x < 0x300)
        object->velocity_x += 0x10;
    if (g_player_world_x < object->collision.x && object->velocity_x >= -0x2ff)
        object->velocity_x -= 0x10;
    if (object->collision.y < g_player_world_y && object->velocity_y < 0x300)
        object->velocity_y += 0x10;
    if (g_player_world_y < object->collision.y && object->velocity_y >= -0x2ff)
        object->velocity_y -= 0x10;
    if (object->collision.z < g_player_world_z && object->velocity_z < 0x300)
        object->velocity_z += 0x10;
    if (g_player_world_z < object->collision.z && object->velocity_z >= -0x2ff)
        object->velocity_z -= 0x10;
}

/* 0x800FC238: aimed projectile constructor. */
/* Original: SCUBA_FUN_800fc238. */
/* Original: SCUBA_800FC238. */
static SPRITE *scuba_projectile_create(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, sint32 damage)
{
    sint32 line[6], value;
    SPRITE *object;
    fixed_dda_initialize(x0, y0, z0, x1, y1, z1, line);
    object = world_sprite_create(x0, y0, z0, data32(0x800ff1d4));
    object->field_a8 = -0x18;
    object->field_ae = 1;
    object->velocity_x = line[3] / 0x100;
    object->velocity_y = line[4] / 0x100;
    value = line[5];
    if (value < 0)
        value += 0xff;
    object->velocity_z = value >> 8;
    object->frame_callback = (FUNC_COLLISION_UPDATE)scuba_homing_projectile_update;
    object->collision.receives_mask = 2;
    object->rotation_step_x = 0x66;
    object->lifetime = (sint16)damage;
    object->damage = 0x66;
    object->collision.callback_14 = scuba_player_damage_apply;
    collision_box_set(object, 0x10, 0x10, 0x10);
    sound_play_positional(0x9e, 0, 0x7f, x1, y1, z1);
    return object;
}

/* Exact 0x800FCBA4..0x800FD04C: routed submarine update, aim and fire. */
/* Original: SCUBA_FUN_800fcba4. */
/* Original: SCUBA_800FCBA4. */
static void scuba_submarine_update(SCUBA_SUBMARINE *object)
{
    SPRITE *shot;
    sint32 line[6], vx, vy, vz, horizontal, sound;
    scuba_trace(2, "UPDATE65", (uint8 *)object - 4);
    if (world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0x13800) == 0)
    {
        object->owner_effect->type = 0x65;
        sound_voice_stop(&object->sound_handle);
        object_destroy(object);
        return;
    }
    if (g_sound_handles_invalidated != 0)
        object->sound_handle = -1;
    if (object->sound_handle == -1)
        object->sound_handle = (sint16)sound_play_positional(0xa0, 3, 0xa0, object->collision.x, object->collision.y, object->collision.z);
    sound = (sint32)(intptr)sound_spatial_volume_calculate(0xa0, object->collision.x, object->collision.y, object->collision.z);
    if (sound != 0)
        sound_voice_spatial_volume_update((void *)(intptr)sound, object->sound_handle);
    object->nodes[3].rotation_z = (sint16)((g_frame_counter << 8) & 0xfff);
    scuba_route_step_advance((SCUBA_VECTOR *)&object->collision.x, scuba_route_waypoint_advance(&object->body_route, (SCUBA_VECTOR *)&object->collision.x), &object->body_route.step);
    scuba_route_step_advance(&object->aim_position, scuba_route_waypoint_advance(&object->aim_route, &object->aim_position), &object->aim_route.step);
    fixed_dda_initialize(object->collision.x, object->collision.y, object->collision.z, object->aim_position.x, object->aim_position.y, object->aim_position.z, line);
    vx = line[3] / 0x600;
    vy = line[4] / 0x600;
    vz = line[5] / 0x600;
    object->target_yaw = (sint16)(fixed_angle_from_vector(vx, vz) * 4);
    horizontal = (abs(vx) < abs(vz)) ? abs(vz) + abs(vx) / 2 : abs(vx) + abs(vz) / 2;
    object->target_pitch = (sint16)((-fixed_angle_from_vector(vy, horizontal) * 4) & 0xfff);
    object->nodes[0].rotation_y = angle_approach_wrapped(object->target_yaw, 0x40, object->nodes[0].rotation_y);
    object->nodes[0].rotation_x = angle_approach_wrapped(object->target_pitch, 0x40, object->nodes[0].rotation_x);
    object->nodes[0].translation_x = object->collision.x - g_camera_world_x;
    object->nodes[0].translation_y = object->collision.y - g_camera_world_y;
    object->nodes[0].translation_z = object->collision.z - g_camera_world_z;
    vx = g_render_row_world_z - object->collision.z;
    g_render_depth_bucket = 0x480 - (vx < 0 ? (vx + 0xff) >> 8 : vx >> 8);
    g_current_model_world_z = object->collision.z;
    if (object->flash_clut_ticks != 0)
    {
        g_model_clut_override = g_hit_flash_clut;
        --object->flash_clut_ticks;
    }
    g_model_render_frame = g_current_render_frame;
    model_render_node(&object->nodes[0], &scuba_identity);
    hierarchy_collision_box_update(object, &object->nodes[4], 4);
    g_model_clut_override = 0;
    if (object->fire_countdown == 0)
    {
        vx = angle_to_player(object->collision.x, object->collision.z, 1) * 4 - object->nodes[0].rotation_y;
        if (vx < 0)
            vx = -vx;
        if (vx < 0x200)
        {
            object->fire_countdown = (sint16)(random_range(0x1f) + 0x28);
            shot = scuba_projectile_create(object->nodes[3].world_x, object->nodes[3].world_y + 0x1000, object->nodes[3].world_z, object->collision.x, object->collision.y + 0x1000, object->collision.z, 0x78);
            shot->collision.x += shot->velocity_x * 0x8c;
            shot->collision.y += shot->velocity_y * 0x8c;
            shot->collision.z += shot->velocity_z * 0x8c;
        }
    }
    else
        --object->fire_countdown;
}

/* Exact 0x800FD04C..0x800FD108: large diver-enemy hit/death callback. */
/* Original: SCUBA_FUN_800fd04c. */
/* Original: SCUBA_800FD04C. */
static void scuba_submarine_damage(SCUBA_SUBMARINE *object, FLASHABLE *hit)
{
    object->health = (sint16)((uint16)object->health - (uint16)hit->field_78);
    if (hit->collision.object_type != 2)
    {
        scuba_blast_sprite_create(object->collision.x, object->collision.y, object->collision.z);
        hit->field_78 = 0;
    }
    object->flash_clut_ticks = 1;
    if (object->health <= 0)
    {
        sound_voice_stop(&object->sound_handle);
        timed_callback_create(scuba_death_particle_emit, object->collision.x, object->collision.y, object->collision.z, 0x28, 2);
        object_destroy(object);
    }
}

/* Exact 0x800FC920..0x800FCBA4: type-0x65 routed submarine constructor. */
/* Original: SCUBA_FUN_800fc920. */
/* Original: SCUBA_800FC920. */
void scuba_submarine_create(EFFECT *effect)
{
    static const sint8 models[8] = {3, 2, 0, 1, -1, -1, -1, -1};
    SCUBA_SUBMARINE *object;
    sint32 n;
    scuba_trace(1, "CTOR65", effect);
    effect->type = 0;
    object = (SCUBA_SUBMARINE *)runtime_heap_allocate(sizeof(*object));
    object->collision.update = (FUNC_COLLISION_UPDATE)scuba_submarine_update;
    linked_list_append(g_object_list, object);
    object->collision.object_type = 0x65;
    for (n = 0; n < 8; n++)
    {
        MODEL_NODE *node = &object->nodes[n];
        object->node_table[n] = node;
        node->model_id = models[n] < 0 ? 0 : g_overlay_model_base_slot_4000 + models[n];
        node->render_flags = 0x3f;
    }
    object->nodes[0].scale = 0x0d55;
    model_initialize_pose(g_overlay_initial_pose_slot_4004, object->node_table, 1);
    object->owner_effect = effect;
    object->collision.z = g_player_world_z + 0x5000;
    if (random_range(1) != 0)
    {
        object->collision.y = g_player_world_y;
        object->collision.x = ((object->collision.z - g_camera_world_z) * 200) / 320 + g_camera_world_x;
    }
    else
    {
        object->collision.x = g_camera_world_x;
        object->collision.y = -((object->collision.z - g_camera_world_z) * 150) / 355 + g_camera_world_y;
    }
    object->collision.x = object->aim_position.x = effect->x;
    object->collision.y = object->aim_position.y = effect->y;
    object->collision.z = object->aim_position.z = effect->z;
    object->collision.receives_mask = 3;
    object->collision.callback_18 = (FUNC_COLLISION_CALLBACK)scuba_submarine_damage;
    object->collision.callback_14 = scuba_player_collision_probe;
    object->health = 0x0f;
    scuba_route_initialize(&object->body_route, effect);
    scuba_route_initialize(&object->aim_route, effect);
    for (n = 0; n < 0xc0; n++)
        scuba_route_step_advance(&object->aim_position, scuba_route_waypoint_advance(&object->aim_route, &object->aim_position), &object->aim_route.step);
    object->sound_handle = -1;
}

/* 0x800FDF54: small underwater particle. Incoming a0/a1/a2 are forwarded
 * unchanged to FUN_80090758 by the original instructions. */
/* Original: SCUBA_FUN_800fdf54. */
/* Original: SCUBA_800FDF54. */
static SPRITE *scuba_particle_create(sint32 x, sint32 y, sint32 z)
{
    SPRITE *effect = world_sprite_create(x, y, z, data32(0x800ff170));
    effect->field_a8 = -0x18;
    effect->field_aa = -2;
    effect->scale_x = 1;
    effect->scale_step = 0x80;
    effect->velocity_x = random_range(0x100) - 0x80;
    effect->velocity_y = (~random_range(2)) << 8;
    effect->acceleration_y = -0x10;
    return effect;
}

/* Exact 0x800FE1A8..0x800FE74C: routed diver update/render/fire logic. */
/* Original: SCUBA_FUN_800fe1a8. */
/* Original: SCUBA_800FE1A8. */
static void scuba_diver_enemy_update(SCUBA_DIVER *object)
{
    static const uint8 frames[8] = {0x18, 0x12, 0, 0x0c, 6, 0x0c, 0, 0x12};
    EFFECT *point;
    sint32 state;
    scuba_trace(0x200, "UPDATE6E", (uint8 *)object - 4);
    state = mission_enemy_update_visibility(&object->collision, object->owner_effect, object->collision.object_type, (uint16)g_scuba_diver_sprite_clut);
    sint32 target_x, target_y, target_z, delta, step, frame;
    if (state == 0)
        return;
    point = object->route_points[object->route_index];
    target_x = point->x;
    target_y = point->y;
    target_z = point->z;
    if (target_x - 0x1000 < object->collision.x && object->collision.x < target_x + 0x1000 && target_z - 0x1000 < object->collision.z && object->collision.z < target_z + 0x1000 && target_y - 0x1000 < object->collision.y && object->collision.y < target_y + 0x1000)
    {
        sint16 index = (sint16)((uint16)object->route_index + object->route_direction);
        object->route_index = index;
        if (index == object->route_count - 1)
            object->route_direction = -1;
        if (object->route_index == 0)
            object->route_direction = 1;
        point = object->route_points[object->route_index];
        target_x = point->x;
        target_y = point->y;
        target_z = point->z;
        fixed_dda_initialize(object->collision.x, object->collision.y, object->collision.z, target_x, target_y, target_z, object->path);
        object->path[3] *= object->route_speed;
        object->path[4] *= object->route_speed;
        object->path[5] *= object->route_speed;
        if (object->half_speed != 0)
        {
            object->path[3] /= 2;
            object->path[4] /= 2;
            object->path[5] /= 2;
        }
        delta = target_x - object->collision.x;
        if (delta < 0)
            delta += 0xfff;
        step = target_z - object->collision.z;
        if (step < 0)
            step += 0xfff;
        object->desired_direction = (uint16)((fixed_angle_from_vector(delta >> 12, step >> 12) & 0x3ff) / 0x80);
    }
    if (object->fire_countdown != 0)
        --object->fire_countdown;
    else
    {
        object->fire_countdown = (sint16)(random_range(0xc8) + 0x12c);
        object->desired_direction = object->previous_target_direction;
    }
    if (object->fire_countdown >= 0x65)
    {
        fixed_dda_advance(object->path);
        object->previous_target_direction = object->desired_direction;
    }
    else
        object->desired_direction = (uint16)angle_to_player(object->collision.x, object->collision.z, 0x80);
    object->collision.x = object->path[0] / 0x100;
    object->collision.y = object->path[1] / 0x100;
    object->collision.z = object->path[2] / 0x100;
    if ((g_frame_counter & 3) == 0 && object->direction != (sint16)object->desired_direction)
    {
        sint16 current = object->direction, target = (sint16)object->desired_direction;
        step = target < current ? -1 : 1;
        delta = current - target;
        if (delta < 0)
            delta = -delta;
        if (delta >= 4)
            step = -step;
        object->direction = (sint16)((current + step) & 7);
    }
    if ((g_frame_counter & 7) == 0)
        ++object->animation_frame;
    if ((sint16)object->animation_frame >= 6)
        object->animation_frame = 0;
    object->nodes[1].scale = 0xa3;
    object->nodes[1].rotation_y = (sint16)((object->direction << 9) & 0xfff);
    object->nodes[1].translation_x = object->collision.x - g_camera_world_x;
    object->nodes[1].translation_y = object->collision.y - (g_camera_world_y + 0xc00);
    object->nodes[1].translation_z = object->collision.z - g_camera_world_z;
    g_model_render_frame = 0;
    model_render_node(&object->nodes[1], &scuba_identity);
    if (object->vram_descriptor.region_token != 0 && state != 2)
    {
        frame = 0x2d400 + frames[object->direction] + object->animation_frame;
        g_next_frame_object->light_delta = 0;
        render_world_sprite((uint32)frame, object->collision.x, object->collision.y + player_bob_offset_update(&object->vertical_animation), object->collision.z, object->direction < 4, 0x1000, 0x1000, (POLY_FT4 *)object->collision.prim, 0, 2, &object->vram_descriptor, 0);
        player_hit_flash_update(object);
        if ((g_frame_counter & 0x40) != 0)
        {
            if ((g_frame_counter & 0x3f) == 0)
                sound_play_positional((sint16)(random_range(3) + 0x98), 0, 0x7f, object->collision.x, object->collision.y, object->collision.z);
            if ((g_frame_counter & 3) == 0 && (g_frame_counter & 0x3f) < 0x10)
                scuba_particle_create(object->nodes[3].world_x, object->nodes[3].world_y, object->nodes[3].world_z);
        }
    }
    if (object->fire_countdown == 0x14)
        scuba_projectile_create(object->nodes[4].world_x, object->nodes[4].world_y, object->nodes[4].world_z, object->nodes[2].world_x, object->nodes[2].world_y, object->nodes[2].world_z, 0x64);
}

/* Exact 0x800FDFD4..0x800FE1A8: type-0x6E routed diver constructor. */
/* Original: SCUBA_FUN_800fdfd4. */
/* Original: SCUBA_800FDFD4. */
void scuba_diver_enemy_create(EFFECT *effect)
{
    SCUBA_DIVER *object;
    sint32 n, type = effect->type;
    scuba_trace(0x100, "CTOR6E", effect);
    if (g_mission_enemy_update_count >= 6)
        speech_random_request(5);
    object = (SCUBA_DIVER *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)scuba_diver_enemy_update);
    setPolyFT4(object->collision.prim);
    setShadeTex(object->collision.prim, 0);
    object->collision.receives_mask = 1;
    object->collision.callback_18 = (FUNC_COLLISION_CALLBACK)destroyable_object_damage_and_reward;
    collision_box_set(object, 0x64, 0x30, 0x30);
    object->health = 4;
    object->collision.object_type = (sint16)type;
    object->collision.frame_descriptor = &object->vram_descriptor;
    animation_set_owner(&object->animation, object);
    object->collision.y = effect->y;
    collision_box_set(object, 0x40, 0x40, 0x10);
    object->route_points[0] = effect;
    object->owner_effect = effect;
    effect->type = 0;
    object->route_count = (sint16)(route_waypoints_collect((sint16)effect->values[0], 0, (void **)object->route_points) + 1);
    object->route_direction = 1;
    object->fire_countdown = 0x15;
    if ((sint16)type != 0x44)
    {
        object->collision.x = effect->x;
        object->collision.z = effect->z;
    }
    object->route_speed = 1;
    g_overlay_initial_pose_slot_40f0 = (void *)data32(0x800fefbc);
    for (n = 0; n < 6; n++)
    {
        object->node_table[n] = &object->nodes[n];
        object->nodes[n].render_flags = 0x3f;
    }
    model_initialize_pose(g_overlay_initial_pose_slot_40f0, object->node_table, 1);
}

/* Original: SCUBA_FUN_800fc644. */
/* Original: SCUBA_800FC644. */
static void scuba_distant_water_particle_update(SPRITE *object)
{
    sint16 value = (sint16)((uint16)object->field_a8 + object->field_c8);
    object->field_a8 = value;
    if (value >= -0x59)
        object->field_c8 = -2;
    if (object->field_a8 < -0x7f)
        object->animation.next_address = 0;
}

/* Exact 0x800FDF00..0x800FDF54: type-0x70 periodic bubble emitter. */
/* Original: SCUBA_FUN_800fdf00. */
/* Original: SCUBA_800FDF00. */
void scuba_bubble_emitter_update(EFFECT *effect)
{
    sint16 delay;
    scuba_trace(0x400, "UPDATE70", effect);
    delay = (sint16)effect->values[2];
    if (delay != 0)
    {
        effect->values[2] = (uint16)(delay - 1);
        return;
    }
    effect->values[2] = effect->values[1];
    scuba_particle_create(effect->x, effect->y + 0x4000, effect->z);
}

/* Exact 0x800FE74C..0x800FE75C: deliberate type-0x6F no-op callback. */
/* Original: SCUBA_FUN_800fe74c. */
/* Original: SCUBA_800FE74C. */
void scuba_noop_effect_update(EFFECT *effect)
{
    scuba_trace(0x800, "UPDATE6F", effect);
}

/* 0x800FC528: periodic distant water particle. */
/* Original: SCUBA_FUN_800fc528. */
/* Original: SCUBA_800FC528. */
static void scuba_distant_water_particle_create(void)
{
    sint32 z = g_camera_world_z + ((random_range(0x100) + 0x12c) << 8);
    sint32 x = (random_range(0x140) * (z - g_camera_world_z)) / 320 + g_camera_world_x - 0xa000;
    SPRITE *effect;
    if (g_previous_camera_world_x < g_camera_world_x)
        x += 0x6000;
    if (g_camera_world_x < g_previous_camera_world_x)
        x -= 0x6000;
    effect = world_sprite_create(x, g_camera_world_y, z, data32(0x800ff19c));
    effect->field_a8 = -0x7f;
    effect->scale_x = (uint16)(random_range(0x1000) + 0x1000);
    effect->scale_y = 0x7000;
    effect->rotation_x = (sint16)((random_range(0x40) - 0x20) & 0x3ff);
    effect->frame_callback = (FUNC_COLLISION_UPDATE)scuba_distant_water_particle_update;
    effect->field_c8 = 1;
    effect->field_ae = 1;
}

/* 0x800FEEF4: remove type 0x71 during room 0x14 and progressively move the
 * original 3x58-cell strip. */
/* Original: SCUBA_FUN_800feef4. */
/* Original: SCUBA_800FEEF4. */
static void scuba_wreck_strip_update(void)
{
    if (g_stage_index == 0x14 && g_objective_counts[0x0a] == 0)
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
/* Original: SCUBA_800FEE68. */
static void scuba_bubble_trail_update(COLLISION *object)
{
    if ((g_frame_counter & 7) == 0)
    {
        SPRITE *effect = scuba_particle_create(object->x, object->y, object->z);
        if (effect != 0)
            effect->acceleration_y = -8;
    }
}

/* Original: SCUBA_FUN_800feeb8. */
/* Original: SCUBA_800FEEB8. */
void scuba_mission_complete_player_update(SCUBA_PLAYER *player)
{
    player_mission_complete_update(player);
    scuba_player_update(player);
    g_hq_world_map_active = 1;
}

/* 0x800FB054..0x800FBE08: exact primary diver update. */
/* Original: SCUBA_FUN_800fb054. */
/* Original: SCUBA_800FB054. */
static void scuba_player_update(SCUBA_PLAYER *player)
{
    sint32 old_x, old_y, old_z, input, raw_direction, frame_base;
    sint32 desired, actual, limit, line[6], motion;
    sint16 direction;
    scuba_wreck_strip_update();
    old_x = player->collision.x;
    old_y = player->collision.y;
    old_z = player->collision.z;
    collision_box_set(player, 0x60, 0x20, 6);
    input = (sint32)g_held_buttons;
    raw_direction = input & 0xf000;
    if (player->collision.update == (FUNC_COLLISION_UPDATE)scuba_mission_complete_player_update)
        input = 8;
    player->desired_velocity.x = player->desired_velocity.y = player->desired_velocity.z = 0;
    player->previous_direction = (sint16)(uint16)player->direction;
    player->desired_direction = (sint16)player_animation_start((sint16)raw_direction);
    if (player->hit_turn_ticks != 0)
    {
        player->desired_direction = (sint16)((g_frame_counter >> 1) & 7);
        --player->hit_turn_ticks;
    }
    if ((g_frame_counter & 1) == 0)
    {
        sint16 current = player->direction, target = player->desired_direction;
        if (current != target)
        {
            sint32 turn = target < current ? -1 : 1;
            desired = current - target;
            if (desired < 0)
                desired = -desired;
            if (desired >= 4)
                turn = -turn;
            player->direction = (sint16)((current + turn) & 7);
        }
    }
    player->nodes[1].rotation_x = 0x800;
    direction = player->direction;
    frame_base = (sint32) * (const sint16 *)((const uint8 *)data32(0x800ff1c4) + direction * 2) + 0x2b800;
    if ((input & 8) != 0)
    {
        frame_base += 0xc;
        player->desired_velocity.y = -0x200;
        player->nodes[1].rotation_x = 0x880;
    }
    if ((input & 2) != 0)
    {
        frame_base += 0x18;
        player->desired_velocity.y = 0x200;
        player->nodes[1].rotation_x = 0x780;
    }
    if ((raw_direction & 0xa000) != 0)
        player->desired_velocity.x = player->direction_x * 0x200;
    if ((input & 0x5000) != 0)
        player->desired_velocity.z = player->direction_z * 0x200;
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
    player->collision.x += player->velocity.x;
    player->collision.y += player->velocity.y;
    player->collision.z += player->velocity.z;
    if (g_camera_limit_top != -1)
    {
        limit = g_camera_limit_top - 0x2000;
        if (player->collision.update == (FUNC_COLLISION_UPDATE)scuba_mission_complete_player_update)
            limit -= 0x5f00;
        if (player->collision.y < limit)
            player->collision.y = limit;
    }
    if (g_camera_limit_bottom != -1 && g_camera_limit_bottom < player->collision.y)
        player->collision.y = g_camera_limit_bottom;
    if (g_room_player_near_z != 0 && player->collision.z < g_room_player_near_z)
        player->collision.z = g_room_player_near_z;
    if (g_player_far_z_limit != -1 && player->collision.z >= g_player_far_z_limit - 0x2600)
        player->collision.z = g_player_far_z_limit - 0x2600;
    if (g_player_near_z_limit != -1 && player->collision.z < g_player_near_z_limit)
        player->collision.z = g_player_near_z_limit;
    player_collision_resolve(old_x, old_y, old_z, &player->collision.x, &player->collision.y, &player->collision.z, 0, 0, 0x40, 0x20, 0x0a);
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
        player->animation_countdown = (sint16)(0xb - (motion >> 6));
        if (player->animation_countdown <= 0)
            player->animation_countdown = 1;
    }
    else
        --player->animation_countdown;
    if (player->animation_frame >= 0x0c)
        player->animation_frame = 0;
    limit = -((player->collision.z - g_camera_world_z) * 0x8c) / 320 + g_camera_world_x;
    if (player->collision.x < limit)
    {
        player->collision.x = limit;
        player->velocity.x = 0;
    }
    limit = ((player->collision.z - g_camera_world_z) * 0x8c) / 320 + g_camera_world_x;
    if (limit < player->collision.x)
    {
        player->collision.x = limit;
        player->velocity.x = 0;
    }
    render_world_sprite((uint32)(frame_base + player->animation_frame), player->collision.x, player->collision.y, player->collision.z, player->direction < 4, 0x1000, 0x1000, (POLY_FT4 *)player->collision.prim, 0, 2, &player->vram_descriptor, 0);
    player_hit_flash_update(player);
    if (g_cd_file_io_enabled == 0)
        player_debug_movement_update(player);
    g_player_world_x = player->collision.x;
    g_player_world_y = player->collision.y;
    g_player_world_z = player->collision.z;
    g_player_displacement_x = player->collision.x - old_x;
    g_player_displacement_y = player->collision.y - old_y;
    g_player_displacement_z = player->collision.z - old_z;
    camera_update(player->collision.x, player->collision.y, player->collision.z, player->desired_direction, 0);
    g_previous_player_world_x = g_player_world_x = player->collision.x;
    g_previous_player_world_y = g_player_world_y = player->collision.y;
    g_previous_player_world_z = g_player_world_z = player->collision.z;
    if ((g_frame_counter & 0xf) == 0 && g_stage_background_resource != 0)
        scuba_distant_water_particle_create();
    player->nodes[1].scale = 0xa3;
    player->nodes[1].rotation_y = (sint16)((player->direction << 9) & 0xfff);
    player->nodes[1].translation_x = player->collision.x - g_camera_world_x;
    player->nodes[1].translation_y = player->collision.y - (g_camera_world_y + 0xc00);
    player->nodes[1].translation_z = player->collision.z - g_camera_world_z;
    g_model_render_frame = 0;
    model_render_node(&player->nodes[1], &scuba_identity);
    if ((g_frame_counter & 0x40) != 0)
    {
        if ((g_frame_counter & 0x3f) == 0)
            sound_play_positional((sint16)(random_range(3) + 0x98), 0, 0x7f, player->collision.x, player->collision.y, player->collision.z);
        if ((g_frame_counter & 3) == 0 && (g_frame_counter & 0x3f) < 0x20)
        {
            scuba_particle_create(player->nodes[3].world_x, player->nodes[3].world_y, player->nodes[3].world_z);
            if (g_invulnerability_cheat_enabled == 0 && player->collision.update != (FUNC_COLLISION_UPDATE)scuba_mission_complete_player_update)
            {
                --player->air;
                if ((player->air & 0x8000) != 0)
                    player->air = 0;
            }
        }
    }
    if (player->direction != player->previous_direction)
        player->turn_flash_ticks = 1;
    if (player->turn_flash_ticks != 0)
    {
        SPRITE *effect;
        --player->turn_flash_ticks;
        effect = world_sprite_create(player->nodes[5].world_x, player->nodes[5].world_y, player->nodes[5].world_z, data32(0x800ff124));
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
            steps = fixed_dda_initialize(player->nodes[4].world_x, player->nodes[4].world_y, player->nodes[4].world_z, player->nodes[2].world_x, player->nodes[2].world_y, player->nodes[2].world_z, line);
            shot = world_sprite_create(player->nodes[2].world_x, player->nodes[2].world_y, player->nodes[2].world_z, data32(0x800ff1d4));
            shot->collision.object_type = 0x64;
            shot->field_a8 = -0x18;
            shot->field_aa = -2;
            shot->field_ae = 1;
            shot->velocity_x = (line[3] / 0x100) * 2;
            shot->velocity_y = (line[4] / 0x100) * 2;
            actual = line[5];
            if (actual < 0)
                actual += 0xff;
            shot->velocity_z = (actual >> 8) * 2;
            shot->rotation_step_x = 0x66;
            shot->frame_callback = (FUNC_COLLISION_UPDATE)scuba_projectile_update;
            shot->collision.sends_mask = 1;
            shot->collision.receives_mask = 4;
            shot->damage = 1;
            collision_box_set(shot, 0x40, 0x10, 0x50);
            shot->collision.box_y /= 2;
            sound_play_positional(0x9e, 0, 0x7f, player->collision.x, player->collision.y, player->collision.z);
            player->fire_latch = 1;
            target = player_special_projectile_create(player->direction << 7);
            if (target != 0)
            {
                shot->field_cc = target->x;
                shot->field_d0 = target->y - 0x2000;
                shot->field_a8 = -0x0c;
                shot->lifetime = 0x96;
                shot->field_d4 = target->z;
            }
            (void)steps;
        }
    }
    else
        player->fire_latch = 0;
    if (player->damage_cooldown != 0)
        --player->damage_cooldown;
    if ((uint16)((uint16)player->damage_cooldown - 1) < 0x45 && (g_frame_counter & 3) == 0)
        player->flash_clut_ticks = 1;
    if ((input & 0x20) != 0)
    {
        if (player->item_button_latch == 0)
        {
            INVENTORY_SLOT *slot = &player->inventory[g_selected_weapon_slot];
            player->item_button_latch = 1;
            if ((player->equipment_flags & 1) == 0 && slot->count != 0)
            {
                if (slot->type == 0x76 && object_count_by_type(0x76) <= 0)
                {
                    SPRITE *item = world_sprite_create(player->collision.x, player->collision.y, player->collision.z, data32(0x800ff084));
                    item->acceleration_y = -0x0a;
                    item->collision.object_type = 0x76;
                    item->frame_callback = (FUNC_COLLISION_UPDATE)scuba_bubble_trail_update;
                    sound_play_positional(0xa3, 0, 0xc8, player->collision.x, player->collision.y, player->collision.z);
                }
                if (slot->type == 0x78 && slot->count != 0 && object_count_by_type(0x79) < 4)
                {
                    PLAYER_ACTION *item = player_action_object_create(player->collision.x, player->collision.y, player->collision.z, 0x79);
                    item->collision.receives_mask = 0;
                    --slot->count;
                }
            }
        }
    }
    else
        player->item_button_latch = 0;
    if ((((input & 0xf) == 0xf) && g_held_buttons != player->previous_buttons) || (player->effect_latch != 0 && (g_frame_counter & 0xf) == 0))
    {
        COLLISION *item = (COLLISION *)object_find_next_by_type(0, 0x79);
        if (item != 0)
        {
            player->effect_latch = 1;
            scuba_blast_effect_create(item);
            object_destroy(item);
        }
        else
            player->effect_latch = 0;
    }
    if ((sint16)player->air >= 0x41 || (g_frame_counter & 8) != 0)
        hud_bar_render(player->particle_state, (sint16)player->air, 0, 0);
    if ((sint16)player->air <= 0 && player->collision.update != (FUNC_COLLISION_UPDATE)scuba_mission_complete_player_update)
    {
        player->air = 0;
        if (--player->health <= 0)
            scuba_player_damage_apply(0, player);
    }
    player->previous_buttons = (uint32)input;
}

/* Exact 0x800FD274..0x800FD47C: drifting hazard movement and render. */
/* Original: SCUBA_FUN_800fd274. */
/* Original: SCUBA_800FD274. */
static void scuba_drifting_hazard_update(SCUBA_DRIFTING_HAZARD *object)
{
    sint32 old_x = object->collision.x, old_y = object->collision.y;
    sint32 old_z = object->collision.z;
    sint32 blocked = 0, ceiling;
    scuba_trace(8, "UPDATE67", (uint8 *)object - 4);
    object->collision.y += object->velocity_y;
    object->velocity_y += 4;
    if (object->collision.y >= g_player_world_y - 0x2000)
    {
        ceiling = g_camera_world_y + ((object->collision.z - g_camera_world_z) * 32) / 355;
        if (object->collision.y >= ceiling)
            blocked = 1;
    }
    object->collision.y += 0x4000;
    if (player_collision_resolve(old_x, old_y + 0x4000, old_z, &object->collision.x, &object->collision.y, &object->collision.z, 0, 0, 0x10, 0x50, 5) != 0)
        blocked = 1;
    object->collision.y -= 0x4000;
    if (blocked)
    {
        scuba_blast_effect_create(&object->collision);
        object_destroy(object);
        return;
    }
    object->node.rotation_y = (sint16)(((uint16)object->node.rotation_y + object->rotation_step_y) & 0xfff);
    object->node.rotation_x = (sint16)(((uint16)object->node.rotation_x + object->rotation_step_x) & 0xfff);
    object->node.translation_x = object->collision.x - g_camera_world_x;
    object->node.translation_y = object->collision.y - g_camera_world_y;
    object->node.translation_z = object->collision.z - g_camera_world_z;
    object->node.model_id = g_scuba_drifting_hazard_model_id;
    ceiling = g_render_row_world_z - object->collision.z;
    g_render_depth_bucket = 0x480 - (ceiling < 0 ? (ceiling + 0xff) >> 8 : ceiling >> 8);
    g_current_model_world_z = object->collision.z;
    g_model_render_frame = g_current_render_frame;
    model_render_node(&object->node, &scuba_identity);
}

/* Exact 0x800FD108..0x800FD274: type-0x67 drifting hazard emitter. */
/* Original: SCUBA_FUN_800fd108. */
/* Original: SCUBA_800FD108. */
void scuba_drifting_hazard_create(EFFECT *effect)
{
    SCUBA_DRIFTING_HAZARD *object;
    sint32 half_x, half_z, x, y, z;
    scuba_trace(4, "CTOR67", effect);
    if ((g_frame_counter & 0x3f) != 0)
        return;
    if ((g_frame_counter & 0x7f) == 0)
        sound_play_positional(0x9f, 6, 0x7f, effect->x, effect->y, effect->z);
    half_x = (sint16)effect->values[0] * 0x4000;
    half_z = (sint16)effect->values[1] * 0x4000;
    x = effect->x + random_range(half_x * 2) - half_x;
    y = g_camera_world_y - (((effect->z - g_camera_world_z) * 96) / 355);
    z = effect->z + random_range(half_z * 2) - half_z;
    scuba_type67_spawn_sequence = g_scuba_drifting_hazard_model_id + 1;
    object = (SCUBA_DRIFTING_HAZARD *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)scuba_drifting_hazard_update);
    object->collision.x = x;
    object->collision.y = y;
    object->collision.z = z;
    object->node.rotation_y = (sint16)random_range(0xfff);
    object->rotation_step_y = random_range(1) * 0x10 - 8;
    object->rotation_step_x = 8;
    object->collision.object_type = 0x67;
}

/* Exact 0x800FD73C..0x800FD960: proximity mine update/render. */
/* Original: SCUBA_FUN_800fd73c. */
/* Original: SCUBA_800FD73C. */
static void scuba_mine_update(SCUBA_MINE *object)
{
    sint32 dx, dy, dz, flash = 0;
    scuba_trace(0x20, "UPDATE69", (uint8 *)object - 4);
#ifdef XPORT_NATIVE
    {
        static sint32 seen;
        if (!seen && getenv("OA_STAGE19_TRACE"))
        {
            FILE *file = fopen("../status/stage19-native.log", "a");
            if (file)
            {
                fprintf(file, "STAGE19_NATIVE_UPDATE69_OBJECT xyz=%d,%d,%d health=%d damage=800fd960\n", object->collision.x, object->collision.y, object->collision.z, object->health);
                fclose(file);
            }
            seen = 1;
        }
    }
#endif
    sint16 phase;
    if (world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0x4000) == 0)
    {
        EFFECT *effect = object->owner_effect;
        object_destroy(object);
        effect->type = 0x69;
        return;
    }
    phase = (sint16)((uint16)object->phase + object->phase_step);
    object->phase = phase;
    if (phase < -0x10)
        object->phase_step = 3;
    if (object->phase >= 0x21)
        object->phase_step = -3;
    dx = g_player_world_x - object->collision.x;
    if (dx < 0)
        dx = -dx;
    dy = g_player_world_y - object->collision.y;
    if (dy < 0)
        dy = -dy;
    dz = g_player_world_z - object->collision.z;
    if (dz < 0)
        dz = -dz;
    if (dx < 0x5000 && dy < 0x5000 && dz < 0x5000)
    {
        sint16 timer = (sint16)((uint16)object->proximity_ticks + 1);
        object->proximity_ticks = timer;
        flash = 0x60;
        if (timer == 1 || timer == 0x1e || timer == 0x2d)
            sound_play_positional(0xa2, 6, 0x7f, object->collision.x, object->collision.y, object->collision.z);
        if (timer >= 0x3d)
        {
            scuba_blast_effect_create(&object->collision);
            object_destroy(object);
            return;
        }
    }
    else
        object->proximity_ticks = 0;
    g_next_frame_object->light_delta = (sint16)((uint16)object->phase + flash);
    render_world_sprite(0x2cc00, object->collision.x, object->collision.y + player_bob_offset_update(&object->vertical_animation), object->collision.z, 0, 0x1000, 0x1000, 0, 0, 0, 0, 0);
    object_hit_flash_apply(object);
}

/* Exact 0x800FD690..0x800FD73C: type-0x69 mine constructor. */
/* Original: SCUBA_FUN_800fd690. */
/* Original: SCUBA_800FD690. */
void scuba_mine_create(EFFECT *effect)
{
    SCUBA_MINE *object;
    scuba_trace(0x10, "CTOR69", effect);
    effect->type = 0;
    object = (SCUBA_MINE *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)scuba_mine_update);
    object->owner_effect = effect;
    object->collision.x = effect->x;
    object->collision.y = effect->y;
    object->collision.z = effect->z;
    object->health = 0x0a;
    object->collision.receives_mask = 1;
    object->collision.callback_18 = (FUNC_COLLISION_CALLBACK)scuba_target_damage;
    collision_box_set(object, 0x40, 0x40, 0x40);
    object->phase_step = -4;
    object->collision.box_y /= 2;
}

/* 0x800FEB90: renderer/update for model attribute 0x71. */
/* Original: SCUBA_FUN_800feb90. */
/* Original: SCUBA_800FEB90. */
static void scuba_model_target_update(SCUBA_MODEL_TARGET *object)
{
    if (world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0x4600) != 0)
    {
        sint32 delta = g_render_row_world_z - object->collision.z;
        sint32 depth = delta < 0 ? (delta + 0xff) >> 8 : delta >> 8;
        g_current_model_id = object->model_id;
        g_render_depth_bucket = 0x490 - depth;
        g_current_model_world_x = object->collision.x;
        g_current_model_world_y = object->collision.y;
        g_current_model_world_z = object->collision.z;
        if (object->fire_countdown < 6)
            g_current_model_world_z += 0x1000;
        g_current_model_shade = 0x80;
        g_current_model_rotation_y = 0;
        g_current_model_rotation_x = 0;
        g_current_model_rotation_z = 0;
        g_current_model_translation_z = 0;
        g_current_model_translation_x = 0;
        model_render();
        if (object->fire_countdown == 0)
        {
            object->fire_countdown = (sint16)(random_range(0x3c) + 0x78);
            scuba_projectile_create(object->collision.x, object->collision.y, object->collision.z, object->collision.x, object->collision.y, object->collision.z - 0x2000, 0x64);
        }
        --object->fire_countdown;
    }
}

/* 0x800FEACC: SLES overlay dispatch for map model attribute 0x71. */
/* Original: FUN_800FEACC. */
/* Original: SCUBA_800FEACC. */
void scuba_model_target_create(sint32 x, sint32 y, sint32 z, sint32 model)
{
    SCUBA_MODEL_TARGET *object = (SCUBA_MODEL_TARGET *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)scuba_model_target_update);
    object->model_id = (sint16)model;
    object->collision.x = x;
    object->collision.y = y;
    object->collision.z = z + 0x2000;
    collision_box_set(object, 0x30, 0x30, 0x30);
    object->collision.receives_mask = 1;
    object->collision.callback_18 = (FUNC_COLLISION_CALLBACK)scuba_target_damage;
    object->health = 3;
    object->collision.object_type = 0x71;
    object->collision.box_y /= 2;
}

/* Original: SCUBA_800FAE70 (0x800FAE70..0x800FB054). */
void scuba_player_create(sint32 x, sint32 y, sint32 z)
{
    SCUBA_PLAYER *player = (SCUBA_PLAYER *)object_create(sizeof(*player), (FUNC_COLLISION_UPDATE)scuba_player_update);
    sint32 index;
    player->field_084 = 1;
    player->air = 0x100;
    SetPolyFT4((POLY_FT4 *)player->collision.prim);
    setShadeTex((POLY_FT4 *)player->collision.prim, 0);
    player->collision.sends_mask = 2;
    ((POLY_FT4 *)player->collision.prim)->clut = g_player_sprite_clut;
    collision_box_set(player, 0x60, 0x20, 6);
    player->collision.x = x;
    player->collision.y = y;
    player->collision.z = z;
    sprite_vram_allocate(&player->vram_descriptor, 0x6c, 0x40, 1);
    player->health = 0x400;
    g_player = (PLAYER *)player;
    player->vram_descriptor.clut = g_player_sprite_clut;
    player_inventory_add(0x76, -1, player);
    player_inventory_add(0x78, 3, player);
    g_overlay_initial_pose_slot_40f0 = (void *)data32(0x800fefbc);
    for (index = 0; index < 6; index++)
    {
        player->node_table[index] = &player->nodes[index];
        player->nodes[index].render_flags = 0x3f;
    }
    model_initialize_pose(g_overlay_initial_pose_slot_40f0, player->node_table, 1);
    g_player_world_x = x;
    g_player_world_y = y;
    g_player_world_z = z;
    hud_bar_initialize(player->particle_state, 0x10, 0x52, 0x40, 0x100);
    scuba_particle_count = 0;
    g_invulnerability_cheat_enabled = 0;
    g_weapon_cheat_enabled = 0;
}
