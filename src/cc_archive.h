#ifndef CC_ARCHIVE_H
#define CC_ARCHIVE_H

#include "resource_table.h"

sint32 archive_validate(const void *data, sint32 size);
GDB_CALL RESOURCE_DATA archive_member_find(const char *name, const void *data, sint32 size);
void archive_resource_register(const char *name, uint32 resource, const void *archive, sint32 size);

#endif
