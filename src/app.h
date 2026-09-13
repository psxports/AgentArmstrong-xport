#ifndef APP_H
#define APP_H

#include "psx.h"

/* Port convenience cheat: expose every mission in the world-map selector. */
#define LEVEL_SELECT 1

typedef void (*CD_CALLBACK)(void);

/* Stable named entry points for CDB/GDB evidence probes in Debug builds.
 * Release builds carry no export or inlining restriction from this macro. */
#if defined(_DEBUG)
    #define GDB_CALL __declspec(dllexport) __declspec(noinline)
#else
    #define GDB_CALL
#endif

/* Types. */
enum
{
    SPATIAL_BUCKET_COUNT = 64,
    SPATIAL_BUCKET_CAPACITY = 24,
    FRAME_OBJECT_LIMIT = 256
};

typedef struct SpatialBucket
{
    uint8 count;
    uint8 reserved;
    sint16 cell_index;
    uint8 object_indices[SPATIAL_BUCKET_CAPACITY];
} SpatialBucket;

typedef char SpatialBucketSizeMustBe0x1c[sizeof(SpatialBucket) == 0x1c ? 1 : -1];

/* Runtime quad record produced by FUN_8008D108 and consumed by
 * FUN_8008E410/FUN_80094548/FUN_8008DDEC. */
typedef struct FrameObjectPartial
{
    void *prim;
    sint32 world_x;
    sint32 world_y;
    sint32 world_z;
    sint32 sort_x;
    sint16 ot_bucket;
    sint16 x_subcell_offset;
    sint16 draw_env_height;
    sint16 reserved_1a;
    sint32 model_plus_one;
    sint16 model_rot_y; /* +0x20 -> DAT_800D4204 */
    sint16 model_rot_x; /* +0x22 -> DAT_800D3F28 */
    sint16 model_rot_z; /* +0x24 -> DAT_800D3F38 */
    sint16 light_delta; /* +0x26, consumed by FUN_80094548 */
    uint8 drawn;        /* +0x28 */
    uint8 force_light;  /* +0x29 */
    uint8 reserved_2a[2];
} FrameObjectPartial;

#if defined(AP_32BIT)
typedef char FrameObjectSizeMustBe0x2c[sizeof(FrameObjectPartial) == 0x2c ? 1 : -1];
typedef char FrameObjectLightOffsetMustBe26[offsetof(FrameObjectPartial, light_delta) == 0x26 ? 1 : -1];
typedef char FrameObjectDrawnOffsetMustBe28[offsetof(FrameObjectPartial, drawn) == 0x28 ? 1 : -1];
typedef char FrameObjectForceLightOffsetMustBe29[offsetof(FrameObjectPartial, force_light) == 0x29 ? 1 : -1];
#endif

#endif
