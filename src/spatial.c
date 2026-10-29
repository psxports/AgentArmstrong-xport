#include <stdio.h>
#include <stdlib.h>

#include "camera.h"
#include "render.h"
#include "collision.h"
#include "object.h"
#include "map.h"
#include "spatial.h"

/* Original: FUN_800933E8. */
sint32 world_object_is_visible(sint32 x, sint32 y, sint32 z, sint32 diameter)
{
    sint32 depth; uint32 left; sint32 numerator; sint32 proj; sint32 vertical_limit; sint32 map_limit;
    if (z == g_camera.world_z)
        return 0;
    depth = ((uint32)z - g_camera.world_z);
    left = ((uint32)x - (diameter / 2));
    numerator = ((left - g_camera.world_x) * 320u);
    if (depth == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("VISIBILITY PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    proj = numerator / depth;
    if (proj > g_render_state.half_width)
        return 0;
    numerator = ((left + diameter - g_camera.world_x) * 320u);
    if (depth == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("VISIBILITY PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    proj = numerator / depth;
    if (proj < (-g_render_state.half_width))
        return 0;
    if (z > (g_camera.world_z + 0x3e800))
        return 0;
    map_limit = (g_map.depth * 3u << 14);
    if (z >= map_limit)
        return 0;
    if (z < (g_camera.world_z + 0x4000))
        return 0;
    if (z <= (map_limit - (g_map.depth << 14) + 0x4000))
        return 0;
    vertical_limit = g_render_state.height / 2 + 0x80;
    numerator = (((uint32)y - g_camera.world_y) * 355u);
    if (depth == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("VISIBILITY PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    proj = numerator / depth;
    if (proj < -vertical_limit)
        return 0;
    numerator = ((y - (g_camera.world_y + 0x8000u)) * 355u);
    if (depth == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("VISIBILITY PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    return numerator / depth <= vertical_limit;
}

/* Original: SLES_004.74:DAT_800E89F0 */
FRAME_OBJECT g_frame_objects[FRAME_OBJECT_LIMIT];

/* Original: SLES_004.74:DAT_800D3F80 */
FRAME_OBJECT *g_next_frame_object = g_frame_objects;

/* Original: SLES_004.74:DAT_800D4174 */
static SPATIAL_BUCKET spatial_buckets[SPATIAL_BUCKET_COUNT];

/* Original: SLES_004.74:DAT_800D3E5C */
SPATIAL_BUCKET **g_spatial_cell_buckets;
sint32 g_spatial_cell_count;

/* Original: FUN_8008E3A0. */
void spatial_buckets_reset(void)
{
    uint32 idx;
    for (idx = 0; idx < SPATIAL_BUCKET_COUNT; ++idx)
    {
        SPATIAL_BUCKET *bucket = spatial_buckets + idx;
        if (g_spatial_cell_buckets != 0 && bucket->cell_idx < g_spatial_cell_count)
            g_spatial_cell_buckets[bucket->cell_idx] = 0;
        bucket->count = 0;
        bucket->cell_idx = 0;
    }
}

/* Original: SLES_004.74:FUN_8008E410. */
void spatial_buckets_build(void)
{
    FRAME_OBJECT *object = g_frame_objects;
    uint32 allocated_buckets = 0;
    XASSERT(!(g_next_frame_object < g_frame_objects || g_next_frame_object >= g_frame_objects + FRAME_OBJECT_LIMIT));
    while (object < g_next_frame_object)
    {
        sint32 x = object->world_x + ((uint32)object->cell_x_offset << 8);
        sint32 z = g_map.depth * 0xc000u - object->world_z;
        sint16 cell_idx;
        SPATIAL_BUCKET *bucket;
        if (x < 0)
            x += 0x3fff;
        if (z < 0)
            z += 0x3fff;
        cell_idx = ((x >> 14) + (z >> 14) * g_map.width);
        XASSERT(!(g_spatial_cell_buckets == 0 || cell_idx >= g_spatial_cell_count));
        bucket = g_spatial_cell_buckets[cell_idx];
        if (bucket == 0)
        {
            /* Original slot zero denotes an empty cell */
            ++allocated_buckets;
            XASSERT(allocated_buckets < SPATIAL_BUCKET_COUNT);
            bucket = spatial_buckets + allocated_buckets;
            bucket->cell_idx = cell_idx;
            g_spatial_cell_buckets[cell_idx] = bucket;
        }
        if (bucket->count < SPATIAL_BUCKET_CAPACITY)
            bucket->objects[bucket->count++] = object;
        ++object;
    }
}

/* Original: SLES_004.74:FUN_80092CF0. */
FRAME_OBJECT *frame_model_create(COLLISION *object, sint32 model_plus_one)
{
    FRAME_OBJECT *rec = g_next_frame_object;
    /* Slot255 remains writable for next-record presets; 256 published records fail E410 */
    XASSERT(!(rec < g_frame_objects || rec >= g_frame_objects + FRAME_OBJECT_LIMIT - 1));
    rec->world_x = object->x;
    rec->world_y = object->y;
    rec->world_z = object->z;
    rec->drawn = 0;
    rec->model_plus_one = model_plus_one;
    rec->cell_x_offset = 0;
    rec->model_rot_y = 0;
    rec->model_rot_x = 0;
    rec->model_rot_z = 0;
    rec->ot_bucket = 0;
    g_next_frame_object = rec + 1;
    return rec;
}

sint32 spatial_cell_objects_get(sint32 cell_x, sint32 cell_z, FRAME_OBJECT **out)
{
    const SPATIAL_BUCKET *bucket;
    sint32 idx;
    if (g_spatial_cell_buckets == 0 || cell_x < 0 || cell_z < 0 || cell_x >= g_map.width || cell_z >= g_map.depth)
        return 0;
    idx = cell_z * g_map.width + cell_x;
    if (idx >= g_spatial_cell_count)
        return 0;
    bucket = g_spatial_cell_buckets[idx];
    if (bucket == 0)
        return 0;
    for (idx = 0; idx < bucket->count; ++idx)
        out[idx] = bucket->objects[idx];
    return bucket->count;
}
