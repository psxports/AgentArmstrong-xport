#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "spatial.h"
#include "game_math.h"
#include "sprite.h"
#include "airship.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "camera.h"
#include "map.h"
#include "mini_gyro.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "runtime_heap.h"
#include "scuba_player.h"

/* Original: DAT_800D3E48. */
MODEL_POSE_RESOURCE g_mini_gyro_type_117_initial_pose;

/* Original: DAT_800D3E44. */
sint16 g_mini_gyro_type_117_model_base;

/* Original: DAT_800D3FB8. */
MODEL_POSE_RESOURCE g_mini_gyro_initial_pose;

/* Original: DAT_800D3FAC. */
sint16 g_mini_gyro_model_base;

/* Types. */
typedef struct
{
    const sint8 *model_offsets;
    uint8 count;
    const MODEL_POSE_RESOURCE *pose;
    const sint16 *model_base;
    sint32 root_rotation_y, half_x, height, half_z;
} MINI_GYRO_DESC;

typedef struct
{
    DESTRUCTIBLE_HIERARCHY hierarchy;
    MODEL_NODE nodes[8];
    MODEL_NODE *node_table[8];
    const MINI_GYRO_DESC *desc;
} MINI_GYRO;

/* Variables. */
static const sint8 common_offsets[5] = {0, -1, -1, -1, -1};

/* Original data: SLES_004.74:0x800CD9CC..0x800CDA2B */
static const MINI_GYRO_DESC mini_gyro_descs[3] = {
    {common_offsets, 5, &g_mini_gyro_initial_pose, &g_mini_gyro_model_base, 0x4b0, 0x6c00, 0x10000, 0x3800},
    {common_offsets, 5, &g_mini_gyro_type_117_initial_pose, &g_mini_gyro_type_117_model_base, 0, 0x6400, 0xaa00, 0x3800},
    {g_scuba_mini_gyro_model_offsets, 8, &g_mini_sub_initial_pose, &g_mini_sub_model_base, 0x400, 0x6c00, 0x8000, 0x3800},
};

/* Functions */
/* Original: FUN_800AD6C0. */
static void mini_gyro_update(void *raw_object)
{
    MINI_GYRO *object = (MINI_GYRO *)raw_object;
    MODEL_NODE *root = &object->nodes[0];
    const MINI_GYRO_DESC *d = object->desc;
    if (!world_object_is_visible(object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z, 0x10000))
        return;
    player_push_out_of_box(object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z, d->half_x, d->height, d->half_z);
    root->translation_x = ((uint32)object->hierarchy.col.x - g_camera.world_x);
    root->translation_y = ((uint32)object->hierarchy.col.y - g_camera.world_y);
    if (object->hierarchy.col.object_type == 116)
        root->translation_y = ((uint32)root->translation_y + player_bob_offset_update(0));
    root->translation_z = ((uint32)object->hierarchy.col.z - g_camera.world_z);
    g_model_draw.target_frame = 0;
    model_render_begin();
    g_model_draw.depth_bucket = 0x480 - div_pow2_trunc((g_map_render.depth_anchor_z + 0x2000u - object->hierarchy.col.z), 8);
    g_model_draw.world_z = object->hierarchy.col.z;
    if (object->hierarchy.col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --object->hierarchy.col.flash_clut_ticks;
    }
    model_render_node(root, &g_model_identity);
    g_model_draw.clut_override = 0;
    model_render_end();
    hierarchy_col_box_update(&object->hierarchy.col, &object->nodes[1], 4);
    object->hierarchy.col.depth = d->half_z;
    object->hierarchy.col.box_z = div_pow2_trunc(-d->half_z, 1);
}

/* Original: FUN_800AD484. */
void mini_gyro_create(EFFECT *effect)
{
    MINI_GYRO *object = (MINI_GYRO *)runtime_heap_allocate(sizeof(*object));
    const MINI_GYRO_DESC *d = &mini_gyro_descs[effect->type == 117 ? 1 : effect->type == 116 ? 2 : 0];
    sint32 i;
    object->hierarchy.col.object.update = mini_gyro_update;
    linked_list_append(&g_object_list, object);
    object->hierarchy.col.object_type = effect->type;
    object->desc = d;
    for (i = 0; i < d->count; i++)
    {
        MODEL_NODE *node = &object->nodes[i];
        object->node_table[i] = node;
        node->model_id = d->model_offsets[i] == -1 ? 0 : *d->model_base + d->model_offsets[i];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(d->pose, object->node_table, d->count, 1);
    object->nodes[0].rotation_y = d->root_rotation_y;
    effect->type = 0;
    object->hierarchy.owner_effect = effect;
    object->hierarchy.col.z = effect->z;
    object->hierarchy.col.x = effect->x;
    object->hierarchy.col.y = effect->y - 0x3400u;
    if (object->hierarchy.col.object_type == 117)
        object->hierarchy.col.y = effect->y - 0x2c00u;
    object->hierarchy.col.receives_mask = 1;
    object->hierarchy.col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->hierarchy.col.hit_callback = destructible_hierarchy_damage;
    object->hierarchy.col.hit.health = 100;
    object->hierarchy.map_rec = map_floor_cell_at(object->hierarchy.col.x, object->hierarchy.col.z)->grp;
}
