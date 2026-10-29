#include <string.h>

#include "object.h"
#include "save.h"
#include "hq.h"
#include "xport_trace.h"

enum
{
    SAVE_VERSION = 1,
    SAVE_MISSION_LIMIT = 96,
    SAVE_HEADER_BYTES = 12,
    SAVE_BYTES = SAVE_HEADER_BYTES + SAVE_MISSION_LIMIT * 2
};

static uint8 save_data[SAVE_BYTES];

void game_progress_load(void)
{
    HQ_MAP_NODE *recs = g_hq_map_nodes;
    uint32 count;
    uint32 idx;
    uint32 rec_count = 0;
    uint16 sel;
    uint16 region;
    if (xport_load(save_data, SAVE_BYTES) != SAVE_BYTES)
        return;
    if (xport_load_le32(save_data) != 0x56534141u || xport_load_le16(save_data + 4) != SAVE_VERSION)
        return;
    count = xport_load_le16(save_data + 6);
    if (count > SAVE_MISSION_LIMIT)
        return;
    while (rec_count < HQ_MAP_NODE_COUNT && recs[rec_count].connection != 0xffffu)
        ++rec_count;
    if (recs[rec_count].connection != 0xffffu || rec_count != count)
        return;
    sel = xport_load_le16(save_data + 8);
    region = xport_load_le16(save_data + 10);
    if (sel >= rec_count || region != recs[sel].region)
        return;
    for (idx = 0; idx < count; ++idx)
    {
        uint8 stage = save_data[SAVE_HEADER_BYTES + idx * 2];
        uint8 unlocked = save_data[SAVE_HEADER_BYTES + idx * 2 + 1];
        if (stage != recs[idx].stage || unlocked > 2)
            return;
    }
    g_hq_sel_map_node = sel;
    g_hq_map_region = region;
    for (idx = 0; idx < count; ++idx)
        recs[idx].unlocked = save_data[SAVE_HEADER_BYTES + idx * 2 + 1];
}

void game_progress_save(void)
{
    HQ_MAP_NODE *rec = g_hq_map_nodes;
    sint32 count = 0;
    memset(save_data, 0, sizeof(save_data));
    while (rec->connection != 0xffffu && count < HQ_MAP_NODE_COUNT)
    {
        if (rec->unlocked > 2)
            return;
        save_data[SAVE_HEADER_BYTES + count * 2] = rec->stage;
        save_data[SAVE_HEADER_BYTES + count * 2 + 1] = rec->unlocked;
        ++rec;
        ++count;
    }
    if (rec->connection != 0xffffu || g_hq_sel_map_node < 0 || g_hq_sel_map_node >= count || g_hq_map_region != g_hq_map_nodes[g_hq_sel_map_node].region)
        return;
    xport_store_le32(save_data, 0x56534141u);
    xport_store_le16(save_data + 4, SAVE_VERSION);
    xport_store_le16(save_data + 6, count);
    xport_store_le16(save_data + 8, g_hq_sel_map_node);
    xport_store_le16(save_data + 10, g_hq_map_region);
    xport_save(save_data, SAVE_BYTES);
}
