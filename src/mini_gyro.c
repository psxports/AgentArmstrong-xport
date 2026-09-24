#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "airship.h"
#include "code_module.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
#include "map.h"
#include "mini_gyro.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "runtime_heap.h"

/* Types. */
typedef struct
{
    const sint8 *model_offsets;
    uint8 count;
    uint8 pad[3];
    void **pose;
    sint16 *model_base;
    sint32 root_rotation_y, half_x, height, half_z;
} MINI_GYRO_DESCRIPTOR;

typedef struct
{
    COLLISION collision;                    /* +0x000 */
    sint16 health;                          /* +0x078 */
    sint16 flash_clut_ticks;                /* +0x07A */
    uint8 field_07c[0x36];                  /* +0x07C */
    sint16 field_0b2;                       /* +0x0B2 */
    uint8 field_0b4[0x10];                  /* +0x0B4 */
    MODEL_NODE nodes[8];                    /* +0x0C4 */
    uint8 field_3c4[0x240];                 /* +0x3C4 */
    MODEL_NODE *node_table[8];              /* +0x604 */
    uint8 field_624[0x1c];                  /* +0x624 */
    EFFECT *owner_effect;                   /* +0x640 */
    uint8 field_644[0x24];                  /* +0x644 */
    MAP_MODEL_GROUP *map_group;             /* +0x668 */
    uint8 field_66c[4];                     /* +0x66C */
    const MINI_GYRO_DESCRIPTOR *descriptor; /* +0x670 */
} MINI_GYRO;

#if defined(AP_32BIT)
    #define MINI_GYRO_OFFSET_ASSERT(field, offset) typedef char MiniGyroObject_##field##_at_##offset[(offsetof(MINI_GYRO, field) == 0x##offset) ? 1 : -1]
MINI_GYRO_OFFSET_ASSERT(nodes, 0c4);
MINI_GYRO_OFFSET_ASSERT(node_table, 604);
MINI_GYRO_OFFSET_ASSERT(owner_effect, 640);
MINI_GYRO_OFFSET_ASSERT(map_group, 668);
MINI_GYRO_OFFSET_ASSERT(descriptor, 670);
typedef char MiniGyroObject_size_674[(sizeof(MINI_GYRO) == 0x674) ? 1 : -1];
    #undef MINI_GYRO_OFFSET_ASSERT
#endif

/* Variables. */
static const sint8 common_offsets[5] = {0, -1, -1, -1, -1};

/* Functions. */
static const MINI_GYRO_DESCRIPTOR *descriptor_for(sint32 type)
{
    static MINI_GYRO_DESCRIPTOR descriptors[3];
    descriptors[0].model_offsets = common_offsets;
    descriptors[0].count = 5;
    descriptors[0].pose = &g_mini_gyro_initial_pose;
    descriptors[0].model_base = &g_mini_gyro_model_base;
    descriptors[0].root_rotation_y = 0x4b0;
    descriptors[0].half_x = 0x6c00;
    descriptors[0].height = 0x10000;
    descriptors[0].half_z = 0x3800;
    descriptors[1] = descriptors[0];
    descriptors[1].pose = &g_mini_gyro_type_117_initial_pose;
    descriptors[1].model_base = &g_mini_gyro_type_117_model_base;
    descriptors[1].root_rotation_y = 0;
    descriptors[1].half_x = 0x6400;
    descriptors[1].height = 0xaa00;
    descriptors[2] = descriptors[0];
    descriptors[2].model_offsets = (const sint8 *)win_code_module_address(0x800ff1e8u);
    descriptors[2].count = 8;
    descriptors[2].pose = &g_overlay_initial_pose_slot_4004;
    descriptors[2].model_base = &g_overlay_model_base_slot_4000;
    descriptors[2].root_rotation_y = 0x400;
    descriptors[2].height = 0x8000;
    if (type == 117)
        return &descriptors[1];
    if (type == 116)
        return &descriptors[2];
    return &descriptors[0];
}

/* Original: FUN_800AD6C0. */
void mini_gyro_update(MINI_GYRO *object)
{
    MODEL_NODE *root = &object->nodes[0];
    const MINI_GYRO_DESCRIPTOR *d = object->descriptor;
#ifdef XPORT_NATIVE
    {
        static sint32 seen;
        if (!seen && getenv("OA_STAGE21_TRACE") && object->collision.object_type == 117)
        {
            FILE *file = fopen("../status/take-a-bath-native.log", "a");
            if (file)
            {
                fprintf(file, "BATH_NATIVE_UPDATE type=117 xyz=%d,%d,%d health=%d\n", object->collision.x, object->collision.y, object->collision.z, object->health);
                fclose(file);
            }
            seen = 1;
        }
    }
#endif
    if (!world_object_is_visible(object->collision.x, object->collision.y, object->collision.z, 0x10000))
        return;
    player_push_out_of_box(object->collision.x, object->collision.y, object->collision.z, d->half_x, d->height, d->half_z);
    /* 800AD758/800AD780/800AD7A8 store object +0x10C/+0x110/+0x114:
     * these are the root node's +0x48/+0x4C/+0x50 translations, not its
     * derived +0x30/+0x34/+0x38 world coordinates. */
    root->translation_x = object->collision.x - g_camera_world_x;
    root->translation_y = object->collision.y - g_camera_world_y;
    if (object->collision.object_type == 116)
        root->translation_y += player_bob_offset_update(0);
    root->translation_z = object->collision.z - g_camera_world_z;
    g_model_render_frame = 0;
    model_render_begin();
    g_render_depth_bucket = 0x480 - ((g_render_row_world_z + 0x2000 - object->collision.z) / 0x100);
    g_current_model_world_z = object->collision.z;
    if (object->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --object->flash_clut_ticks;
    }
    model_render_node(root, (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
    model_render_end();
    hierarchy_collision_box_update(object, &object->nodes[1], 4);
    object->collision.depth = d->half_z;
    object->collision.box_z = ((d->half_z > 0) - d->half_z) >> 1;
}

/* Original: FUN_800AD484. */
void mini_gyro_create(EFFECT *effect)
{
    MINI_GYRO *object = (MINI_GYRO *)runtime_heap_allocate(0x674);
    const MINI_GYRO_DESCRIPTOR *d = descriptor_for(effect->type);
    sint32 i, cell_x, cell_z;
    object->collision.update = (FUNC_COLLISION_UPDATE)mini_gyro_update;
    linked_list_append(g_object_list, object);
    object->collision.object_type = (sint16)effect->type;
    object->descriptor = d;
    for (i = 0; i < d->count; i++)
    {
        MODEL_NODE *node = &object->nodes[i];
        object->node_table[i] = node;
        node->model_id = d->model_offsets[i] == -1 ? 0 : *d->model_base + d->model_offsets[i];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(*d->pose, object->node_table, 1);
    object->nodes[0].rotation_y = (sint16)d->root_rotation_y;
    effect->type = 0;
    object->owner_effect = effect;
    object->collision.z = effect->z;
    object->collision.x = effect->x;
    object->collision.y = effect->y - 0x3400;
    if (object->collision.object_type == 117)
        object->collision.y = effect->y - 0x2c00;
    object->collision.receives_mask = 1;
    object->collision.callback_18 = destructible_hierarchy_damage;
    object->health = 100;
    object->field_0b2 = -1;
    cell_x = div_16384_trunc(object->collision.x);
    cell_z = div_16384_trunc(g_map_depth_cells * 0xc000 - object->collision.z);
    object->map_group = (MAP_MODEL_GROUP *)((uint8 *)g_map_model_groups + (uint32)g_map_floor_cells[cell_x + cell_z * g_map_width_cells].group_index * sizeof(MAP_MODEL_GROUP));
#ifdef XPORT_NATIVE
    if (getenv("OA_STAGE21_TRACE"))
    {
        static sint32 count;
        FILE *file = fopen("../status/take-a-bath-native.log", count ? "a" : "w");
        if (file)
        {
            fprintf(file, "BATH_NATIVE_CTOR n=%d type=%d xyz=%d,%d,%d health=%d damage=800acb84 update=800ad6c0\n", count + 1, object->collision.object_type, object->collision.x, object->collision.y, object->collision.z, object->health);
            fclose(file);
        }
        ++count;
    }
#endif
}
