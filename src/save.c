#include <string.h>
#include "save.h"
#include "global.h"
#include "mission.h"
#include "player.h"
#include "xport.h"
#include "xport_trace.h"

enum
{
    SAVE_VERSION = 1,
    SAVE_MISSION_LIMIT = 96,
    SAVE_HEADER_BYTES = 12,
    SAVE_BYTES = SAVE_HEADER_BYTES + SAVE_MISSION_LIMIT * 2
};

static uint8 save_data[SAVE_BYTES];

static MissionRecord *mission_records(void)
{
    return (MissionRecord *)player_assets_executable_address(0x800c28e4u);
}

void game_progress_load(void)
{
    MissionRecord *records = mission_records();
    uint32 count;
    uint32 index;
    uint32 record_count = 0;
    uint16 selected;
    uint16 region;
    if (xport_load(save_data, sizeof(save_data)) != (sint32)sizeof(save_data))
        return;
    if (xport_load_le32(save_data) != 0x56534141u || xport_load_le16(save_data + 4) != SAVE_VERSION)
        return;
    count = xport_load_le16(save_data + 6);
    if (count > SAVE_MISSION_LIMIT)
        return;
    while (record_count < SAVE_MISSION_LIMIT && records[record_count].connection != 0xffffu)
        ++record_count;
    if (record_count == SAVE_MISSION_LIMIT || record_count != count)
        return;
    selected = xport_load_le16(save_data + 8);
    region = xport_load_le16(save_data + 10);
    if (selected >= record_count || region != records[selected].region)
        return;
    g_hq_selected_map_node = (sint16)selected;
    g_hq_map_region = (sint16)region;
    for (index = 0; index < count; ++index)
    {
        MissionRecord *record;
        uint8 stage = save_data[SAVE_HEADER_BYTES + index * 2];
        uint8 unlocked = save_data[SAVE_HEADER_BYTES + index * 2 + 1];
        if (unlocked > 2)
            continue;
        for (record = records; record->connection != 0xffffu; ++record)
            if (record->stage == stage)
            {
                record->unlocked = unlocked;
                break;
            }
    }
}

void game_progress_save(void)
{
    MissionRecord *record = mission_records();
    uint32 count = 0;
    memset(save_data, 0, sizeof(save_data));
    while (record->connection != 0xffffu && count < SAVE_MISSION_LIMIT)
    {
        save_data[SAVE_HEADER_BYTES + count * 2] = record->stage;
        save_data[SAVE_HEADER_BYTES + count * 2 + 1] = record->unlocked;
        ++record;
        ++count;
    }
    if (record->connection != 0xffffu)
        return;
    xport_store_le32(save_data, 0x56534141u);
    xport_store_le16(save_data + 4, SAVE_VERSION);
    xport_store_le16(save_data + 6, count);
    xport_store_le16(save_data + 8, (uint16)g_hq_selected_map_node);
    xport_store_le16(save_data + 10, (uint16)g_hq_map_region);
    xport_save(save_data, sizeof(save_data));
}
