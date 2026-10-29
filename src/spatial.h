#ifndef MODULE_API_SPATIAL_H
#define MODULE_API_SPATIAL_H

#include "xport.h"
#include "psx.h"
#include "collision.h"

enum
{
    SPATIAL_BUCKET_COUNT = 64,
    SPATIAL_BUCKET_CAPACITY = 24,
    FRAME_OBJECT_LIMIT = 256
};

typedef struct FRAME_OBJECT
{
    POLY_FT4 *prim;
    sint32 world_x, world_y, world_z;
    sint32 sort_x;
    sint16 ot_bucket;
    sint16 cell_x_offset;
    sint16 clip_width; /* Becomes Q8 during clipped submission */
    sint32 model_plus_one;
    sint16 model_rot_y, model_rot_x, model_rot_z;
    sint16 light_delta;
    uint8 drawn, force_light;
} FRAME_OBJECT;

typedef struct
{
    uint8 count;
    sint16 cell_idx;
    FRAME_OBJECT *objects[SPATIAL_BUCKET_CAPACITY];
} SPATIAL_BUCKET;

extern FRAME_OBJECT g_frame_objects[FRAME_OBJECT_LIMIT];
extern FRAME_OBJECT *g_next_frame_object;
extern SPATIAL_BUCKET **g_spatial_cell_buckets;
extern sint32 g_spatial_cell_count;

sint32 world_object_is_visible(sint32 x, sint32 y, sint32 z, sint32 diameter);

sint32 spatial_cell_objects_get(sint32 cell_x, sint32 cell_z, FRAME_OBJECT **out);
FRAME_OBJECT *frame_model_create(COLLISION *object, sint32 model_plus_one);
void spatial_buckets_build(void);
void spatial_buckets_reset(void);

#endif
