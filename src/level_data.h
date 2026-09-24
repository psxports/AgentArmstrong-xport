#ifndef MODULE_API_LEVEL_DATA_H
#define MODULE_API_LEVEL_DATA_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

/* Types. */
typedef union
{
    uint32 packed;

    struct
    {
        uint16 flags;
        sint16 music_id;
    } fields;
} STAGE_MUSIC_FLAGS;

typedef union
{
    uint32 packed;

    struct
    {
        sint16 light_bias;
        uint16 flags;
    } fields;
} STAGE_DISPLAY_FLAGS;

typedef struct
{
    const char *archive_path;      /* +0x00 */
    const sint32 *mission_script;  /* +0x04 */
    const sint32 *resource_script; /* +0x08 */
    sint32 *material_lookup;       /* +0x0C */
    sint32 background_resource;    /* +0x10 */
    sint32 background_y;           /* +0x14 */
    sint32 map_resource;           /* +0x18 */
    STAGE_MUSIC_FLAGS music;       /* +0x1C */
    STAGE_DISPLAY_FLAGS display;   /* +0x20 */
} STAGE_ENV;

/* View beginning at record +0x0C in the immutable 0x24-byte executable
 * stage table.  FUN_80095828 receives exactly this interior address. */
typedef struct
{
    uint32 material_table;      /* record +0x0C / view +0x00 */
    uint32 background_resource; /* record +0x10 / view +0x04 */
    sint32 background_y;        /* record +0x14 / view +0x08 */
    uint32 map_resource;        /* record +0x18 / view +0x0C */
    uint32 music_and_flags;     /* record +0x1C / view +0x10 */
    uint32 display_flags;       /* record +0x20 / view +0x14 */
} STAGE_ENV_RECORD_TAIL;

#if defined(AP_32BIT)
typedef char StageEnvironment_size_24[sizeof(STAGE_ENV) == 0x24 ? 1 : -1];
typedef char StageEnvironment_material_at_0c[offsetof(STAGE_ENV, material_lookup) == 0x0c ? 1 : -1];
typedef char StageEnvironment_map_at_18[offsetof(STAGE_ENV, map_resource) == 0x18 ? 1 : -1];
typedef char StageEnvironment_music_at_1e[offsetof(STAGE_ENV, music.fields.music_id) == 0x1e ? 1 : -1];
typedef char StageEnvironmentRecordTail_size_18[sizeof(STAGE_ENV_RECORD_TAIL) == 0x18 ? 1 : -1];
typedef char StageEnvironmentRecordTail_map_at_0c[offsetof(STAGE_ENV_RECORD_TAIL, map_resource) == 0x0c ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
void hq_assets_load(void);
/* END GENERATED MODULE API */

#endif
