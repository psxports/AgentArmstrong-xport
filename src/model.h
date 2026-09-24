#ifndef MODEL_H
#define MODEL_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"
#include "map.h"

typedef struct
{
    void *next_sibling;      /* +0x00 */
    void *previous_sibling;  /* +0x04 */
    void *first_child;       /* +0x08 */
    void *parent;            /* +0x0C */
    MATRIX local_matrix;     /* +0x10 */
    sint32 world_x;          /* +0x30 */
    sint32 world_y;          /* +0x34 */
    sint32 world_z;          /* +0x38 */
    sint16 world_rotation_x; /* +0x3C */
    sint16 world_rotation_y; /* +0x3E */
    sint16 world_rotation_z; /* +0x40 */
    sint16 rotation_x;       /* +0x42 */
    sint16 rotation_y;       /* +0x44 */
    sint16 rotation_z;       /* +0x46 */
    sint32 translation_x;    /* +0x48 */
    sint32 translation_y;    /* +0x4C */
    sint32 translation_z;    /* +0x50 */
    sint16 scale;            /* +0x54 */
    sint16 field_56;         /* +0x56 */
    sint32 model_id;         /* +0x58 */
    sint16 render_flags;     /* +0x5C */
    sint16 field_5e;         /* +0x5E */
} MODEL_NODE;

#if defined(AP_32BIT)
typedef char ModelHierarchyNode_size_60[sizeof(MODEL_NODE) == 0x60 ? 1 : -1];
typedef char ModelHierarchyNode_world_x_at_30[offsetof(MODEL_NODE, world_x) == 0x30 ? 1 : -1];
typedef char ModelHierarchyNode_rotation_x_at_42[offsetof(MODEL_NODE, rotation_x) == 0x42 ? 1 : -1];
typedef char ModelHierarchyNode_model_id_at_58[offsetof(MODEL_NODE, model_id) == 0x58 ? 1 : -1];
#endif
typedef struct OriginalModelDescriptor
{
    const void *model_data; /* +0x00 */
    sint32 half_x;          /* +0x04 */
    sint32 half_y;          /* +0x08: original negative Y half extent */
    sint32 half_z;          /* +0x0c */
    uint8 collision;        /* +0x10 */
    uint8 special;          /* +0x11 */
    uint8 reserved[2];      /* +0x12 */
} OriginalModelDescriptor;
#if defined(AP_32BIT)
typedef char OriginalModelDescriptorSize[(sizeof(OriginalModelDescriptor) == 20) ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
sint32 model_append(const void *source, sint32 size, sint32 count, sint32 material1, sint32 material2, sint32 material3);
sint32 model_begin(const void *source, sint32 size, sint32 count);
sint32 model_dump_parser_audit(const char *path);
sint32 model_reserve_base(sint32 count);
void *model_parse_and_build_packets(sint32 material1, sint32 material2, sint32 material3, sint32 material4, const sint32 *material_table);
void model_render(void);
void model_apply_pose(const void *raw_resource, MODEL_NODE **nodes, sint32 pose);
void model_attach_child(MODEL_NODE *node, MODEL_NODE *parent);
void model_initialize_pose(const void *raw_resource, MODEL_NODE **nodes, sint32 pose);
void model_render_begin(void);
void model_render_end(void);
void model_render_node(MODEL_NODE *node, MATRIX *parent);
void model_draw_cell(const MAP_MODEL_GROUP *g, sint32 cell_x, sint32 cell_z, sint32 row_bucket);
void model_draw_runtime_model(sint32 id, sint32 x, sint32 y, sint32 z, sint32 rot_y, sint32 rot_x, sint32 rot_z, sint32 row_bucket);
void model_select_source(const void *source, sint32 size);
void model_set_screen_offset(sint32 x, sint32 y);
/* END GENERATED MODULE API */

#endif
