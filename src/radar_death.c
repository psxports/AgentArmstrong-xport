#include <stddef.h>
#include "airship.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "hq.h"
#include "map.h"
#include "mechanoid.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"

/* Types. */
typedef struct
{
    COLLISION collision;              /* +0x000 */
    sint16 health;                    /* +0x078 */
    sint16 flash_clut_ticks;          /* +0x07A */
    uint8 field_07c[0x36];            /* +0x07C */
    sint16 field_0b2;                 /* +0x0B2 */
    uint8 field_0b4[8];               /* +0x0B4 */
    sint16 field_0bc;                 /* +0x0BC */
    uint8 field_0be[6];               /* +0x0BE */
    MODEL_NODE nodes[9];              /* +0x0C4 */
    uint8 field_424[0x1e0];           /* +0x424 */
    MODEL_NODE *node_table[9];        /* +0x604 */
    uint8 field_628[0x18];            /* +0x628 */
    EFFECT *owner_effect;             /* +0x640 */
    uint8 field_644[8];               /* +0x644 */
    sint32 saved_node3_translation_z; /* +0x64C */
    sint32 field_650[6];              /* +0x650 */
    MAP_MODEL_GROUP *map_group;       /* +0x668 */
    uint8 field_66c[8];               /* +0x66C */
} RADAR_VEHICLE;

struct DESTRUCTIBLE_HIERARCHY
{
    COLLISION collision;     /* +0x000 */
    sint16 health;           /* +0x078 */
    sint16 flash_clut_ticks; /* +0x07A */
    uint8 field_07c[0x5c4];  /* +0x07C */
    EFFECT *owner_effect;    /* +0x640 */
    uint8 field_644[0x24];   /* +0x644 */

    union
    {
        MAP_MODEL_GROUP *group;
        MAP_MODEL_INSTANCE *instance;
    } map_link; /* +0x668 */
};

#if defined(AP_32BIT)
    #define RADAR_OFFSET_ASSERT(field, offset) typedef char RadarVehicle_##field##_at_##offset[(offsetof(RADAR_VEHICLE, field) == 0x##offset) ? 1 : -1]
RADAR_OFFSET_ASSERT(nodes, 0c4);
RADAR_OFFSET_ASSERT(field_0b2, 0b2);
RADAR_OFFSET_ASSERT(field_0bc, 0bc);
RADAR_OFFSET_ASSERT(node_table, 604);
RADAR_OFFSET_ASSERT(owner_effect, 640);
RADAR_OFFSET_ASSERT(saved_node3_translation_z, 64c);
RADAR_OFFSET_ASSERT(field_650, 650);
RADAR_OFFSET_ASSERT(map_group, 668);
typedef char RadarVehicle_size_674[(sizeof(RADAR_VEHICLE) == 0x674) ? 1 : -1];
    #undef RADAR_OFFSET_ASSERT
typedef char DestructibleHierarchyOwner_at_640[offsetof(DESTRUCTIBLE_HIERARCHY, owner_effect) == 0x640 ? 1 : -1];
typedef char DestructibleHierarchyInstance_at_668[offsetof(DESTRUCTIBLE_HIERARCHY, map_link) == 0x668 ? 1 : -1];
#endif

/* Functions. */

/* Original: FUN_800AD218. */
static void radar_vehicle_update(RADAR_VEHICLE *object)
{
    static const MATRIX identity = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    MODEL_NODE *root = &object->nodes[0], *aim = &object->nodes[4];
    EFFECT *effect = object->owner_effect;
    sint32 a, b, angle;
    if (!world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0x8000))
    {
        object_destroy(object);
        effect->type = 0x38;
        return;
    }
    player_push_out_of_box(object->collision.x, object->collision.y, object->collision.z, 0x3800, 0xaa00, 0x3800);
    root->world_x = object->collision.x - g_camera_world_x;
    root->world_y = object->collision.y - g_camera_world_y;
    root->world_z = object->collision.z - g_camera_world_z;
    object->nodes[1].rotation_y = (sint16)((object->nodes[1].rotation_y + 0x30) & 0xfff);
    fixed_dda_initialize(aim->world_x, aim->world_y, aim->world_z, g_player_world_x, g_player_world_y, g_player_world_z, object->field_650);
    a = object->field_650[5];
    if (a < 0)
        a = -a;
    b = object->field_650[3];
    if (b < 0)
        b = -b;
    if (b < a)
        a += div_pow2_trunc(b, 1);
    else
        a = b + div_pow2_trunc(a, 1);
    angle = fixed_angle_from_vector(div_pow2_trunc(object->field_650[4], 11), -div_pow2_trunc(a, 11));
    aim->rotation_x = angle_approach_wrapped((sint16)(((angle << 2) - 0x500) & 0xffc), 12, aim->rotation_x);
    g_model_render_frame = g_current_render_frame;
    g_render_depth_bucket = 0x480 - div_pow2_trunc(g_render_row_world_z + 0x2000 - object->collision.z, 8);
    g_current_model_world_z = object->collision.z;
    if (object->flash_clut_ticks != 0)
    {
        g_model_clut_override = g_hit_flash_clut;
        --object->flash_clut_ticks;
    }
    model_render_node(root, (MATRIX *)&identity);
    object->nodes[3].translation_z = object->saved_node3_translation_z;
    hierarchy_collision_box_update(object, &object->nodes[5], 4);
    g_model_clut_override = 0;
}

/* 0x800AD9E0..0x800ADA10: destroyed map instance flicker. */
/* Original: FUN_800AD9E0. */
static void destroyed_map_instance_flicker_update(DESTRUCTIBLE_HIERARCHY *object)
{
    MAP_MODEL_INSTANCE *instance = object->map_link.instance;
    instance->light = (uint8)random_range(0xff);
}

/* Exact non-water branch reached by stage 7 at 0x800ADA10..0x800ADB18. */
/* Original: FUN_800ADA10. */
void large_object_destruction_effect_create(COLLISION *object)
{
    SPRITE *effect;
    player_particle_ring_create(object->x, object->y, object->z, 0x0c, 3);
    camera_shake_start();
    speech_random_request(4);
    expl_flash_create(object->x, object->y - 0x4000, object->z - 0x100);
    timed_callback_create(expl_debris_particle_create, object->x, object->y, object->z, 0x40, 8);
    timed_callback_create(expl_trail_particle_create, object->x, object->y, object->z, 0x30, 0x10);
    effect = world_sprite_create(object->x, object->y, object->z - 0x2800, (const sint32 *)player_assets_executable_address(0x800c8b5cu));
    effect->scale_x = 0x2000;
    sound_play_positional(10, 0, 0x7f, object->x, object->y, object->z);
}

/* Exact 0x800ACB84..0x800ACDA0 shared destructible-object callback. */
/* Original: FUN_800ACB84. */
void destructible_hierarchy_damage(DESTRUCTIBLE_HIERARCHY *object, void *source)
{
    EFFECT *effect;
    MAP_MODEL_GROUP *group;
    MAP_MODEL_INSTANCE *instance;
    sint32 count;
    object->flash_clut_ticks = 1;
    if (object_damage_apply(object, source) == 0)
        return;
    if (object->collision.object_type == 0x35)
        --g_objective_counts[3];
    if (object->collision.object_type == 0x38)
        --g_objective_counts[4];
    if (object->collision.object_type == 0x62)
        --g_objective_counts[8];
    if (object->collision.object_type == 0x75)
        --g_objective_counts[11];
    if (object->collision.object_type == 0x74)
        --g_objective_counts[23];
    object->collision.receives_mask = 0;
    object->collision.update = (FUNC_COLLISION_UPDATE)destroyed_map_instance_flicker_update;
    large_object_destruction_effect_create(&object->collision);
    effect = object->owner_effect;
    effect->values[0] = 999;
    group = object->map_link.group;
    if (group == g_map_model_groups)
        return;
    count = group->active_count;
    instance = (MAP_MODEL_INSTANCE *)(group + 1);
    g_shared_scratch_value = count;
    while (g_shared_scratch_value > 0)
    {
        sint32 delta = ((sint32)instance->y << 8) - object->collision.y;
        if (delta < 0)
            delta = -delta;
        if (delta < 0x4000)
        {
            instance->y = (sint16)(instance->y + g_model_descriptors[(uint16)instance->model_id].half_y + g_model_descriptors[(uint16)g_radar_target_model_id].half_y + 2);
            instance->model_id = g_radar_target_model_id;
            object->map_link.instance = instance;
            return;
        }
        instance++;
        --g_shared_scratch_value;
    }
}

/* Original: FUN_800AD038. */
void radar_vehicle_create(EFFECT *effect)
{
    static const sint8 model_offsets[9] = {3, 2, 1, 0, -1, -1, -1, -1, -1};
    RADAR_VEHICLE *object = (RADAR_VEHICLE *)runtime_heap_allocate(sizeof(*object));
    sint32 index, cell_x, cell_z;
    uint16 group_index;
    object->collision.update = (FUNC_COLLISION_UPDATE)radar_vehicle_update;
    linked_list_append(g_object_list, object);
    object->collision.object_type = (sint16)effect->type;
    effect->type = 0;
    for (index = 0; index < 9; index++)
    {
        MODEL_NODE *node = &object->nodes[index];
        object->node_table[index] = node;
        node->model_id = model_offsets[index] < 0 ? 0 : g_radar_vehicle_model_base + model_offsets[index];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(g_overlay_initial_pose_slot_40f0, object->node_table, 1);
    object->owner_effect = effect;
    object->collision.z = effect->z;
    object->collision.x = effect->x;
    object->collision.y = effect->y + 0x1000;
    object->collision.receives_mask = 1;
    object->collision.callback_18 = destructible_hierarchy_damage;
    object->health = 10;
    object->nodes[0].scale = 0x1000;
    object->field_0b2 = -1;
    object->field_0bc = 0x10;
    object->saved_node3_translation_z = object->nodes[3].translation_z;
    cell_x = div_pow2_trunc(object->collision.x, 14);
    cell_z = div_pow2_trunc(g_map_depth_cells * 3 * 0x4000 - object->collision.z, 14);
    group_index = g_map_floor_cells[cell_x + cell_z * g_map_width_cells].group_index;
    object->map_group = (MAP_MODEL_GROUP *)((uint8 *)g_map_model_groups + (uint32)group_index * sizeof(MAP_MODEL_GROUP));
}
