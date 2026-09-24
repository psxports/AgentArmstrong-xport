#include <stddef.h>
#include "airship.h"
#include "animation.h"
#include "game_sound.h"
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
#include "radar_death.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "vram_alloc.h"

/* Types. */
typedef struct
{
    COLLISION collision;       /* +0x00 */
    uint8 field_078[4];        /* +0x78 */
    VRAM_SPRITE descriptor;    /* +0x7C */
    ANIM animation;            /* +0xAC */
    sint32 field_c8;           /* +0xC8 */
    sint16 state;              /* +0xCC */
    sint16 field_ce;           /* +0xCE */
    sint32 velocity_x;         /* +0xD0 */
    sint32 velocity_z;         /* +0xD4 */
    sint32 render_scale;       /* +0xD8 */
    sint16 departure_started;  /* +0xDC */
    sint16 player_near_frames; /* +0xDE */
    uint8 field_e0[4];         /* +0xE0 */
    EFFECT *owner_effect;      /* +0xE4 */
} TANYA_ACTOR;

typedef struct
{
    COLLISION collision;              /* +0x000 */
    sint16 health;                    /* +0x078 */
    sint16 flash_clut_ticks;          /* +0x07A */
    uint8 field_07c[0x34];            /* +0x07C */
    uint16 fire_countdown;            /* +0x0B0 */
    sint16 field_0b2;                 /* +0x0B2 */
    sint16 field_0b4;                 /* +0x0B4 */
    uint8 field_0b6[6];               /* +0x0B6 */
    sint16 field_0bc;                 /* +0x0BC */
    sint16 field_0be;                 /* +0x0BE */
    sint16 field_0c0;                 /* +0x0C0 */
    sint16 field_0c2;                 /* +0x0C2 */
    MODEL_NODE nodes[11];             /* +0x0C4 */
    uint8 field_4e4[0x120];           /* +0x4E4 */
    MODEL_NODE *node_table[11];       /* +0x604 */
    uint8 field_630[0x10];            /* +0x630 */
    EFFECT *owner_effect;             /* +0x640 */
    sint32 patrol_min_x;              /* +0x644 */
    sint32 patrol_max_x;              /* +0x648 */
    sint32 saved_node3_translation_z; /* +0x64C */
    sint32 field_650[6];              /* +0x650 */
    MAP_MODEL_GROUP *map_group;       /* +0x668 */
    sint32 aim_jitter_x;              /* +0x66C */
    uint8 field_670[4];               /* +0x670 */
} TANYA_GUN_EMPLACEMENT;

#if defined(AP_32BIT)
    #define TANYA_OFFSET_ASSERT(field, offset) typedef char TanyaActor_##field##_at_##offset[(offsetof(TANYA_ACTOR, field) == 0x##offset) ? 1 : -1]
TANYA_OFFSET_ASSERT(descriptor, 7c);
TANYA_OFFSET_ASSERT(animation, ac);
TANYA_OFFSET_ASSERT(state, cc);
TANYA_OFFSET_ASSERT(render_scale, d8);
TANYA_OFFSET_ASSERT(owner_effect, e4);
typedef char TanyaActor_size_e8[(sizeof(TANYA_ACTOR) == 0xe8) ? 1 : -1];
    #undef TANYA_OFFSET_ASSERT
    #define TANYA_GUN_OFFSET_ASSERT(field, offset) typedef char TanyaGun_##field##_at_##offset[(offsetof(TANYA_GUN_EMPLACEMENT, field) == 0x##offset) ? 1 : -1]
TANYA_GUN_OFFSET_ASSERT(nodes, 0c4);
TANYA_GUN_OFFSET_ASSERT(node_table, 604);
TANYA_GUN_OFFSET_ASSERT(owner_effect, 640);
TANYA_GUN_OFFSET_ASSERT(field_650, 650);
TANYA_GUN_OFFSET_ASSERT(map_group, 668);
TANYA_GUN_OFFSET_ASSERT(aim_jitter_x, 66c);
typedef char TanyaGun_size_674[(sizeof(TANYA_GUN_EMPLACEMENT) == 0x674) ? 1 : -1];
    #undef TANYA_GUN_OFFSET_ASSERT
#endif

/* Functions. */
/* Original: FUN_800A48A8. */
static void tanya_update(TANYA_ACTOR *object)
{
    EFFECT *effect = object->owner_effect;
    sint32 old_x = object->collision.x, old_y = object->collision.y;
    sint32 old_z = object->collision.z, render = 1;
    animation_update(&object->animation);
    if (g_primary_objective_states[1] == 2)
    {
        if (object->descriptor.region_token != 0)
            sprite_vram_defer_release(&object->descriptor);
        object_destroy(object);
        return;
    }
    if (!world_object_is_visible(old_x, old_y, old_z, 0xb400))
    {
        if (object->descriptor.region_token != 0)
            sprite_vram_defer_release(&object->descriptor);
        if (object->departure_started == 0)
            effect->type = 0x2c;
        object_destroy(object);
        return;
    }
    if (object->descriptor.region_token == 0)
        sprite_vram_allocate(&object->descriptor, 0x50, 0x56, 1);
    object->collision.y += 0x400;
    if (object->state == 2)
    {
        object->collision.x += object->velocity_x;
        object->collision.z += object->velocity_z;
    }
    player_collision_resolve(old_x, old_y, old_z, &object->collision.x, &object->collision.y, &object->collision.z, 0, 0, 0x20, 0x50, 5);
    if (abs_s32(object->collision.x - g_player_world_x) <= 0xffff && abs_s32(object->collision.y - g_player_world_y) <= 0x7fff)
    {
        if (object->state == 0)
        {
            g_objective_counts[2]++;
            if (object->player_near_frames >= 0x33)
            {
                animation_start(&object->animation, (const sint32 *)player_assets_executable_address(0x800cad34u));
                object->departure_started = 1;
                if (object_find_next_by_type(0, 0x5c) == 0 && object->state == 0)
                {
                    mission_text_message_create((const char *)player_assets_executable_pointer(0x800827d0u));
                    object->field_c8 = 0;
                    object->state = 1;
                }
            }
            else
                ++object->player_near_frames;
        }
    }
    if (object->state != 0 && object->animation.next_address == 0)
    {
        animation_start(&object->animation, (const sint32 *)player_assets_executable_address(0x800cad40u));
        object->state = 2;
        object->velocity_x = 0x100;
        object->velocity_z = -0x20;
        object->render_scale = g_default_model_scale;
    }
    render_world_sprite((sint32)object->animation.frame_offset, object->collision.x, object->collision.y, object->collision.z, render, object->render_scale, object->render_scale, (POLY_FT4 *)object->collision.prim, 0, 2, &object->descriptor, 0);
}

/* Original: FUN_800A479C. */
void tanya_create(EFFECT *effect)
{
    TANYA_ACTOR *object = (TANYA_ACTOR *)runtime_heap_allocate(0xe8);
    object->collision.update = (FUNC_COLLISION_UPDATE)tanya_update;
    linked_list_append(g_object_list, object);
    setPolyFT4(object->collision.prim);
    setShadeTex(object->collision.prim, 0);
    object->collision.x = effect->x;
    object->collision.y = effect->y + 0x2000;
    object->collision.z = effect->z - 0x1000;
    if (g_stage_index == 0x0e)
        object->collision.x += 0x6000;
    object->owner_effect = effect;
    effect->type = 0;
    collision_box_set(object, 0x40, 0x40, 0x10);
    animation_set_owner(&object->animation, object);
    object->descriptor.clut = g_tanya_sprite_clut;
    animation_start(&object->animation, (const sint32 *)player_assets_executable_address(0x800cace0u));
    object->render_scale = g_default_model_scale + 0x200;
}

/* Original: FUN_800AC610. */
static void tanya_gun_emplacement_update(TANYA_GUN_EMPLACEMENT *object)
{
    static const MATRIX identity = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    MODEL_NODE *root = &object->nodes[0], *aim, *muzzle;
    sint32 a, b, angle;
    if (!world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0x8000))
        return;
    root->world_x = object->collision.x - g_camera_world_x;
    root->world_y = object->collision.y - g_camera_world_y;
    root->world_z = object->collision.z - g_camera_world_z;
    player_push_out_of_box(object->collision.x, object->collision.y, object->collision.z, 0x3800, 0xaa00, 0x3800);
    if ((g_frame_counter & 0x3f) == 0)
        object->aim_jitter_x = (random_range(0x40) - 0x20) << 8;
    fixed_dda_initialize(object->nodes[1].world_x, object->nodes[1].world_y, object->nodes[1].world_z, g_player_world_x + object->aim_jitter_x, g_player_world_y - 0x3000, g_player_world_z, object->field_650);
    angle = fixed_angle_from_vector(div_pow2_trunc(object->field_650[3], 11), div_pow2_trunc(object->field_650[5], 11));
    object->nodes[1].rotation_y = angle_approach_wrapped((sint16)(angle << 2), 0x0e, object->nodes[1].rotation_y);
    aim = &object->nodes[g_scuba_stage_active ? 3 : 4];
    fixed_dda_initialize(aim->world_x, aim->world_y, aim->world_z, g_player_world_x + object->aim_jitter_x, g_player_world_y, g_player_world_z, object->field_650);
    a = abs_s32(object->field_650[5]);
    b = abs_s32(object->field_650[3]);
    if (b < a)
        a += div_pow2_trunc(b, 1);
    else
        a = b + div_pow2_trunc(a, 1);
    angle = ((fixed_angle_from_vector(div_pow2_trunc(object->field_650[4], 11), -div_pow2_trunc(a, 11)) << 2) - 0x400 - (g_scuba_stage_active ? 0x400 : 0)) & 0xfff;
    if (angle < 0x190)
        angle = 0x190;
    aim->rotation_x = angle_approach_wrapped((sint16)angle, 0x0c, aim->rotation_x);
    g_model_render_frame = g_current_render_frame;
    g_render_depth_bucket = 0x480 - div_pow2_trunc(g_render_row_world_z + 0x2000 - object->collision.z, 8);
    g_current_model_world_z = object->collision.z;
    if (object->flash_clut_ticks != 0)
    {
        g_model_clut_override = g_hit_flash_clut;
        --object->flash_clut_ticks;
    }
    model_render_node(root, (MATRIX *)&identity);
    g_model_clut_override = 0;
    object->nodes[3].translation_z = object->saved_node3_translation_z;
    if (object->fire_countdown < 0x14 && !g_scuba_stage_active)
    {
        object->nodes[3].translation_z = object->saved_node3_translation_z - ((g_frame_counter << 9) & 0xe00);
        if (object->fire_countdown == 0x13)
            sound_play_positional(0x12, 0, 0x7f, object->collision.x, object->collision.y, object->collision.z);
        muzzle = &object->nodes[5];
        /* PAL 800AC99C..800ACA24: even countdown emits the muzzle sprite. */
        if ((object->fire_countdown & 1) == 0)
        {
            g_render_depth_bucket = 0x480 - div_pow2_trunc(g_render_row_world_z - muzzle->world_z, 8);
            render_world_sprite(0x1e000 | ((object->fire_countdown >> 2) & 1), muzzle->world_x, muzzle->world_y, muzzle->world_z, 0, 0x2200, 0x2200, 0, g_render_depth_bucket, 0, 0, 0);
        }
        /* PAL 800ACA28..800ACA90: independent frame cadence; next node's
         * world XYZ starts at muzzle+0x90 (not +0x94). No axis correction. */
        if ((g_frame_counter & 3) == 0)
        {
            PROJECTILE *shot = projectile_create(muzzle->world_x, muzzle->world_y, muzzle->world_z, object->nodes[6].world_x, object->nodes[6].world_y, object->nodes[6].world_z);
            shot->damage = 0x80;
            shot->collision.sends_mask |= 1;
            shot->collision.receives_mask |= 4;
            shot->collision.object_type = 0;
        }
    }
    if (object->fire_countdown == 0)
    {
        if (g_scuba_stage_active)
            sound_play_positional(0xa9, 0, 0x7f, object->collision.x, object->collision.y, object->collision.z);
        object->fire_countdown = (uint16)(random_range(0x64) + 0x78);
    }
    --object->fire_countdown;
    hierarchy_collision_box_update(object, &object->nodes[g_scuba_stage_active ? 6 : 7], 4);
}

/* Original: FUN_800AC3B8. */
void tanya_gun_emplacement_create(EFFECT *effect)
{
    TANYA_GUN_EMPLACEMENT *object = (TANYA_GUN_EMPLACEMENT *)runtime_heap_allocate(0x674);
    const sint8 *models = (const sint8 *)player_assets_executable_address(g_scuba_stage_active ? 0x800cd9a8u : 0x800cd99cu);
    sint32 count = g_scuba_stage_active ? 10 : 11, index, cell_x, cell_z;
    uint16 group_index;
    object->collision.update = (FUNC_COLLISION_UPDATE)tanya_gun_emplacement_update;
    linked_list_append(g_object_list, object);
    object->collision.object_type = (sint16)effect->type;
    effect->type = 0;
    for (index = 0; index < count; index++)
    {
        MODEL_NODE *node = &object->nodes[index];
        object->node_table[index] = node;
        node->model_id = models[index] < 0 ? 0 : g_tanya_model_base + models[index];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(g_tanya_initial_pose, object->node_table, 1);
    object->owner_effect = effect;
    object->collision.z = effect->z;
    object->collision.x = effect->x;
    object->collision.y = effect->y + 0x1000;
    object->collision.receives_mask = 1;
    object->collision.callback_18 = destructible_hierarchy_damage;
    object->health = (sint16)(g_scuba_stage_active ? 5 : 0x28);
    object->nodes[0].scale = 0x1000;
    object->fire_countdown = 0x3c;
    object->field_0b2 = -1;
    object->field_0b4 = -1;
    object->patrol_max_x = effect->x + 0x10a00;
    object->patrol_min_x = effect->x - 0x10a00;
    object->field_0bc = 0x18;
    object->field_0c0 = 0x1f4;
    object->field_0c2 = 0xc8;
    object->saved_node3_translation_z = object->nodes[3].translation_z;
    cell_x = div_pow2_trunc(object->collision.x, 14);
    cell_z = div_pow2_trunc(g_map_depth_cells * 3 * 0x4000 - object->collision.z, 14);
    group_index = g_map_floor_cells[cell_x + cell_z * g_map_width_cells].group_index;
    object->map_group = (MAP_MODEL_GROUP *)((uint8 *)g_map_model_groups + (uint32)group_index * sizeof(MAP_MODEL_GROUP));
}
