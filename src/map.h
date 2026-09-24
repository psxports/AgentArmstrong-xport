#ifndef MODULE_API_MAP_H
#define MODULE_API_MAP_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"
#include "effect_update.h"

/* Types. */
typedef struct MAP_STAMP MAP_STAMP;

typedef struct
{
    uint16 group_index;       /* +0x00 */
    sint16 height;            /* +0x02 */
    uint8 material;           /* +0x04 */
    uint8 field_05;           /* +0x05 */
    uint8 packed_orientation; /* +0x06 */
    uint8 flags;              /* +0x07 */
    sint8 slope;              /* +0x08 */
    uint8 field_09;           /* +0x09 */
} MAP_FLOOR_CELL;

typedef struct MAP_MODEL_GROUP
{
    sint16 active_count;  /* +0x00 */
    sint16 y_offset;      /* +0x02 */
    sint16 removed_count; /* +0x04 */
    sint16 field_06;      /* +0x06 */
    sint16 field_08;      /* +0x08 */
    sint16 field_0a;      /* +0x0A */
} MAP_MODEL_GROUP;

typedef struct
{
    sint16 model_id; /* +0x00 */
    sint16 y;        /* +0x02 */
    sint16 local_x;  /* +0x04 */
    sint16 local_z;  /* +0x06 */
    sint16 yaw;      /* +0x08 */
    uint8 light;     /* +0x0A */
    uint8 field_0b;  /* +0x0B */
} MAP_MODEL_INSTANCE;

#if defined(AP_32BIT)
typedef char MAP_FLOOR_CELL_size_0a[sizeof(MAP_FLOOR_CELL) == 0x0a ? 1 : -1];
typedef char MAP_FLOOR_CELL_height_02[offsetof(MAP_FLOOR_CELL, height) == 0x02 ? 1 : -1];
typedef char MAP_FLOOR_CELL_slope_08[offsetof(MAP_FLOOR_CELL, slope) == 0x08 ? 1 : -1];
typedef char MAP_MODEL_GROUP_size_0c[sizeof(MAP_MODEL_GROUP) == 0x0c ? 1 : -1];
typedef char MAP_MODEL_GROUP_removed_04[offsetof(MAP_MODEL_GROUP, removed_count) == 0x04 ? 1 : -1];
typedef char MAP_MODEL_INSTANCE_size_0c[sizeof(MAP_MODEL_INSTANCE) == 0x0c ? 1 : -1];
typedef char MAP_MODEL_INSTANCE_light_0a[offsetof(MAP_MODEL_INSTANCE, light) == 0x0a ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
MAP_FLOOR_CELL *map_floor_cell_at(sint32 world_x, sint32 world_z);
sint32 map_floor_height_at(sint32 world_x, sint32 current_y, sint32 world_z);
sint32 model_attribute_find(sint16 model_id);
void horizontal_camera_limit_create(EFFECT *source);
void map_grid_initialize(void);
void map_light_stamp_apply(sint32 world_x, sint32 world_z, sint16 clear, const MAP_STAMP *stamp);
void map_runtime_initialize(sint32 width, sint32 height);
void room_camera_limits_update(void);
void room_depth_limit_create(void);
void vertical_camera_limit_create(EFFECT *source);
/* END GENERATED MODULE API */

#endif
