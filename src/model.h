#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"
#include "map.h"
#include "render.h"

/* Original model template slots hold one shared FT3 or FT4 packet */
typedef union
{
    POLY_FT3 triangle;
    POLY_FT4 quad;
} MODEL_PACKET;

extern const MATRIX g_model_identity;

typedef struct
{
    const void *data;
    uint32 size;
} MODEL_POSE_RESOURCE;

typedef struct MODEL_NODE
{
    struct MODEL_NODE *next_sibling;
    struct MODEL_NODE *prev_sibling;
    struct MODEL_NODE *first_child;
    struct MODEL_NODE *parent;
    MATRIX local_matrix;
    sint32 world_x;
    sint32 world_y;
    sint32 world_z;
    sint16 world_rotation_x;
    sint16 world_rotation_y;
    sint16 world_rotation_z;
    sint16 rotation_x;
    sint16 rotation_y;
    sint16 rotation_z;
    sint32 translation_x;
    sint32 translation_y;
    sint32 translation_z;
    sint16 scale;
    sint32 model_id;
    sint16 pose_enabled;
} MODEL_NODE;

typedef struct MODEL_DESC
{
    const uint8 *data;
    uint32 data_size;
    sint32 half_x;
    sint32 half_y; /* Original negative Y half extent */
    sint32 half_z;
    uint8 col;
    uint8 special;
} MODEL_DESC;
typedef struct MODEL MODEL;

typedef struct
{
    MODEL *recs;
    MODEL_DESC *descs;
    sint32 count, capacity;
    const uint8 *src;
    uint32 src_size;
} MODEL_CATALOG;

/* Selections are original scalar publications, distinct from catalog capacity */
typedef struct MODEL_SELECTIONS
{
    sint16 falling_hazard, target_indicator, robot_press, bio_goo, radar_target;
} MODEL_SELECTIONS;

extern MODEL_SELECTIONS g_model_selections;

extern MODEL_CATALOG g_model_catalog;

/* Draw and construction operations share the original current-model selection */
typedef struct MODEL_DRAW_STATE
{
    sint32 model_idx;
    sint32 world_x, world_y, world_z;
    sint32 translation_x, translation_z;
    sint32 rotation_x, rotation_y, rotation_z;
    sint32 shade, depth_bucket, vertical_cull_override;
    sint16 clut_override;
    RENDER_FRAME *target_frame;
} MODEL_DRAW_STATE;

extern MODEL_DRAW_STATE g_model_draw;
/* Original construction counters remain distinct from catalog count/capacity */
extern sint16 g_model_count, g_model_packet_count;

/* BEGIN GENERATED MODULE API */
GDB_CALL void model_stage_load(const char *path, sint32 size, sint16 *base, sint16 count, sint32 material1, sint32 material2, sint32 material3, const uint8 *data);
sint32 model_begin(const void *src, sint32 size, sint32 count);
sint32 model_reserve_base(sint32 count);
sint32 model_parse_and_build_packets(sint32 material1, sint32 material2, sint32 material3, sint32 material4, const sint32 *material_table, sint32 material_count);
void model_render(void);
void model_pose_apply(const MODEL_POSE_RESOURCE *resource, MODEL_NODE **nodes, sint32 node_count, sint32 pose);
void model_attach_child(MODEL_NODE *node, MODEL_NODE *parent);
void model_build_from_config(MODEL_NODE *nodes, const sint16 *cfg, sint16 model_base);
void model_pose_init(const MODEL_POSE_RESOURCE *resource, MODEL_NODE **nodes, sint32 node_count, sint32 pose);
void model_render_begin(void);
void model_render_end(void);
void model_render_node(MODEL_NODE *node, const MATRIX *parent);
void model_draw_cell(MAP_FLOOR_CELL *cell, sint32 cell_x, sint32 cell_z, sint32 row_bucket);
void model_draw_runtime_model(sint32 id, sint32 x, sint32 y, sint32 z, sint32 rot_y, sint32 rot_x, sint32 rot_z, sint32 row_bucket);
/* END GENERATED MODULE API */

/* Native stage-heap lifetime boundary */
void model_heap_reset(void);

#endif
