#ifndef MODULE_API_COLLISION_H
#define MODULE_API_COLLISION_H

#include <stddef.h>

#include "app.h"

/* Types. */
typedef void (*FUNC_COLLISION_UPDATE)(void *object);
typedef void (*FUNC_COLLISION_CALLBACK)(void *object, void *source);

/* Common object prefix consumed by FUN_80098588/FUN_8009875C.  Concrete
 * actor/effect allocations extend this prefix with type-specific storage. */
typedef struct
{
    void *previous;                      /* +0x00 */
    void *next;                          /* +0x04 */
    uint32 field_08;                     /* +0x08 */
    FUNC_COLLISION_UPDATE update;        /* +0x0C */
    FUNC_COLLISION_CALLBACK callback_10; /* +0x10 */
    FUNC_COLLISION_CALLBACK callback_14; /* +0x14 */
    FUNC_COLLISION_CALLBACK callback_18; /* +0x18 */
    sint16 object_type;                  /* +0x1C */
    uint16 field_1e;                     /* +0x1E */
    sint32 x, y, z;                      /* +0x20..+0x28 */
    sint32 box_x, box_y, box_z;          /* +0x2C..+0x34 */
    sint32 width, height, depth;         /* +0x38..+0x40 */
    uint8 prim[0x28];                    /* +0x44, POLY_FT4 packet bytes */
    void *frame_descriptor;              /* +0x6C */
    uint32 receives_mask;                /* +0x70 */
    uint32 sends_mask;                   /* +0x74 */
} COLLISION;

typedef void (*COLLISION_CALLBACK)(COLLISION *object, COLLISION *source);

typedef struct
{
    sint32 x;     /* +0x00 */
    sint32 y;     /* +0x04 */
    sint32 z;     /* +0x08 */
    sint32 flags; /* +0x0C */
    sint32 step;  /* +0x10 */
} COLLISION_RESULT;

#if defined(AP_32BIT)
    #define COLLISION_OFFSET_ASSERT(field, offset) typedef char CollisionObject_##field##_at_##offset[(offsetof(COLLISION, field) == 0x##offset) ? 1 : -1]
COLLISION_OFFSET_ASSERT(update, 0c);
COLLISION_OFFSET_ASSERT(callback_14, 14);
COLLISION_OFFSET_ASSERT(callback_18, 18);
COLLISION_OFFSET_ASSERT(object_type, 1c);
COLLISION_OFFSET_ASSERT(x, 20);
COLLISION_OFFSET_ASSERT(box_x, 2c);
COLLISION_OFFSET_ASSERT(width, 38);
COLLISION_OFFSET_ASSERT(prim, 44);
COLLISION_OFFSET_ASSERT(frame_descriptor, 6c);
COLLISION_OFFSET_ASSERT(receives_mask, 70);
COLLISION_OFFSET_ASSERT(sends_mask, 74);
    #undef COLLISION_OFFSET_ASSERT
#endif
typedef char CollisionResult_size_14[(sizeof(COLLISION_RESULT) == 0x14) ? 1 : -1];

/* Types. */
typedef struct
{
    void *previous; /* +0x00 */
    void *next;     /* +0x04 */
    uint8 field_08[8];
    sint32 top_y;    /* +0x10 */
    sint32 bottom_y; /* +0x14 */
    uint8 field_18[8];
    sint32 min_x; /* +0x20 */
    uint8 field_24[4];
    sint32 max_x; /* +0x28 */
    uint8 field_2c[12];
    sint32 min_z;     /* +0x38 */
    sint32 max_z;     /* +0x3C */
    sint16 attribute; /* +0x40 */
    uint8 field_42[2];
} DYNAMIC_COLLISION_NODE;

typedef char DynamicCollisionNode_size_44[sizeof(DYNAMIC_COLLISION_NODE) == 0x44 ? 1 : -1];

/* Types. */
/* Common prefix consumed by resident flash-palette helper FUN_80096B58. */
typedef struct
{
    COLLISION collision;     /* +0x00 */
    sint16 field_78;         /* +0x78 */
    sint16 flash_clut_ticks; /* +0x7A */
} FLASHABLE;

#if defined(AP_32BIT)
typedef char FlashableObject_size_7c[sizeof(FLASHABLE) == 0x7c ? 1 : -1];
typedef char FlashableObject_ticks_at_7a[offsetof(FLASHABLE, flash_clut_ticks) == 0x7a ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
GDB_CALL void collision_box_set(COLLISION *object, sint32 width, sint32 height, sint32 depth);
sint16 object_damage_apply(FLASHABLE *target, FLASHABLE *source);
void object_collisions_dispatch(void);
/* END GENERATED MODULE API */

#endif
