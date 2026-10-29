#ifndef MODULE_API_COLLISION_H
#define MODULE_API_COLLISION_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"
#include "object.h"

struct VRAM_SPRITE;
struct MODEL_NODE;

/* Types. */
typedef void (*FUNC_COLLISION_CALLBACK)(void *object, void *src);
typedef void *(*FUNC_COLLISION_OBJECT_CALLBACK)(void *object, void *src);

typedef enum
{
    COLLISION_CALLBACK_NOTIFY,
    COLLISION_CALLBACK_OBJECT
} COLLISION_CALLBACK_KIND;

/* Native collision prefix shared by actors and effects */
typedef struct
{
    OBJECT object;
    /* Player contact includes damage, pickups and scripted interaction */
    FUNC_COLLISION_CALLBACK player_contact_callback;
    /* Hits include projectiles, explosions and damaging particles */
    COLLISION_CALLBACK_KIND hit_callback_kind;

    union
    {
        FUNC_COLLISION_CALLBACK hit_callback;
        FUNC_COLLISION_OBJECT_CALLBACK hit_object_callback;
    };

    sint16 object_type;
    sint32 x, y, z;
    sint32 box_x, box_y, box_z;
    sint32 width, height, depth;
    POLY_FT4 prim;
    struct VRAM_SPRITE *frame_desc;
    uint32 receives_mask;
    uint32 sends_mask;

    union
    {
        sint16 health;
        sint16 damage;
    } hit;

    sint16 flash_clut_ticks;
} COLLISION;

typedef struct
{
    sint32 x;     /* Legacy +0x00 */
    sint32 y;     /* Legacy +0x04 */
    sint32 z;     /* Legacy +0x08 */
    sint32 flags; /* Legacy +0x0C */
    sint32 step;  /* Legacy +0x10 */
} COLLISION_RESULT;

/* Types. */
typedef struct DYNAMIC_COLLISION_NODE
{
    struct DYNAMIC_COLLISION_NODE *prev, *next;
    sint32 top_y, bottom_y;
    sint32 min_x, max_x, min_z, max_z;
    sint16 attribute;
} DYNAMIC_COLLISION_NODE;

typedef struct
{
    DYNAMIC_COLLISION_NODE *head, *tail;
} DYNAMIC_COLLISION_ANCHOR;

typedef struct DYNAMIC_COLLISION_GRID
{
    DYNAMIC_COLLISION_ANCHOR *cells;
    sint32 count;
} DYNAMIC_COLLISION_GRID;

extern DYNAMIC_COLLISION_GRID g_dynamic_col_grid;
extern sint32 g_col_surface_resource;
void col_heap_reset(void);

/* Types. */
/* Common prefix consumed by resident flash-palette helper FUN_80096B58. */

/* BEGIN GENERATED MODULE API */
void hierarchy_col_box_update(COLLISION *object, const struct MODEL_NODE *first_node, sint16 count);
GDB_CALL void col_box_set(void *object_val, sint32 width, sint32 height, sint32 depth);
sint16 object_damage_apply(void *target_val, void *src_val);
sint16 swept_col_test(sint32 old_x, sint32 old_y, sint32 old_z, sint32 *new_x, sint32 *new_y, sint32 *new_z);
void object_cols_dispatch(void);
/* END GENERATED MODULE API */

extern COLLISION_RESULT *g_cur_col_result;
extern sint32 g_col_flags, g_dynamic_col_hit;

COLLISION_RESULT *static_map_col_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 new_x, sint32 new_y, sint32 new_z, sint32 width, sint32 height, sint32 depth);
uint32 map_motion_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 *x, sint32 *y, sint32 *z, sint32 *velocity_x, sint32 *velocity_y, sint32 width, sint32 height, sint32 depth);

sint16 map_environment_damage_test(sint32 world_x, sint32 world_y, sint32 world_z, sint32 width, sint32 height, sint32 depth);

#endif
