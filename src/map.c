#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_math.h"
#include "native_config.h"
#include "game.h"
#include "render.h"
#include "game_sound.h"
#include "spatial.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "camera.h"
#include "mission.h"
#include "level_data.h"
#include "map.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "random.h"
#include "runtime_heap.h"
#include "scuba_player.h"
#include "sprite.h"

/* Originals: DAT_800D3F24, DAT_800D3F8C, DAT_800D40D8, DAT_800D4160 */
/* Originals: DAT_800D3F64, DAT_800D40DC, DAT_800D41B8, DAT_800D3FA8, DAT_800D3F74 */
MAP_STATE g_map;

/* Types. */
/* Fixed byte-only records are borrowed from the archive, never game state */
typedef struct MAP_FLOOR_FILE_RECORD
{
    uint8 grp[2], height[2];
    uint8 material, shade, packed_orientation, tile_flags, height_step;
    uint8 opaque_tail;
} MAP_FLOOR_FILE_RECORD;

typedef struct MAP_MODEL_FILE_RECORD
{
    uint8 model_id[2], y[2], local_x[2], local_z[2], yaw[2];
    uint8 light, opaque_tail;
} MAP_MODEL_FILE_RECORD;

typedef char MAP_FLOOR_FILE_SIZE_10[sizeof(MAP_FLOOR_FILE_RECORD) == 10 ? 1 : -1];
typedef char MAP_MODEL_FILE_SIZE_12[sizeof(MAP_MODEL_FILE_RECORD) == 12 ? 1 : -1];

/* Moving map instances share native state with the map owner */

typedef struct
{
    COLLISION col;                /* Legacy +0x00 */
    MAP_MODEL_INSTANCE *instance; /* Legacy +0x80 */
    sint32 velocity_y;            /* Legacy +0x84 */
    sint32 y_offset;              /* Legacy +0x88 */
    sint16 grounded;              /* Legacy +0x8C */
    sint16 bounce_count;          /* Legacy +0x8E */
} MAP_RUNTIME;

/* Release native decoded owners before the stage heap is replaced */
void map_heap_reset(void)
{
    free(g_map.floor);
    free(g_map.grps);
    free(g_map.attributes);
    g_map.floor = 0;
    g_map.grps = 0;
    g_map.attributes = 0;
    g_map.floor_count = g_map.rec_capacity = g_map.attribute_count = 0;
    g_map.light.vals = 0;
    g_map.light.count = 0;
}

/* Functions. */
/* Original: SLES_004.74:FUN_80095E64. */
static void map_model_grp_sort_by_y(MAP_MODEL_GROUP *grp)
{
    sint32 swapped;
    if (grp == 0 || grp == g_map.grps || grp->active_count < 2)
        return;
    do
    {
        sint32 i;
        swapped = 0;
        for (i = 0; i + 1 < grp->active_count; ++i)
        {
            MAP_MODEL_INSTANCE *a = (grp + 1) + i;
            MAP_MODEL_INSTANCE *b = a + 1;
            if (b->y < a->y)
            {
                MAP_MODEL_INSTANCE temp = *b;
                *b = *a;
                *a = temp;
                swapped = 1;
            }
        }
    } while (swapped);
}

/* Original: FUN_8009557C. */
MAP_FLOOR_CELL *map_floor_cell_at(sint32 world_x, sint32 world_z)
{
    sint32 cell_x;
    uint32 cell_z;
    if (world_x < 0)
        world_x += 0x3fff;
    world_z = (g_map.depth * 0xc000u - world_z);
    if (world_z < 0)
        world_z += 0x3fff;
    cell_x = world_x >> 14;
    cell_z = world_z >> 14;
    sint32 idx = cell_z * g_map.width + cell_x;
    XASSERT(idx < g_map.floor_count);
    return g_map.floor + idx;
}

/* The private floor query supplies signed16 corners and fixed 0x4000 extents */
/* Original: FUN_80096CAC. */
static sint32 map_height_interpolate(sint32 h00, sint32 h01, sint32 h10, sint32 h11, sint32 extent_x, sint32 extent_z, sint32 local_x, sint32 local_z)
{
    sint32 top_left = h00 + 0x400;
    sint32 top_right = h01 + 0x400;
    sint32 bottom_left = h10 + 0x400;
    sint32 bottom_right = h11 + 0x400;
    sint32 top_sum = top_left + top_right;
    sint32 total = top_sum + bottom_left + bottom_right;
    sint32 half_z = extent_z / 2;
    sint32 half_x = extent_x / 2;
    sint32 z_centered = local_z - half_z;
    sint32 x_centered = local_x - half_x;
    sint32 avg = total / 4;
    sint32 along_z;
    sint32 along_x;
    sint32 result;

    if (avg == 0)
        avg = 1;
    if (half_z == 0)
        half_z = 1;

    along_z = mips_mult_lo_div(top_sum, half_z + z_centered, half_z);
    along_z += mips_mult_lo_div(bottom_left + bottom_right, half_z - z_centered, half_z);
    along_z /= 4;

    along_x = mips_mult_lo_div(top_left + bottom_left, half_x + x_centered, half_x);
    along_x += mips_mult_lo_div(top_right + bottom_right, half_x - x_centered, half_x);
    along_x /= 4;

    result = mul_lo(along_x, along_z);
    if (avg == -1 && result == INT32_MIN)
    {
#if defined(_DEBUG)
        fputs("MAP HEIGHT INTERPOLATION BREAK 6\n", stderr);
#endif
        abort();
    }
    result = ((result / avg) - 0x400u);
    return ((uint32)result << 8);
}

/* Original: FUN_80096B94. */
sint32 map_floor_height_at(sint32 world_x, sint32 cur_y, sint32 world_z)
{
    MAP_FLOOR_CELL *cell = map_floor_cell_at(world_x, world_z);
    sint32 map_z, local_x, local_z;
    sint16 h00, h01, h10, h11;
    if (cell->material == 0xff)
        return (cur_y + 0x800u);

    map_z = (g_map.depth * 0xc000u - world_z);
    local_x = 0x4000 - (world_x - (world_x / 0x4000) * 0x4000);
    local_z = 0x4000 - (map_z - (map_z / 0x4000) * 0x4000);
    sint32 idx = cell - g_map.floor;
    XASSERT(!(g_map.width < 0 || g_map.width >= g_map.floor_count - idx - 1));
    h00 = cell->height;
    h01 = (cell + 1)->height;
    h10 = (cell + g_map.width)->height;
    h11 = (cell + g_map.width + 1)->height;
    sint32 height = map_height_interpolate(h00, h01, h10, h11, 0x4000, 0x4000, local_x, local_z);

    /* 0x80096C68..0x80096C84: publish the material/surface value for
     * the cell from the runtime table built by FUN_800AAEC8. */
    g_col_surface_resource = g_stage_env->material_lookup[cell->material];
    return height;
}

/* Original: FUN_80095FA0. */
void map_runtime_initialize(sint32 width, sint32 height)
{
    g_map.width = width;
    g_map.depth_subcells = ((uint32)height << 6);
    g_map.depth = height;
    g_map.width_subcells = ((uint32)width << 6);
}

/* Original lookup scans attribute-zero terminated pairs in the owning archive */
/* Original: FUN_80095604. */
sint32 model_attribute_find(sint16 model_id)
{
    sint32 idx;
    for (idx = 0; idx < g_map.attribute_count; ++idx)
    {
        const MAP_MODEL_ATTRIBUTE *entry = &g_map.attributes[idx];
        if (entry->model_id == model_id)
            return entry->attribute;
    }
    return 0;
}

/* Original: SLES_004.74:FUN_800A6D64. */
static void map_runtime_object_update(void *raw_object)
{
    MAP_RUNTIME *object = (MAP_RUNTIME *)raw_object;
    uint32 old_x = object->col.x;
    uint32 old_y = object->col.y;
    sint32 old_z = object->col.z;
    sint32 depth = (uint32)old_z - g_camera.world_z;
    sint32 sx, sy, numerator;
    sint32 horizontal_limit;
    sint32 floor_y;
    COLLISION_RESULT *result;

    if (old_z == g_camera.world_z)
        return;
    numerator = ((old_x - g_camera.world_x) * 0x140u);
    if (depth == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("MOVING MAP X PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    sx = numerator / depth;
    horizontal_limit = (g_render_state.half_width + 0x80u);
    if (sx > horizontal_limit || sx < (-horizontal_limit))
        return;
    numerator = ((old_y - g_camera.world_y) * 0x163u);
    if (depth == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("MOVING MAP Y PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    sy = numerator / depth;
    if (sy + 0xc8u > 0x190u)
        return;

    object->velocity_y += 0x80;
    if (object->col.object_type == 0x5d || object->col.object_type == 0x93)
        object->velocity_y = 0;
    if (object->velocity_y > 0x500)
        object->velocity_y = 0x500;
    object->col.y = (uint32)object->col.y + object->velocity_y;

    result = static_map_col_resolve(old_x, (old_y + object->y_offset), old_z, object->col.x, ((uint32)object->col.y + object->y_offset), object->col.z, 0x30, 0x30, 0x40);
    floor_y = map_floor_height_at(object->col.x, ((uint32)object->col.y + object->y_offset), object->col.z);
    if (floor_y < (object->col.y + object->y_offset))
    {
        result->y = floor_y;
        result->flags |= 1;
    }
    if (result->flags & 1)
    {
        if (object->bounce_count == 0)
        {
            sint32 velocity = -object->velocity_y;
            object->bounce_count = 1;
            object->velocity_y = velocity / 2;
        }
        else
            object->velocity_y = 0;
        object->col.x = result->x;
        object->col.y = (uint32)result->y - object->y_offset;
        object->col.z = result->z;
        object->grounded = 1;
    }
    else
        object->grounded = 0;
    object->instance->y = div_256_trunc(object->col.y);
}

/* Original: SLES_004.74:FUN_800A713C. */
static void map_runtime_object_damage(void *raw_object, void *raw_src)
{
    MAP_RUNTIME *object = (MAP_RUNTIME *)raw_object;
    COLLISION *src = (COLLISION *)raw_src;
    sint16 type = object->col.object_type;
    if (type == 0x94)
    {
        if (src->object_type != 2 || src->hit.damage != 10)
            return;
    }
    if (object_damage_apply(&object->col, src) == 0)
    {
        if (object->grounded == 0)
            return;
        object->velocity_y = -0x200;
        object->grounded = 0;
        return;
    }

    object->instance->y = 0x4e20;
    object_destroy(object);
    speech_random_request(9);
    type = object->col.object_type;
    if (type == 0x24 || type == 0x90 || type == 0x93)
        g_mission.primary_targets_done = (g_mission.primary_targets_done + 1u);
    if (type == 0x48)
        g_mission.secondary_targets_done = (g_mission.secondary_targets_done + 1u);

    if (type == 0x48 || type == 0x90 || type == 0x93 || type == 0x94)
    {
        SPRITE *effect;
        sint32 kind = type == 0x94 ? 10 : 1;
        object->col.y = object->col.y + 0x2000u;
        effect = damage_expl_create(&object->col, kind);
        col_box_set(effect, 0xb4, 0xb4, 0xb4);
        object->col.y = object->col.y - 0x2000u;
        sound_play_positional(10, 0, 0x96, object->col.x, object->col.y, object->col.z);
        return;
    }
    if (g_session.scuba_active == 0)
    {
        sound_play_positional(0x11, 0, 0x7f, object->col.x, object->col.y, object->col.z);
        sound_play_positional(9, -2, 0x64, object->col.x, object->col.y, object->col.z);
        timed_callback_create(expl_debris_particle_create, object->col.x, (object->col.y + 0x2000u), object->col.z, 12, 2);
    }
    else
    {
        timed_callback_create(scuba_death_particle_emit, object->col.x, object->col.y, object->col.z, 0x14, 2);
    }
}

/* Original: SLES_004.74:FUN_800A6BB8. */
static MAP_RUNTIME *map_runtime_object_create(sint32 x, sint32 y, sint32 z, MAP_MODEL_INSTANCE *instance, sint16 attribute)
{
    MAP_RUNTIME *object = (MAP_RUNTIME *)object_create(sizeof(*object), map_runtime_object_update);
    XASSERT((instance->model_id) < g_model_catalog.count);
    sint32 half_y = g_model_catalog.descs[instance->model_id].half_y;
    object->instance = instance;
    object->col.object_type = attribute;
    object->col.x = x;
    object->col.z = z;
    object->y_offset = (-half_y) << 8;
    object->col.y = (uint32)y - object->y_offset;
    if (attribute != 0x26)
    {
        sint32 col_height = object->y_offset;
        object->col.receives_mask = 0x11;
        object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
        object->col.hit_callback = map_runtime_object_damage;
        object->col.hit.health = 6;
        if (attribute == 0x48 || attribute == 0x90 || attribute == 0x93)
            object->col.hit.health = 0x19;
        if (col_height < 0)
            col_height += 0x7f;
        col_box_set(object, 0x46, col_height >> 7, 0x46);
        object->col.box_y = ((uint32)object->col.box_y + object->y_offset);
        if (attribute == 0x24 || attribute == 0x90)
            g_mission.primary_targets_total = (g_mission.primary_targets_total + 1u);
        if (attribute == 0x48)
            g_mission.secondary_targets_total = (g_mission.secondary_targets_total + 1u);
    }
    return object;
}

/* Original: SLES_004.74:FUN_80095664. */
void map_grid_initialize(const MAP_INPUT *input)
{
    uint32 logical_cells;
    uint32 cell_idx;
    sint32 cell_x = 0;
    uint32 row_z;
    MAP_FLOOR_CELL *floor;
    MAP_FLOOR_CELL *decoded_floor;
    MAP_MODEL_GROUP *decoded_grps;
    MAP_MODEL_ATTRIBUTE *decoded_attributes = 0;
    uint32 floor_count, rec_count, attribute_count = 0, idx;
    DYNAMIC_COLLISION_ANCHOR *col_anchors;
    const STAGE_ENV *stage_entry = g_stage_env;
    const sint32 *material_lookup;
    sint32 special_material;

    XASSERT(!(g_map.width <= 0 || g_map.depth <= 0 || g_map.depth > 0x7ffffffa || g_map.width > INT32_MAX / (g_map.depth + 5)));
    XASSERT(!(stage_entry == 0 || stage_entry->material_lookup == 0));
    material_lookup = stage_entry->material_lookup;
    special_material = stage_entry->map_resource;
    logical_cells = g_map.width * g_map.depth;
    XASSERT(!(input == 0 || input->floor.data == 0 || input->grps.data == 0 || input->attribute_lookup.data == 0 || input->grps.size < 12 || input->grps.size % 12 != 0));
    floor_count = input->floor.size / 10;
    rec_count = input->grps.size / 12;
    XASSERT(floor_count > logical_cells);
    decoded_floor = (MAP_FLOOR_CELL *)calloc(floor_count, sizeof(*decoded_floor));
    decoded_grps = (MAP_MODEL_GROUP *)calloc(rec_count, sizeof(*decoded_grps));
    XASSERT(!(decoded_floor == 0 || decoded_grps == 0));
    for (idx = 0; idx < rec_count; ++idx)
    {
        const MAP_MODEL_FILE_RECORD *entry = (const MAP_MODEL_FILE_RECORD *)(input->grps.data + idx * 12);
        MAP_MODEL_GROUP *rec = decoded_grps + idx;
        rec->model_id = xport_load_le16(entry->model_id);
        rec->y = xport_load_le16(entry->y);
        rec->local_x = xport_load_le16(entry->local_x);
        rec->local_z = xport_load_le16(entry->local_z);
        rec->yaw = xport_load_le16(entry->yaw);
        rec->light = entry->light;
    }
    for (idx = 0; idx < floor_count; ++idx)
    {
        const MAP_FLOOR_FILE_RECORD *entry = (const MAP_FLOOR_FILE_RECORD *)(input->floor.data + idx * 10);
        MAP_FLOOR_CELL *cell = decoded_floor + idx;
        uint32 grp_idx = xport_load_le16(entry->grp);
        MAP_MODEL_GROUP *grp;
        uint32 count;
        XASSERT(grp_idx < rec_count);
        grp = decoded_grps + grp_idx;
        count = grp->active_count + grp->removed_count;
        XASSERT(!(grp_idx != 0 && (grp->active_count < 0 || grp->removed_count < 0 || count > rec_count - grp_idx - 1)));
        cell->grp = grp;
        cell->height = xport_load_le16(entry->height);
        XASSERT(!(entry->material != 0xff && entry->material >= sizeof(g_material_resource_table) / sizeof(g_material_resource_table[0])));
        cell->material = entry->material;
        cell->shade = entry->shade;
        cell->packed_orientation = entry->packed_orientation;
        cell->tile_flags = entry->tile_flags;
        cell->height_step = entry->height_step;
    }
    for (;;)
    {
        uint32 offset;
        XASSERT(attribute_count < input->attribute_lookup.size / 4);
        offset = attribute_count * 4;
        if (xport_load_le16(input->attribute_lookup.data + offset + 2) == 0)
            break;
        ++attribute_count;
    }
    if (attribute_count != 0)
    {
        decoded_attributes = (MAP_MODEL_ATTRIBUTE *)calloc(attribute_count, sizeof(*decoded_attributes));
        XASSERT(!(decoded_attributes == 0));
        for (idx = 0; idx < attribute_count; ++idx)
        {
            decoded_attributes[idx].model_id = xport_load_le16(input->attribute_lookup.data + idx * 4);
            decoded_attributes[idx].attribute = xport_load_le16(input->attribute_lookup.data + idx * 4 + 2);
        }
    }
    free(g_map.floor);
    free(g_map.grps);
    free(g_map.attributes);
    g_map.floor = decoded_floor;
    g_map.grps = decoded_grps;
    g_map.attributes = decoded_attributes;
    g_map.floor_count = floor_count;
    g_map.rec_capacity = rec_count;
    g_map.attribute_count = attribute_count;
    floor = decoded_floor;
    XASSERT(!(logical_cells > 0x7fffffffu / sizeof(*col_anchors)));
    col_anchors = (DYNAMIC_COLLISION_ANCHOR *)runtime_heap_allocate((logical_cells * sizeof(*col_anchors)));
    g_dynamic_col_grid.cells = col_anchors;
    g_dynamic_col_grid.count = logical_cells;
    XASSERT(!(logical_cells > 0x7fffffffu / sizeof(*g_spatial_cell_buckets)));
    g_spatial_cell_buckets = (SPATIAL_BUCKET **)runtime_heap_allocate((logical_cells * sizeof(*g_spatial_cell_buckets)));
    g_spatial_cell_count = logical_cells;
    g_map.light.count = g_map.width * (g_map.depth + 5u);
    g_map.light.vals = (uint8 *)runtime_heap_allocate(g_map.light.count);
    row_z = (g_map.depth * 0xc000u - 0x4000);

    for (cell_idx = 0; cell_idx < logical_cells; cell_idx++)
    {
        MAP_FLOOR_CELL *cell = floor + cell_idx;
        MAP_MODEL_GROUP *grp = cell->grp;
        sint32 model_count;
        sint32 model_idx;
        sint32 removed_count = 0;
        uint8 packed_orientation;

        g_map.light.vals[cell_idx] = 0;
        if (cell_x == g_map.width)
        {
            cell_x = 0;
            row_z = (row_z - 0x4000u);
        }

        packed_orientation = cell->packed_orientation;
        cell->packed_orientation = ((((packed_orientation & 3) * 2) + 2) & 7);
        model_count = grp == g_map.grps ? 0 : grp->active_count;
        if (model_count > 0x80)
            fatal_error_with_val("MODEL COUNT STOPPROG", model_count);

        sint32 material = cell->material == 0xff ? *g_void_floor_material : material_lookup[cell->material];
        if (special_material != 0 && special_material == material)
        {
            if ((cell->tile_flags & 0x0f) == 0)
            {
                cell->height_step = -1;
                (cell + 1)->height_step = -1;
                cell->height = (cell->height - random_range(g_map.terrain_variation));
                (cell + 1)->height = ((cell + 1)->height - random_range(g_map.terrain_variation));
            }
        }
        if (special_material == material && (cell->tile_flags & 0x0f) != 0)
        {
            cell->tile_flags &= 0xf0;
            cell->height_step = 0;
        }
        if (packed_orientation & 4)
            cell->tile_flags |= 0x40;
        if (packed_orientation & 8)
            cell->tile_flags |= 0x80;

        for (model_idx = 0; model_idx < model_count; ++model_idx)
        {
            MAP_MODEL_INSTANCE *instance = (grp + 1) + model_idx;
            sint32 attribute;
            instance->local_x = ((uint16)instance->local_x << 8);
            instance->local_z = ((uint16)instance->local_z << 8);
            instance->yaw = ((instance->yaw & 0x03ff) << 2);
            attribute = model_attribute_find(instance->model_id);
            if (attribute == 0x2d || attribute == 0x2e)
            {
                XASSERT((instance->model_id) < g_model_catalog.count);
                sint32 trigger_height = (-g_model_catalog.descs[instance->model_id].half_y) << 9;
                player_detect_ledge(((cell_x << 14) + instance->local_x), (instance->y * 256 + trigger_height / 2), (row_z + 0x2000u), 0x2800, trigger_height);
                instance->local_z = (instance->local_z + 0x2000);
                if (attribute == 0x2d)
                {
                    instance->y = (instance->y + 5000);
                    grp->removed_count = (grp->removed_count + 1);
                }
                else
                    instance->y = 6000;
                ++removed_count;
            }
            else if (attribute == 0x4d)
            {
                instance->local_z = (instance->local_z + 0x2000);
                instance->y = (instance->y + 5000);
                grp->removed_count = (grp->removed_count + 1);
                ++removed_count;
            }
            else if (attribute == 0x25 || attribute == 0x95 || attribute == 0x63)
            {
                if (attribute == 0x25 || attribute == 0x95)
                {
                    XASSERT((instance->model_id) < g_model_catalog.count);
                    MODEL_DESC *desc = &g_model_catalog.descs[instance->model_id];
                    DYNAMIC_COLLISION_NODE *node;
                    DYNAMIC_COLLISION_ANCHOR *anchor = col_anchors + cell_idx;
                    instance->y = (instance->y + 5000);
                    grp->removed_count = (grp->removed_count + 1);
                    node = (DYNAMIC_COLLISION_NODE *)runtime_heap_allocate(sizeof(*node));
                    node->prev = anchor->tail;
                    node->next = 0;
                    if (anchor->tail != 0)
                        anchor->tail->next = node;
                    else
                        anchor->head = node;
                    anchor->tail = node;
                    node->attribute = attribute;
                    node->min_x = ((cell_x << 14) + instance->local_x - (desc->half_x << 8));
                    node->max_x = (node->min_x + (desc->half_x << 9));
                    node->max_z = (row_z + instance->local_z + 0x600);
                    node->min_z = (row_z + instance->local_z - 0x1e00);
                    node->top_y = (desc->half_y + instance->y - 0x1380) * 256;
                    node->bottom_y = (instance->y - desc->half_y - 0x1390) * 256;
                }
                if (attribute == 0x63)
                {
                    instance->y = (instance->y + 5000);
                    grp->removed_count = (grp->removed_count + 1);
                }
                ++removed_count;
            }
        }
        map_model_grp_sort_by_y(grp);
        grp->active_count = (grp->active_count - removed_count);

        model_count = grp == g_map.grps ? 0 : grp->active_count;
        for (model_idx = 0; model_idx < model_count; model_idx++)
        {
            MAP_MODEL_INSTANCE *instance = (grp + 1) + model_idx;
            sint32 attribute = model_attribute_find(instance->model_id);
            if (attribute == 0x39)
                g_model_selections.falling_hazard = instance->model_id;
            if (attribute == 0x24 || attribute == 0x26 || attribute == 0x48 || attribute == 0x5d || attribute == 0x90 || attribute == 0x93 || attribute == 0x94)
            {
                XASSERT((instance->model_id) < g_model_catalog.count);
                map_runtime_object_create(((cell_x << 14) + instance->local_x), (instance->y - g_model_catalog.descs[instance->model_id].half_y) * 256, (row_z + instance->local_z), instance, attribute);
            }
            if (attribute == 0x2f)
            {
                XASSERT((instance->model_id) < g_model_catalog.count);
                g_model_catalog.descs[instance->model_id].special = 1;
            }
            if (attribute == 0x71)
            {
                scuba_model_target_create(((cell_x << 14) + instance->local_x), instance->y * 256, row_z, instance->model_id);
                instance->y = 6000;
            }
            if (attribute == 0x7d)
            {
                g_model_selections.bio_goo = instance->model_id;
                instance->y = 6000;
            }
            if (attribute == 0x80)
            {
                g_model_selections.robot_press = instance->model_id;
                instance->y = 6000;
            }
        }
        cell_x++;
    }
}

/* Original: SLES_004.74:FUN_80095190. */
void map_light_stamp_apply(sint32 world_x, sint32 world_z, sint16 clear, const MAP_STAMP *stamp)
{
    sint32 x, z;
    sint32 base_x; uint32 base_z;
    uint32 row_idx;
    sint32 random_offset;
    sint32 width = stamp->width;
    sint32 height = stamp->height;
    const sint16 *src = stamp->vals;

    if (world_x < 0)
        world_x = (world_x + 0x3fffu);
    world_z = (g_map.depth * 0xc000u - world_z);
    if (world_z < 0)
        world_z = (world_z + 0x3fffu);
    base_x = (world_x >> 14) + stamp->cell_x;
    base_z = (world_z >> 14) + stamp->cell_z;
    row_idx = (base_z * g_map.width + base_x);
    random_offset = random_range(0x30);

    for (z = 0; z < height; ++z)
    {
        for (x = 0; x < width; ++x)
        {
            uint32 idx = row_idx + x;
            uint8 *dst;
            XASSERT(!(g_map.light.vals == 0 || idx >= g_map.light.count));
            dst = g_map.light.vals + idx;
            if (clear == 0x80)
                *dst = 0;
            else
            {
                sint16 val = *src;
                if (val != -99)
                    *dst = val + random_offset;
            }
            ++src;
        }
        /* A nonpositive signed width skips pixels but retains row subtraction */
        row_idx = row_idx + g_map.width - (width < 0 ? width : 0);
    }
}
