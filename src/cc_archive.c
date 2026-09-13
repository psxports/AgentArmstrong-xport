#include <string.h>
#include "app.h"
#include "cc_archive.h"
#include "original_file.h"
#include "platform/win/platform_file.h"

/* Variables. */
static uint8 *archive_data;

static sint32 archive_size;

static CC_ARCHIVE_ENTRY *archive_entries;

static sint32 archive_entry_count;

/* Functions. */
static sint32 valid_range(sint32 offset, sint32 size)
{
    return offset >= 0 && size >= 0 && (sint32)offset <= archive_size && (sint32)size <= archive_size - (sint32)offset;
}

void cc_archive_close(void)
{
    app_file_free(archive_data);
    archive_data = 0;
    archive_size = 0;
    archive_entries = 0;
    archive_entry_count = 0;
}

sint32 cc_archive_open(const char *path)
{
    CC_ARCHIVE_HEADER *header;
    sint32 count;
    cc_archive_close();
    archive_data = (uint8 *)game_file_load(path, &archive_size);
    if (archive_data == 0 || archive_size < 16)
        return 0;

    header = (CC_ARCHIVE_HEADER *)archive_data;
    count = header->entry_count;
    if (count < 0 || count > 1024 || count > (archive_size - (sint32)sizeof(*header)) / (sint32)sizeof(CC_ARCHIVE_ENTRY))
    {
        cc_archive_close();
        return 0;
    }
    archive_entry_count = count;
    archive_entries = (CC_ARCHIVE_ENTRY *)(header + 1);
    return 1;
}

void *cc_archive_data(void)
{
    return archive_data;
}

void *cc_archive_find(const char *name, sint32 *size_out)
{
    sint32 i;
    for (i = 0; i < archive_entry_count; ++i)
    {
        CC_ARCHIVE_ENTRY *entry = archive_entries + i;
        if (strncmp(entry->name, name, sizeof(entry->name)) == 0 && valid_range(entry->offset, entry->size))
        {
            if (size_out != 0)
                *size_out = (sint32)entry->size;
            return archive_data + entry->offset;
        }
    }
    if (size_out != 0)
        *size_out = 0;
    return 0;
}
