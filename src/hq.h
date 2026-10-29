#ifndef MODULE_API_HQ_H
#define MODULE_API_HQ_H

#include "xport.h"
#include "psx.h"
#include "effect_update.h"

extern uint8 g_hq_weapon_area_enabled;
void hq_heap_reset(void);

/* Types. */
enum
{
    HQ_MAP_NODE_COUNT = 65
};

typedef struct
{
    union
    {
        uint16 connection;
        sint16 signed_connection;
    };
    uint8 stage;
    uint8 region;
    sint16 x, y;
    sint16 neighbour[4];
    uint8 map_idx;
    uint8 unlocked;
    uint8 next_connection;
} HQ_MAP_NODE;

extern HQ_MAP_NODE g_hq_map_nodes[HQ_MAP_NODE_COUNT + 1];
extern sint16 g_hq_map_region;
extern sint16 g_hq_sel_map_node;

/* BEGIN GENERATED MODULE API */
GDB_CALL HQ_MAP_NODE *hq_world_map_node_find_by_id(sint16 id);
HQ_MAP_NODE *hq_world_map_node_find_by_stage(sint16 stage);
GDB_CALL sint16 hq_scrolling_text_render(const char *text, sint16 x, sint16 extent, const char *caption, sint32 automatic);
GDB_CALL sint16 text_line_break_count(const char *text);
GDB_CALL void hq_loading_scr_run(void);
GDB_CALL void hq_portrait_trigger_update(EFFECT *effect);
GDB_CALL void hq_world_map_relocate_nodes(void);
void hq_level_update(void);
/* END GENERATED MODULE API */

#endif
