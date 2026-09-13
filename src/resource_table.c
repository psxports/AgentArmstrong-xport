#include <stddef.h>
#include "app.h"
#include "resource_table.h"

/* Variables. */
/* Address-based resource registry recovered from FUN_8008CCE4/FUN_8008CD00. */

static uint8 *resource_table[1024];

/* Functions. */
/* Original: SLES_004.74:FUN_8008CCE4.
 * Also used for host relocation, formerly resource_table_set_builtin;
 * that identical host helper had no separate original entry address. */
void resource_table_register(uint32 resource_id, void *data)
{
    uint32 index = resource_id >> 10;
    if (index < sizeof(resource_table) / sizeof(resource_table[0]))
        resource_table[index] = (uint8 *)data;
}

/* Original: FUN_8008CD00. */
uint8 *animation_frame_resource(uint32 resource_id)
{
    uint8 *base = resource_table[resource_id >> 10];
    uint8 *entry;
    uint32 offset;

    if (base == NULL)
        return NULL;
    entry = base + (resource_id & 0x3ff) * 4 + 4;
    offset = (uint32)entry[0] | (uint32)entry[1] << 8 | (uint32)entry[2] << 16 | (uint32)entry[3] << 24;
    return entry + offset;
}

uint8 *resource_table_data(uint32 resource_id)
{
    return resource_table[resource_id >> 10];
}
