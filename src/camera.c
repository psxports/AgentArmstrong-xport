#include <stdlib.h>

#include "game_math.h"
#include "collision.h"
#include "effect_update.h"
#include "map.h"
#include "game.h"
#include "render.h"
#include "camera.h"
#include "object.h"
#include "runtime_heap.h"
#include "player.h"

/* Exact type-0x29 room/depth limit logic from FUN_800A7ACC/800A7C00. */

typedef struct
{
    sint32 min_x, min_y, max_x, max_y, signed_depth, side;
} ROOM_DEPTH_LIMIT;

typedef struct
{
    COLLISION col;
    sint32 min_x, min_y, min_z;
    sint32 margin;
    sint32 max_x, max_y, max_z;
    sint16 activates_room;
} CAMERA_LIMIT;

/* The original table at 0x800EA298 holds forty records plus the terminating
 * -1 word. */
static ROOM_DEPTH_LIMIT room_depth_limits[41];
/* Original material255 reads the same word as room record31's side */
const sint32 *const g_void_floor_material = &room_depth_limits[31].side;
static sint32 room_depth_limit_count;

CAMERA_STATE g_camera;

/* Functions. */
/* Original: FUN_8009C0C8. */
void camera_update(sint32 player_x, sint32 player_y, sint32 player_z, sint32 mode)
{
    sint32 delta, step, travel_limit, target_x, target_y, target_z, upper_y, snapshot_z = player_z;
    sint32 horizontal, lookahead = 0, depth_target;
    uint32 player_camera_flags;
    if (g_camera.frozen != 0)
        return;
    horizontal = mode == 2 ? 1 : mode == 6 ? -1 : 0;
    player_camera_flags = g_player->sampled_buttons;
    depth_target = ((mode >= 3 && mode < 5) || mode == 5) ? 0x40 : 0;
    depth_target += g_camera.depth_offset_target;
    if (g_camera.depth_offset < depth_target)
        g_camera.depth_offset += 2;
    if (depth_target < g_camera.depth_offset)
        g_camera.depth_offset -= 2;

    delta = ((uint32)player_x - g_camera.prev_player_x);
    if (delta == 0)
        delta = horizontal;
    if (delta < 0)
        lookahead = g_camera.horizontal_bias - 0x6000;
    if (delta > 0)
        lookahead = g_camera.horizontal_bias + 0x6000;
    if (horizontal == 0)
        lookahead = 0;
    target_x = ((uint32)player_x + lookahead);
    travel_limit = (abs_s32(delta) + 0x200u);
    step = abs_s32(((uint32)g_camera.world_x - target_x));
    if (travel_limit < step)
        step = travel_limit;
    if (g_camera.world_x < target_x)
        g_camera.world_x = ((uint32)g_camera.world_x + step);
    if (target_x < g_camera.world_x)
        g_camera.world_x = ((uint32)g_camera.world_x - step);
    if (g_camera.prev_z == -1)
        g_camera.world_x = player_x;

    if (g_camera.limit_left != -1)
    {
        sint32 boundary = g_camera.limit_left + ((uint32)g_render_state.half_width << 8) - 0x4000u;
        if (g_camera.world_x < boundary)
        {
            if (g_camera.prev_z == -1)
                g_camera.world_x = boundary;
            else
            {
                step = abs_s32(((uint32)g_camera.world_x - boundary));
                if (step > 0x800)
                    step = 0x800;
                g_camera.world_x = ((uint32)g_camera.world_x + step);
            }
        }
    }
    if (g_camera.limit_right != -1)
    {
        sint32 boundary = g_camera.limit_right - ((uint32)g_render_state.half_width << 8) + 0x4000u;
        if (boundary < g_camera.world_x)
        {
            if (g_camera.prev_z == -1)
                g_camera.world_x = boundary;
            else
            {
                step = abs_s32(((uint32)g_camera.world_x - boundary));
                if (step > 0x800)
                    step = 0x800;
                g_camera.world_x = ((uint32)g_camera.world_x - step);
            }
        }
    }

    target_z = player_z - 0x15e00u - (g_camera.depth_offset << 8);
    if (g_camera.prev_z == -1)
    {
        g_camera.world_z = target_z;
        g_camera.world_y = (player_y - 0x7800u);
        g_camera.target_y = g_camera.world_y;
    }
    else
    {
        if (target_z < g_camera.limit_near)
            target_z = g_camera.limit_near;
        if (g_camera.limit_far < target_z)
            target_z = g_camera.limit_far;
        step = abs_s32(((uint32)g_camera.world_z - target_z));
        if (step > 0x500)
            step = 0x500;
        if (g_camera.world_z < target_z)
            g_camera.world_z = ((uint32)g_camera.world_z + step);
        if (target_z < g_camera.world_z)
            g_camera.world_z = ((uint32)g_camera.world_z - step);
        snapshot_z = (((g_map.depth * 3u << 6) - g_map.depth_subcells) << 8);
        if (g_camera.world_z < snapshot_z)
            g_camera.world_z = snapshot_z;
    }

    target_y = (player_y - (g_session.scuba_active ? 0x6000u : 0x8000u) + g_camera.manual_y_offset);
    step = abs_s32(((uint32)g_camera.target_y - target_y));
    if (step > 0x300)
        step = 0x300;
    if (g_camera.target_y < target_y)
        g_camera.target_y = ((uint32)g_camera.target_y + step);
    upper_y = (target_y + 0x1c00u);
    step = abs_s32(((uint32)g_camera.target_y - upper_y));
    if (step > 0x300)
        step = 0x300;
    if (upper_y < g_camera.target_y)
        g_camera.target_y = ((uint32)g_camera.target_y - step);
    if (g_camera.limit_bottom != -1)
    {
        if (g_camera.prev_z == -1)
            g_camera.target_y = g_camera.limit_bottom;
        else if (g_camera.limit_bottom < g_camera.target_y)
        {
            step = abs_s32(((uint32)g_camera.target_y - g_camera.limit_bottom));
            if (step > 0x800)
                step = 0x800;
            g_camera.target_y = ((uint32)g_camera.target_y - step);
        }
    }
    if (g_camera.limit_top != -1)
    {
        if (g_camera.prev_z == -1)
            g_camera.target_y = g_camera.limit_top;
        else if (g_camera.target_y < g_camera.limit_top)
        {
            step = abs_s32(((uint32)g_camera.target_y - g_camera.limit_top));
            if (step > 0x800)
                step = 0x800;
            g_camera.target_y = ((uint32)g_camera.target_y + step);
        }
    }
    g_camera.world_y = g_camera.target_y;

    if ((player_camera_flags & 10) == 0)
    {
        if (g_camera.manual_y_offset > 0)
            g_camera.manual_y_offset -= 0x800;
        if (g_camera.manual_y_offset < 0)
            g_camera.manual_y_offset += 0x800;
    }
    else
    {
        if (player_camera_flags & 8)
            g_camera.manual_y_offset -= 0x800;
        if (player_camera_flags & 2)
            g_camera.manual_y_offset += 0x800;
        if (g_camera.manual_y_offset > 0x6000)
            g_camera.manual_y_offset = 0x6000;
        if (g_camera.manual_y_offset < -0x4000)
            g_camera.manual_y_offset = -0x4000;
    }
    g_camera.prev_player_x = player_x;
    /* Original GP+214/+218 retain adjusted Y/Z work values on this path */
    g_camera.prev_player_y = upper_y;
    g_camera.prev_player_z = snapshot_z;
    if (g_camera.prev_z == -1)
    {
        g_camera.prev_x = g_camera.world_x;
        g_camera.prev_y = g_camera.world_y;
        g_camera.prev_z = g_camera.world_z;
    }
    g_camera.render_x = div_256_trunc(g_camera.world_x);
    g_camera.render_y = div_256_trunc(g_camera.world_y);
    g_camera.render_z = div_256_trunc(g_camera.world_z);
}

/* Direct translation of 0x800A8264..0x800A8298. */
/* Original: FUN_800A8264. */
void camera_depth_limit_set(EFFECT *effect)
{
    if (effect->vals[0] == 0)
        g_camera.player_far_z_limit = effect->z;
    else
        g_camera.player_near_z_limit = effect->z;
}

/* 800A7DA8: horizontal camera-limit object update. */
/* Original: FUN_800A7DA8. */
static void horizontal_camera_limit_update(void *raw_object)
{
    CAMERA_LIMIT *object = (CAMERA_LIMIT *)raw_object;
    if (g_player_world.x < object->min_x || object->max_x < g_player_world.x || g_player_world.y < object->min_y || object->max_y < g_player_world.y || g_player_world.z < object->min_z || object->max_z < g_player_world.z)
        return;
    if (object->activates_room != 0)
        g_camera.room_activation_pending = 1;
    if (object->col.object_type == 0x4e)
        g_camera.limit_left = ((uint32)object->min_x + object->margin);
    else
        g_camera.limit_right = ((uint32)object->max_x - object->margin);
}

/* 800A7E98: constructor for effect types 0x4E and 0x4F. */
/* Original: FUN_800A7E98. */
void horizontal_camera_limit_create(EFFECT *src)
{
    CAMERA_LIMIT *object = (CAMERA_LIMIT *)runtime_heap_allocate_best_fit(sizeof(*object));
    sint32 margin;
    object->col.object.update = horizontal_camera_limit_update;
    linked_list_insert_after(&g_object_list, object, &g_object_list.sentinel);
    if (src->vals[0] == 0)
        src->vals[0] = 2;
    if (src->vals[1] == 0)
        src->vals[1] = 6;
    src->y = (src->y + 0x2000u);
    src->z = (src->z - 0x2000u);
    object->col.object_type = src->type;
    object->max_y = src->y;
    object->min_y = (src->y - ((uint32)(sint16)src->vals[1] << 14));
    object->min_z = src->z;
    object->max_z = (src->z + ((uint32)(sint16)src->vals[2] << 14));
    if (src->type == 0x4e)
    {
        object->min_x = (src->x - 0x2000u);
        object->max_x = (object->min_x + ((uint32)(sint16)src->vals[0] << 14));
    }
    else
    {
        object->max_x = (src->x + 0x2000u);
        object->min_x = (object->max_x - ((uint32)(sint16)src->vals[0] << 14));
    }
    margin = src->vals[3] != 0 ? 0xb000 : 0x5000;
    if (src->vals[3] != 0)
        object->activates_room = 1;
    object->margin = margin;
    object->min_x = ((uint32)object->min_x - margin);
    object->max_x = ((uint32)object->max_x + margin);
    src->type = 0;
}

/* 800A8018: vertical camera-limit object update. */
/* Original: FUN_800A8018. */
static void vertical_camera_limit_update(void *raw_object)
{
    CAMERA_LIMIT *object = (CAMERA_LIMIT *)raw_object;
    if (g_player_world.x < object->min_x || object->max_x < g_player_world.x || g_player_world.y < object->min_y || object->max_y < g_player_world.y || g_player_world.z < object->min_z || object->max_z < g_player_world.z)
        return;
    if (object->col.object_type == 0x6b)
        g_camera.limit_top = (object->min_y + 0x7800u);
    else
        /* 800A80B4 delay slot computes maxY-0x6000 before the non-0x6B
         * branch applies its additional -0x6C00. */
        g_camera.limit_bottom = (object->max_y - 0x6000u - 0x6c00u);
}

/* 800A80D8: constructor for effect types 0x6B and 0x6C. */
/* Original: FUN_800A80D8. */
void vertical_camera_limit_create(EFFECT *src)
{
    CAMERA_LIMIT *object = (CAMERA_LIMIT *)runtime_heap_allocate_best_fit(sizeof(*object));
    object->col.object.update = vertical_camera_limit_update;
    linked_list_insert_after(&g_object_list, object, &g_object_list.sentinel);
    if (src->vals[1] == 0)
        src->vals[1] = 2;
    src->vals[2] = 1;
    src->z = (src->z - 0x2000u);
    src->x = (src->x - 0x2000u);
    object->min_x = src->x;
    object->min_z = src->z;
    object->max_x = (src->x + ((uint32)(sint16)src->vals[1] << 14));
    object->max_z = (src->z + ((uint32)(sint16)src->vals[3] << 14));
    object->col.object_type = src->type;
    if (src->type == 0x6b)
    {
        object->min_y = (src->y - 0x2000u);
        object->max_y = (object->min_y + ((uint32)(sint16)src->vals[2] << 14));
    }
    else
    {
        object->max_y = (src->y + 0x2000u);
        object->min_y = (object->max_y - ((uint32)(sint16)src->vals[2] << 14));
    }
    object->min_y = (object->min_y - 0x6000u);
    object->max_y = (object->max_y + 0x6000u);
    src->type = 0;
}

/* Original: FUN_800A7ACC. */
void room_depth_limit_create(void)
{
    EFFECT *src = 0;
    ROOM_DEPTH_LIMIT *limit = room_depth_limits;
    sint32 count = 0;
    for (;;)
    {
        XASSERT(count != 40);
        src = effect_find_next(0x29, src);
        if (src == 0)
        {
            limit->min_x = -1;
            room_depth_limit_count = count;
            return;
        }
        src->type = 0;
        limit->min_x = (src->x - (((sint16)src->vals[0] - 1u) << 14) - 0x2000u);
        limit->min_y = (src->y - (((sint16)src->vals[2] - 1u) << 14) - 0x2000u);
        limit->max_x = (src->x + (((sint16)src->vals[1] - 1u) << 14) + 0x2000u);
        limit->max_y = (src->y + (((sint16)src->vals[3] - 1u) << 14) + 0x2000u);
        limit->signed_depth = src->vals[4] != 0 ? (-(uint32)src->z) : src->z;
        limit->side = (sint16)src->vals[5];
        ++count;
        ++limit;
        ++src;
    }
}

/* Original: FUN_800A7C00. */
void room_camera_limits_update(void)
{
    sint32 idx;
    g_camera.room_activation_pending = 0;
    g_camera.limit_near = g_camera.limit_left = g_camera.limit_right = -1;
    g_camera.limit_top = g_camera.limit_bottom = -1;
    g_camera.room_player_near_z = g_camera.room_player_far_z = 0;
    g_camera.limit_far = (g_map.depth * 0xc000u);
    /* 0x800A7C48..0x800A7C60: while the player is following the recovery
     * path, the original skips all type-0x29 room depth-limit records. */
    if (g_player->col.object.update == player_checkpoint_recovery_update)
        return;
    for (idx = 0; idx < room_depth_limit_count; ++idx)
    {
        ROOM_DEPTH_LIMIT *limit = &room_depth_limits[idx];
        sint32 depth = limit->signed_depth;
        sint32 distance;
        if (limit->min_x == -1)
            break;
        if (depth < 0)
            depth = (-(uint32)depth);
        if (!(limit->min_x < g_player_world.x && limit->min_y < g_player_world.y && g_player_world.x < limit->max_x && g_player_world.y < limit->max_y))
            continue;
        distance = (depth - 0x10e00u - g_camera.world_z);
        if (distance < 0)
            distance = (-(uint32)distance);
        if (distance >= 0x11800)
            continue;
        if (limit->side == 0)
        {
            g_camera.limit_near = limit->signed_depth;
            if (g_camera.limit_near < 0)
            {
                g_camera.room_player_near_z = (-(uint32)g_camera.limit_near);
                g_camera.limit_near = (-(uint32)g_camera.limit_near);
            }
            g_camera.limit_near = (g_camera.limit_near - 0x10e00u);
        }
        else
        {
            g_camera.limit_far = limit->signed_depth;
            if (g_camera.limit_far < 0)
            {
                g_camera.room_player_far_z = (-(uint32)g_camera.limit_far);
                g_camera.limit_far = (-(uint32)g_camera.limit_far);
            }
            g_camera.limit_far = (g_camera.limit_far - 0x10e00u);
        }
    }
}
