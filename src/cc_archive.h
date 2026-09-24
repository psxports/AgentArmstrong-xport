#ifndef CC_ARCHIVE_H
#define CC_ARCHIVE_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

typedef struct
{
    uint8 unknown_00[0x0c];
    sint32 entry_count; /* +0x0C */
} CC_ARCHIVE_HEADER;

typedef struct
{
    char name[16]; /* +0x00 */
    sint32 offset; /* +0x10 */
    sint32 size;   /* +0x14 */
} CC_ARCHIVE_ENTRY;

#if defined(AP_32BIT)
typedef char CC_ARCHIVE_HEADER_size_10[sizeof(CC_ARCHIVE_HEADER) == 0x10 ? 1 : -1];
typedef char CC_ARCHIVE_ENTRY_size_18[sizeof(CC_ARCHIVE_ENTRY) == 0x18 ? 1 : -1];
typedef char CC_ARCHIVE_ENTRY_offset_10[offsetof(CC_ARCHIVE_ENTRY, offset) == 0x10 ? 1 : -1];
typedef char CC_ARCHIVE_ENTRY_size_field_14[offsetof(CC_ARCHIVE_ENTRY, size) == 0x14 ? 1 : -1];
#endif

#endif
