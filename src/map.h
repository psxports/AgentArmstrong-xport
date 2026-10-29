#ifndef MODULE_API_MAP_H
#define MODULE_API_MAP_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"
#include "effect_update.h"
#include "resource_table.h"

/* Types. */
typedef struct MAP_STAMP
{
    sint16 cell_x, cell_z;
    sint16 width, height;
    const sint16 *vals;
} MAP_STAMP;

typedef struct MAP_LIGHT_GRID
{
    uint8 *vals;
    uint32 count;
} MAP_LIGHT_GRID;

/* Header and model roles share records in the original deformation traversal */
typedef struct MAP_MODEL_RECORD
{
    union
    {
        sint16 model_id;
        sint16 active_count;
    };

    union
    {
        sint16 y;
        sint16 y_offset;
    };

    union
    {
        sint16 local_x;
        sint16 removed_count;
    };

    sint16 local_z;
    sint16 yaw;
    uint8 light;
} MAP_MODEL_RECORD;

typedef MAP_MODEL_RECORD MAP_MODEL_GROUP;
typedef MAP_MODEL_RECORD MAP_MODEL_INSTANCE;

typedef struct
{
    MAP_MODEL_GROUP *grp;
    sint16 height;
    uint8 material, shade, packed_orientation, tile_flags;
    sint8 height_step;
} MAP_FLOOR_CELL;

typedef struct
{
    sint16 model_id, attribute;
} MAP_MODEL_ATTRIBUTE;

typedef struct
{
    RESOURCE_DATA floor, grps;
    /* The original attribute sentinel scan continues into the owning archive */
    RESOURCE_DATA attribute_lookup;
} MAP_INPUT;

/* Decoded native allocations and stage dimensions share one map lifetime */
typedef struct MAP_STATE
{
    sint32 width, depth, width_subcells, depth_subcells;
    uint8 terrain_variation;
    MAP_FLOOR_CELL *floor;
    MAP_MODEL_GROUP *grps;
    MAP_MODEL_ATTRIBUTE *attributes;
    sint32 floor_count, rec_capacity, attribute_count;
    MAP_LIGHT_GRID light;
} MAP_STATE;

extern MAP_STATE g_map;

/* BEGIN GENERATED MODULE API */
MAP_FLOOR_CELL *map_floor_cell_at(sint32 world_x, sint32 world_z);
sint32 map_floor_height_at(sint32 world_x, sint32 cur_y, sint32 world_z);
sint32 model_attribute_find(sint16 model_id);
void map_grid_initialize(const MAP_INPUT *input);
void map_light_stamp_apply(sint32 world_x, sint32 world_z, sint16 clear, const MAP_STAMP *stamp);
void map_runtime_initialize(sint32 width, sint32 height);
/* END GENERATED MODULE API */

/* Native stage-heap lifetime boundary */
void map_heap_reset(void);

#endif
