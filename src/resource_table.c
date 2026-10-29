#include <stdlib.h>
#include <string.h>

#include "render.h"
#include "resource_table.h"

typedef struct
{
    RESOURCE_DATA resource;
    uint8 owns_data;
} RESOURCE_SLOT;

static RESOURCE_SLOT resource_table[RESOURCE_SLOT_COUNT];

/* Original: SLES_004.74:FUN_8008CCE4. */
void resource_table_register(uint32 resource_id, void *data, uint32 size, RESOURCE_STORAGE storage)
{
    uint32 idx = resource_id >> 10;
    uint8 *copy = (uint8 *)data;
    RESOURCE_SLOT *slot;
    XASSERT(idx < RESOURCE_SLOT_COUNT);
    if (data != 0 && storage == RESOURCE_COPY)
    {
        copy = (uint8 *)malloc(size);
        XASSERT(!(copy == 0));
        memcpy(copy, data, size);
    }
    slot = &resource_table[idx];
    if (slot->owns_data)
        free(slot->resource.data);
    slot->resource.data = copy;
    slot->resource.size = data != 0 ? size : 0;
    slot->owns_data = data != 0 && storage == RESOURCE_COPY;
}

/* Original: FUN_8008CD00. */
uint8 *animation_frame_resource(uint32 resource_id)
{
    const RESOURCE_DATA *resource = resource_table_data(resource_id);
    uint32 slot = ((resource_id & 0x3ff) + 1) * 4;
    uint32 offset;
    if (resource == 0 || slot > resource->size || resource->size - slot < 4)
        return 0;
    offset = xport_load_le32(resource->data + slot);
    if (offset > resource->size - slot - 4)
        return 0;
    return resource->data + slot + offset;
}

const RESOURCE_DATA *resource_table_data(uint32 resource_id)
{
    uint32 idx = resource_id >> 10;
    if (idx >= RESOURCE_SLOT_COUNT || resource_table[idx].resource.data == 0)
        return 0;
    return &resource_table[idx].resource;
}
