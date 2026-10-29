#ifndef MODULE_API_LEVEL_DATA_H
#define MODULE_API_LEVEL_DATA_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"
#include "resource_script.h"
#include "map.h"

struct MISSION_OBJECTIVE_SCRIPT;

enum
{
    STAGE_COUNT = 33
};

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
    const char *archive_path;
    const struct MISSION_OBJECTIVE_SCRIPT *mission_script;
    const RESOURCE_COMMAND *resource_script;
    sint32 *material_lookup;
    sint32 background_resource;
    sint32 background_y;
    sint32 map_resource;
    STAGE_MUSIC_FLAGS music;
    STAGE_DISPLAY_FLAGS disp;
    const char *background_tile;
} STAGE_ENV;

extern const STAGE_ENV *g_stage_env;
extern sint32 g_material_resource_table[64];
extern const uint32 g_hq_material_resources[46];

/* BEGIN GENERATED MODULE API */
void hq_assets_load(void);
/* END GENERATED MODULE API */

extern MAP_INPUT g_stage_map_input;
extern void *g_stage_archive;
extern sint32 g_stage_archive_size;

/* Native stage-heap lifetime boundary */
void level_data_heap_reset(void);

#endif
