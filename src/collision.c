#include <stdlib.h>

#include "game_math.h"
#include "object.h"
#include "render.h"
#include "collision.h"
#include "map.h"
#include "model.h"

/* Original: DAT_800D3E70 */
DYNAMIC_COLLISION_GRID g_dynamic_col_grid;
/* Original: DAT_800D3FF4 */
sint32 g_col_surface_resource;

/* Preserve persistent collision results and surface publications */
void col_heap_reset(void)
{
    g_dynamic_col_grid.cells = 0;
    g_dynamic_col_grid.count = 0;
}

/* Persistent workspaces retain original cross-call selection and aliasing */
/* Originals: DAT_800D4D30, DAT_800D4D44, DAT_800E7F80, DAT_800E8000 */
enum
{
    COLLISION_CELL_CAPACITY = 32
};

static COLLISION_RESULT dynamic_result;
static COLLISION_RESULT static_result;
static sint32 touched_rows[COLLISION_CELL_CAPACITY];
static sint32 touched_columns[COLLISION_CELL_CAPACITY];

/* Original: DAT_800D411C */
COLLISION_RESULT *g_cur_col_result;
/* Originals: DAT_800D4058, DAT_800D3E88 */
sint32 g_col_flags;
sint32 g_dynamic_col_hit;

/* 0x80096EA0..0x80097128: moving/runtime collision lists built in the
 * eight-byte per-cell DAT_800D3E70 table. */
/* Original: FUN_80096EA0. */
static COLLISION_RESULT *runtime_object_col_resolve(sint32 old_z, sint32 new_x, sint32 new_y, sint32 new_z, sint32 width, sint32 height)
{
    sint32 min_x = (uint32)new_x - (width / 2);
    sint32 max_x = (uint32)min_x + width;
    sint32 map_z = g_map.depth * 0xc000u - new_z;
    sint32 cell_x = min_x < 0 ? (min_x + 0x3fff) >> 14 : min_x >> 14;
    uint32 cell_z = map_z < 0 ? (map_z + 0x3fff) >> 14 : map_z >> 14;
    sint32 start = cell_x + cell_z * g_map.width - 1u - g_map.width;
    sint32 row = 0, visited = 0;
    sint32 cell_idx = start;
    sint32 cell_count = g_map.width * g_map.depth;
    dynamic_result.x = new_x;
    dynamic_result.y = new_y;
    dynamic_result.z = new_z;
    dynamic_result.flags = 0;
    g_dynamic_col_hit = 0;
    if (g_dynamic_col_grid.cells == 0 || cell_idx < 0)
        return &dynamic_result;
    while (cell_idx >= 0 && cell_idx < cell_count && cell_idx < g_dynamic_col_grid.count && visited < 9)
    {
        DYNAMIC_COLLISION_NODE *object = g_dynamic_col_grid.cells[cell_idx].head;
        while (object != 0)
        {
            sint32 object_min_x = object->min_x;
            sint32 object_max_x = object->max_x;
            sint32 object_min_z = object->min_z;
            sint32 object_max_z = object->max_z;
            sint32 object_top = object->top_y;
            sint32 object_bottom = object->bottom_y;
            if (object_min_x <= max_x && min_x <= object_max_x && object_min_z <= new_z && new_z <= object_max_z && object_top <= new_y && (new_y - height) <= object_bottom)
            {
                sint32 plane = object_min_z;
                if (object->attribute == 0x25)
                    plane = object_max_z;
                dynamic_result.z = plane <= old_z ? object_max_z : object_min_z;
                dynamic_result.flags = 4;
                g_dynamic_col_hit = 1;
                return &dynamic_result;
            }
            object = object->next;
        }
        ++row;
        ++cell_idx;
        if (row >= 3)
        {
            row = 0;
            cell_idx += g_map.width - 3;
        }
        ++visited;
    }
    return &dynamic_result;
}

/* Original: SLES_004.74:FUN_80097128. */
COLLISION_RESULT *static_map_col_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 new_x, sint32 new_y, sint32 new_z, sint32 width, sint32 height, sint32 depth)
{
    sint32 ox = div_256_trunc(old_x);
    sint32 oy = div_256_trunc(old_y);
    sint32 oz = g_map.depth * 0xc0u - div_256_trunc(old_z);
    sint32 nx = div_256_trunc(new_x);
    sint32 dst_y = div_256_trunc(new_y);
    sint32 ny = dst_y;
    sint32 rising = dst_y < oy;
    sint32 nz = g_map.depth * 0xc0u - div_256_trunc(new_z);
    const uint32 half_width = width / 2;
    sint32 min_x = nx - half_width;
    const uint32 half_depth = depth / 2;
    sint32 min_z = nz - half_depth;
    uint32 floor_count = 0;
    sint32 cx, cz;
    sint32 cur_x = ox, cur_z = oz;
    sint32 walk_min_x = cur_x - half_width;
    sint32 walk_max_x = cur_x + half_width;
    sint32 walk_min_z = cur_z - half_depth;
    sint32 walk_max_z = cur_z + half_depth;
    sint32 x_blocked = 0, z_blocked = 0, alternate = 0;

    static_result.x = new_x;
    static_result.y = new_y;
    static_result.z = new_z;
    static_result.flags = 0;
    static_result.step = 0;

    /* Each cell compares against the original destination Y and direction */
    {
        sint32 first_z = div_64_trunc(min_z);
        sint32 z_count = div_64_trunc(((min_z & 0x3fu) + depth + 0x3f));
        sint32 first_x = div_64_trunc(min_x);
        sint32 x_count = div_64_trunc(((min_x & 0x3fu) + width + 0x3f));
        sint32 zi, xi;
        for (zi = 0, cz = first_z; zi < z_count; ++zi, ++cz)
        {
            for (xi = 0, cx = first_x; xi < x_count; ++xi, ++cx)
            {
                sint32 cell_idx = cz*g_map.width + cx;
                XASSERT(cell_idx < g_map.floor_count);
                MAP_MODEL_GROUP *grp = g_map.floor[cell_idx].grp;
                sint32 count = (grp == g_map.grps ? 0 : grp->active_count);
                if (count > 0)
                {
                    sint32 grp_idx = grp - g_map.grps;
                    XASSERT(!(grp_idx >= g_map.rec_capacity || count > g_map.rec_capacity - grp_idx - 1));
                }
                sint32 i;
                if (count != 0)
                    for (i = 0; i < count; ++i)
                    {
                        MAP_MODEL_INSTANCE *instance = (grp + 1) + i;
                        sint32 id = instance->model_id;
                        MODEL_DESC *model;
                        sint32 plane;
                        XASSERT(id < g_model_catalog.count);
                        model = &g_model_catalog.descs[id];
                        if (model->col != 1)
                        {
                            continue;
                        }
                        if (rising)
                        {
                            plane = ((uint32)instance->y - model->half_y + height + 1);
                            if (dst_y <= plane && plane <= oy)
                            {
                                static_result.flags |= 8;
                                static_result.y = plane * 256;
                                XASSERT(floor_count < COLLISION_CELL_CAPACITY);
                                touched_rows[floor_count] = cz;
                                touched_columns[floor_count] = cx;
                                ++floor_count;
                                ny = plane;
                                break;
                            }
                        }
                        else
                        {
                            sint32 bottom = instance->y + model->half_y;
                            plane = bottom - 1;
                            if (!(dst_y + grp->y_offset < plane || bottom + 7 < oy + grp->y_offset))
                            {
                                static_result.flags |= 1;
                                static_result.y = plane * 256;
                                XASSERT(floor_count < COLLISION_CELL_CAPACITY);
                                touched_rows[floor_count] = cz;
                                touched_columns[floor_count] = cx;
                                ++floor_count;
                                ny = plane;
                                break;
                            }
                        }
                    }
            }
        }
    }

    /* 0x800974E4..0x80097AC4: advance one model unit at a time, alternating
     * Z and X.  Only a footprint edge exactly crossing a 64-unit boundary
     * causes the newly entered row/column to be tested. */
    const sint32 height_word = height;
    for (;;)
    {
        sint32 arrived = cur_x == nx && cur_z == nz;
        if (!x_blocked)
        {
            walk_min_x = (cur_x - half_width);
            walk_max_x = (cur_x + half_width);
        }
        if (!z_blocked)
        {
            walk_min_z = (cur_z - half_depth);
            walk_max_z = (cur_z + half_depth);
        }
        if (!z_blocked && nz != oz)
        {
            sint32 edge = nz < oz ? (walk_min_z - 1u) : walk_max_z;
            sint32 crossed = nz < oz ? ((edge & 0x3f) == 0x3f) : ((edge & 0x3f) == 0);
            if (crossed)
            {
                sint32 row = div_64_trunc(edge);
                sint32 first = div_64_trunc(walk_min_x);
                sint32 count_x = div_64_trunc(((walk_min_x & 0x3fu) + width + 0x3f));
                sint32 xi;
                for (xi = 0, cx = first; xi < count_x; ++xi, ++cx)
                {
                    sint32 cell_idx = row*g_map.width + cx;
                    XASSERT(cell_idx < g_map.floor_count);
                    MAP_MODEL_GROUP *grp = g_map.floor[cell_idx].grp;
                    sint32 count = (grp == g_map.grps ? 0 : grp->active_count), i;
                    if (count > 0)
                    {
                        sint32 grp_idx = grp - g_map.grps;
                        XASSERT(!(grp_idx >= g_map.rec_capacity || count > g_map.rec_capacity - grp_idx - 1));
                    }
                    if (count != 0)
                        for (i = 0; i < count; ++i)
                        {
                            MAP_MODEL_INSTANCE *instance = (grp + 1) + i;
                            XASSERT((instance->model_id) < g_model_catalog.count);
                            MODEL_DESC *model = &g_model_catalog.descs[instance->model_id];
                            sint32 upper = instance->y - model->half_y;
                            sint32 lower = instance->y + model->half_y;
                            sint32 correction;
                            if (model->col != 1 || upper < (ny - height_word) || ny < lower)
                            {
                                continue;
                            }
                            correction = ny - lower;
                            if (correction < 0x16 && static_result.step < correction + 1)
                                static_result.step = correction + 1;
                            uint32 idx;
                            sint32 same_row = 1;
                            for (idx = 0; idx < floor_count; ++idx)
                            {
                                if (touched_rows[idx] != row)
                                {
                                    same_row = 0;
                                    break;
                                }
                            }
                            if (same_row)
                            {
                                static_result.y = dst_y * 256;
                                static_result.flags &= ~9;
                                ny = dst_y;
                            }
                            z_blocked = 1;
                            static_result.flags |= 4;
                            static_result.z = ((g_map.depth * 0xc0u - cur_z) << 8);
                            break;
                        }
                }
            }
        }
        if (!x_blocked && nx != ox)
        {
            sint32 edge = nx < ox ? (walk_min_x - 1u) : walk_max_x;
            sint32 crossed = nx < ox ? ((edge & 0x3f) == 0x3f) : ((edge & 0x3f) == 0);
            if (crossed)
            {
                sint32 column = div_64_trunc(edge);
                sint32 first = div_64_trunc(walk_min_z);
                sint32 count_z = div_64_trunc(((walk_min_z & 0x3fu) + depth + 0x3f));
                sint32 zi;
                for (zi = 0, cz = first; zi < count_z; ++zi, ++cz)
                {
                    sint32 cell_idx = cz*g_map.width + column;
                    XASSERT(cell_idx < g_map.floor_count);
                    MAP_MODEL_GROUP *grp = g_map.floor[cell_idx].grp;
                    sint32 count = (grp == g_map.grps ? 0 : grp->active_count), i;
                    if (count > 0)
                    {
                        sint32 grp_idx = grp - g_map.grps;
                        XASSERT(!(grp_idx >= g_map.rec_capacity || count > g_map.rec_capacity - grp_idx - 1));
                    }
                    if (count != 0)
                        for (i = 0; i < count; ++i)
                        {
                            MAP_MODEL_INSTANCE *instance = (grp + 1) + i;
                            XASSERT((instance->model_id) < g_model_catalog.count);
                            MODEL_DESC *model = &g_model_catalog.descs[instance->model_id];
                            sint32 upper = instance->y - model->half_y;
                            sint32 lower = instance->y + model->half_y;
                            sint32 correction;
                            if (model->col != 1 || upper < (ny - height_word) || ny < lower)
                            {
                                continue;
                            }
                            correction = ny - lower;
                            if (correction < 0x16 && static_result.step < correction + 1)
                                static_result.step = correction + 1;
                            uint32 idx;
                            sint32 same_column = 1;
                            for (idx = 0; idx < floor_count; ++idx)
                            {
                                if (touched_columns[idx] != column)
                                {
                                    same_column = 0;
                                    break;
                                }
                            }
                            if (same_column)
                            {
                                static_result.y = dst_y * 256;
                                static_result.flags &= ~9;
                                ny = dst_y;
                            }
                            x_blocked = 1;
                            static_result.flags |= 2;
                            static_result.x = cur_x * 256;
                            break;
                        }
                }
            }
        }
        if (arrived)
            break;
        if (alternate)
        {
            if (cur_x < nx)
                ++cur_x;
            if (nx < cur_x)
                --cur_x;
        }
        else
        {
            if (cur_z < nz)
                ++cur_z;
            if (nz < cur_z)
                --cur_z;
        }
        alternate ^= 1;
    }
    return &static_result;
}

/* 0x80097DB8..0x80098224: original collision dispatcher and flag merge. */
/* Original: FUN_80097DB8. */
uint32 map_motion_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 *x, sint32 *y, sint32 *z, sint32 *velocity_x, sint32 *velocity_y, sint32 width, sint32 height, sint32 depth)
{
    sint32 grounded = 0;
    sint32 prev_y = *y;
    COLLISION_RESULT *result;
    g_col_flags = 0;
    sint32 floor_y = map_floor_height_at(old_x, *y, old_z);
    if (floor_y < *y && *y < (floor_y + 0x4000))
    {
        *y = floor_y;
        if (velocity_y)
            *velocity_y = 0;
        grounded = 1;
        g_col_flags |= 1;
        g_cur_col_result->flags |= 1;
        prev_y = floor_y;
    }
    else
        g_col_surface_resource = -2;
    if (*z != old_z || *x != old_x)
    {
        result = runtime_object_col_resolve(old_z, *x, *y, *z, ((width / 2u) << 8), ((uint32)height << 8));
        g_cur_col_result = result;
        *x = result->x;
        *z = result->z;
        g_col_flags |= result->flags;
    }
    result = static_map_col_resolve(old_x, old_y, old_z, *x, *y, *z, width, height, depth);
    g_cur_col_result = result;
    g_col_flags |= result->flags;
    *x = result->x;
    *y = result->y;
    *z = result->z;
    if ((result->flags & 2) && velocity_x)
        *velocity_x = 0;
    if (result->flags & 8)
    {
        if ((result->flags & 6) == 0)
        {
            if (velocity_y)
                *velocity_y = 0x100;
        }
        else
        {
            result = static_map_col_resolve(*x, (*y + 0x100u), *z, *x, *y, *z, width, height, depth);
            g_cur_col_result = result;
            *x = result->x;
            *y = result->y;
            *z = result->z;
            if ((result->flags & 8) == 0)
            {
                g_col_flags &= ~8;
                *y = prev_y;
            }
        }
    }
    if (result->step != 0 && ((result->flags & 1) || (g_col_flags & 1)))
    {
        *y = (*y - ((uint32)result->step << 8));
        if (velocity_y)
            *velocity_y = 0;
        return 0;
    }
    if (result->flags & 1)
    {
        if ((result->flags & 6) == 0)
        {
            if (velocity_y)
                *velocity_y = 0x100;
            return 1;
        }
        result = static_map_col_resolve(*x, *y, *z, *x, *y, *z, width, height, depth);
        g_cur_col_result = result;
        *x = result->x;
        *y = result->y;
        *z = result->z;
        if (!grounded && !(result->flags & 1))
        {
            g_col_flags &= ~1;
            *y = prev_y;
        }
    }
    return (g_col_flags & 1);
}

/* Original: FUN_80098244. */
sint16 swept_col_test(sint32 old_x, sint32 old_y, sint32 old_z, sint32 *new_x, sint32 *new_y, sint32 *new_z)
{
    sint32 x = *new_x, y = *new_y, z = *new_z;
    sint32 x_cell = div_16384_trunc(x);
    sint32 z_cell = div_16384_trunc(((g_map.depth * 3u << 14) - z));
    sint32 cell_idx = x_cell + z_cell * g_map.width;
    MAP_MODEL_GROUP *grp;
    MAP_MODEL_INSTANCE *instance;
    sint32 center_x = (x & 0xffffc0ffu) + 0x2000u;
    sint32 center_z = (z & 0xffffc0ffu) + 0x2000u;
    sint32 count, i;

    g_col_flags = 0;
    XASSERT(cell_idx < g_map.floor_count);
    grp = g_map.floor[cell_idx].grp;
    if (grp == g_map.grps)
        return 0;
    count = grp->active_count;
    if (count > 0)
    {
        sint32 grp_idx = grp - g_map.grps;
        XASSERT(!(grp_idx >= g_map.rec_capacity || count > g_map.rec_capacity - grp_idx - 1));
    }
    instance = grp + 1;
    for (i = 0; i < count; ++i, instance++)
    {
        sint32 model_idx = instance->model_id;
        sint32 model_y = instance->y * 256;
        XASSERT(model_idx < g_model_catalog.count);
        sint32 half_y = g_model_catalog.descs[model_idx].half_y * 256;
        sint32 top = model_y + half_y;
        sint32 bottom = model_y - half_y;

        if ((y - 0x800) < top && bottom < *new_y)
        {
            *new_y = top;
            g_col_flags |= 1;
            return g_col_flags;
        }
        /* Negative descriptor half-height makes the interior top < y < bottom */
        if (y < bottom && top < y)
        {
            if (old_x < x && x < center_x)
            {
                *new_x = (center_x - 0x2800u);
                g_col_flags |= 2;
            }
            if (x < old_x && center_x < x)
            {
                *new_x = (center_x + 0x2800u);
                g_col_flags |= 2;
            }
            if (old_z < z && z < center_z)
            {
                *new_z = (center_z - 0x2800u);
                g_col_flags |= 4;
            }
            if (z < old_z && center_z < z)
            {
                *new_z = (center_z + 0x2800u);
                g_col_flags |= 4;
            }
            return g_col_flags;
        }
    }

    if (*new_z != old_z || *new_x != old_x)
    {
        COLLISION_RESULT *result = runtime_object_col_resolve(old_z, *new_x, *new_y, *new_z, 0x100, 0x100);
        g_cur_col_result = result;
        *new_x = result->x;
        *new_y = result->y;
        *new_z = result->z;
        g_col_flags |= result->flags;
    }
    if ((g_col_flags & 4) == 0)
        return 0;
    return g_col_flags;
}

/* Original: SLES_004.74:FUN_80098588. */
void object_cols_dispatch(void)
{
    COLLISION *src;
    COLLISION *target;

    for (src = (COLLISION *)g_object_list.sentinel.next; src != 0; src = (COLLISION *)src->object.next)
    {
        uint32 sends_mask = src->sends_mask;
        sint32 src_x, src_y, src_z, src_max_x, src_max_y, src_max_z;
        if (sends_mask == 0)
            continue;

        /* The original snapshots source bounds and mask before target callbacks */
        src_x = ((uint32)src->x + src->box_x);
        src_y = ((uint32)src->y + src->box_y);
        src_z = ((uint32)src->z + src->box_z);
        src_max_x = ((uint32)src_x + src->width);
        src_max_y = ((uint32)src_y + src->height);
        src_max_z = ((uint32)src_z + src->depth);

        for (target = (COLLISION *)g_object_list.sentinel.next; target != 0; target = (COLLISION *)target->object.next)
        {
            sint32 target_x, target_y, target_z;
            if ((sends_mask & target->receives_mask) == 0)
                continue;

            target_x = ((uint32)target->x + target->box_x);
            if (src_max_x < target_x || (target_x + target->width) < src_x)
                continue;
            target_y = ((uint32)target->y + target->box_y);
            if (src_max_y < target_y || (target_y + target->height) < src_y)
                continue;
            target_z = ((uint32)target->z + target->box_z);
            if (src_max_z < target_z || (target_z + target->depth) < src_z)
                continue;

            if ((sends_mask & 2) != 0)
                target->player_contact_callback(target, src);
            if ((sends_mask & 0xc1) != 0)
            {
                if (target->hit_callback_kind == COLLISION_CALLBACK_OBJECT)
                    target->hit_object_callback(target, src);
                else
                    target->hit_callback(target, src);
            }
        }
    }
}

/* Original: FUN_8009875C. */
GDB_CALL void col_box_set(void *object_val, sint32 width, sint32 height, sint32 depth)
{
    COLLISION *object = (COLLISION *)object_val;
    object->width = ((uint32)width << 8);
    object->height = ((uint32)height << 8);
    object->depth = ((uint32)depth << 8);
    object->box_x = ((-(uint32)div_pow2_trunc(width, 1)) << 8);
    object->box_y = ((-(uint32)height) << 8);
    object->box_z = ((-(uint32)div_pow2_trunc(depth, 1)) << 8);
}

/* Original: FUN_800AD8A8. */
void hierarchy_col_box_update(COLLISION *object, const MODEL_NODE *first_node, sint16 count)
{
    const MODEL_NODE *node = first_node;
    sint16 remaining;
    sint32 min_x = node->world_x, min_y = node->world_y, min_z = node->world_z;
    sint32 max_x = min_x, max_y = min_y, max_z = min_z;
    sint32 width, height, depth;
    for (remaining = (count - 1); remaining > 0; --remaining)
    {
        ++node;
        if (node->world_x < min_x)
            min_x = node->world_x;
        if (max_x < node->world_x)
            max_x = node->world_x;
        if (node->world_y < min_y)
            min_y = node->world_y;
        if (max_y < node->world_y)
            max_y = node->world_y;
        if (node->world_z < min_z)
            min_z = node->world_z;
        if (max_z < node->world_z)
            max_z = node->world_z;
    }
    width = div_pow2_trunc(((uint32)max_x - min_x), 8);
    height = div_pow2_trunc(((uint32)max_y - min_y), 8);
    depth = div_pow2_trunc(((uint32)max_z - min_z), 8);
    col_box_set(object, width, height, depth);
    object->box_y = (object->box_y + ((uint32)div_pow2_trunc(height, 1) << 8));
}

/* Original: FUN_800987B4. */
sint16 object_damage_apply(void *target_val, void *src_val)
{
    COLLISION *target = (COLLISION *)target_val;
    COLLISION *src = (COLLISION *)src_val;
    sint16 type;
    target->hit.health = (target->hit.health - src->hit.damage);
    type = src->object_type;
    if (type == 10 || type == 0x64)
    {
        target->flash_clut_ticks = 1;
        src->hit.damage = 0;
    }
    else
    {
        target->flash_clut_ticks = (g_render_state.frame & 1);
        src->receives_mask |= 0x20;
    }
    return target->hit.health < 1;
}

/* Original: SLES_004.74:FUN_80098824. */
sint16 map_environment_damage_test(sint32 world_x, sint32 world_y, sint32 world_z, sint32 width, sint32 height, sint32 depth)
{
    sint32 min_x = div_256_trunc(world_x) - width / 2;
    sint32 min_z = g_map.depth * 0xc0 - div_256_trunc(world_z) - depth / 2;
    sint32 row_count = div_64_trunc((min_z & 0x3f) + depth + 0x3f);
    uint32 row_start = div_64_trunc(min_x) + (uint32)div_64_trunc(min_z) * g_map.width;
    sint32 bottom_y = div_256_trunc(world_y);
    sint32 col_count = div_64_trunc((min_x & 0x3f) + width + 0x40);
    sint32 row;

    for (row = 0; row < row_count; ++row, row_start += g_map.width)
    {
        sint32 col;
        for (col = 0; col < col_count; ++col)
        {
            sint32 cell_idx = row_start + col;
            XASSERT(cell_idx < g_map.floor_count);
            const MAP_FLOOR_CELL *cell = &g_map.floor[cell_idx];
            const MAP_MODEL_GROUP *grp = cell->grp;
            const MAP_MODEL_INSTANCE *instance;
            sint32 model_count, model_idx;
            if (grp == g_map.grps)
                continue;
            model_count = grp->active_count;
            if (model_count > 0)
            {
                sint32 grp_idx = grp - g_map.grps;
                XASSERT(!(grp_idx >= g_map.rec_capacity || model_count > g_map.rec_capacity - grp_idx - 1));
            }
            instance = grp + 1;
            for (model_idx = 0; model_idx < model_count; ++model_idx, instance++)
            {
                sint16 model_id = instance->model_id;
                sint32 model_y = instance->y;
                XASSERT(model_id < g_model_catalog.count);
                sint32 half_y = g_model_catalog.descs[model_id].half_y;
                if (model_y + half_y - 4 < bottom_y && bottom_y - height < model_y - half_y + 4)
                {
                    sint32 attribute = model_attribute_find(model_id);
                    if ((attribute >= 81 && attribute < 89))
                        return 1;
                }
            }
        }
    }
    return 0;
}
