#include <string.h>

#include "cc_archive.h"
#include "xport_trace.h"
#include "resource_table.h"

/* Validate serialized CC directory entries before resolving native references */
sint32 archive_validate(const void *data, sint32 size)
{
    const uint8 *bytes = (const uint8 *)data;
    uint32 count, idx;
    if (bytes == 0 || size < 16)
        return 0;
    uint32 capacity = size;
    count = xport_load_le32(bytes + 12);
    if (count > (capacity - 16) / 24)
        return 0;
    for (idx = 0; idx < count; ++idx)
    {
        const uint8 *entry = bytes + 16 + idx * 24;
        uint32 offset = xport_load_le32(entry + 16);
        uint32 len = xport_load_le32(entry + 20);
        if (memchr(entry, 0, 16) == 0 || offset > capacity || len > capacity - offset)
            return 0;
    }
    return 1;
}

/* Original: SLES_004.74:FUN_800A6ABC. */
GDB_CALL RESOURCE_DATA archive_member_find(const char *name, const void *data, sint32 size)
{
    const uint8 *bytes = (const uint8 *)data;
    uint32 count, idx;
    RESOURCE_DATA member = {0, 0};
    if (name == 0 || !archive_validate(data, size))
        return member;
    count = xport_load_le32(bytes + 12);
    for (idx = 0; idx < count; ++idx)
    {
        const uint8 *entry = bytes + 16 + idx * 24;
        if (strcmp(name, (const char *)entry) != 0)
            continue;
        member.data = (uint8 *)data + xport_load_le32(entry + 16);
        member.size = xport_load_le32(entry + 20);
        return member;
    }
    return member;
}

/* Original: SLES_004.74:FUN_800A6A0C. */
void archive_resource_register(const char *name, uint32 resource, const void *archive, sint32 size)
{
    RESOURCE_DATA member = archive_member_find(name, archive, size);
    resource_table_register(resource, member.data, member.size, RESOURCE_COPY);
}
