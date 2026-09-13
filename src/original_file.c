#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "global.h"
#include "object.h"
#include "platform/win/platform_file.h"
#include "stubs.h"

/* Types. */
/* PAL game-side file wrappers at 0x800A6658..0x800A683C and
 * 0x800A9AFC..0x800A9F5C.  Only the sector transfer itself is replaced by
 * the agreed synchronous platform file boundary. */

typedef struct OriginalCdFile
{
    uint32 sector;
    sint32 size;
    uint32 reserved[4];
} OriginalCdFile;

typedef struct OriginalCdCacheEntry
{
    char name[24];
    OriginalCdFile file;
} OriginalCdCacheEntry;

/* Variables. */
static OriginalCdCacheEntry cd_cache[150];

static sint32 cd_cache_count;

/* Functions. */
static void make_cd_path(char *destination, const char *path)
{
    destination[0] = '\\';
    destination[1] = '\0';
    strcat(destination, path);
    strcat(destination, ";1");
}

/* FUN_800A9DA8.  The 150 records have the original 0x30-byte layout: a
 * 24-byte ISO name followed by the 24-byte CdlFILE result. */
void *cd_file_search_cached(OriginalCdFile *file, const char *path)
{
    sint32 index;
    sint32 size;

    if (cd_cache_count == 0)
    {
        for (index = 0; index < 150; ++index)
            cd_cache[index].name[0] = 0;
    }
    for (index = 0; index < cd_cache_count; ++index)
    {
        if (strcmp(path, cd_cache[index].name) == 0)
        {
            *file = cd_cache[index].file;
            ++g_cd_read_command_count;
            return file;
        }
    }
    if (index == 150)
        fatal_error("CDSEARCHFILE2");
    do
    {
        size = app_file_size(path);
    } while (size <= 0);
    memset(file, 0, sizeof(*file));
    file->size = size;
    if (cd_cache_count < 150)
    {
        strcpy(cd_cache[cd_cache_count].name, path);
        cd_cache[cd_cache_count].file = *file;
        ++cd_cache_count;
    }
    return file;
}

/* FUN_800A9BF4. */
sint32 cd_file_size(const char *path)
{
    OriginalCdFile file;

    return cd_file_search_cached(&file, path) != 0 ? file.size : 0;
}

/* FUN_800A9AFC.  The PAL routine returns the CdlFILE size after all sectors
 * have completed; zero denotes search/read failure. */
sint32 cd_file_read(const char *path, void *destination)
{
    OriginalCdFile file;
    sint32 transferred = 0;
    sint32 sectors;

    if (cd_file_search_cached(&file, path) == 0)
        return 0;
    if (!app_file_read(path, destination, file.size, &transferred) || transferred != file.size)
        return 0;
    sectors = (file.size + 0x7ff) >> 11;
    g_cd_sectors_read += sectors;
    return file.size;
}

/* FUN_800A67A8. */
GDB_CALL sint32 game_file_size(const char *path)
{
    char cd_path[260];

    if (g_cd_file_io_enabled == 0)
        return app_file_size(path);
    make_cd_path(cd_path, path);
    return cd_file_size(cd_path);
}

/* FUN_800A6A44.  PCcreat/PCwrite/PCclose are the SN Systems host-I/O
 * boundary in the PAL executable; app_file_write is its native equivalent. */
sint32 game_file_write(const char *path, const void *data, sint32 size)
{
    if (!app_file_write(path, data, size))
    {
        fatal_error("CANT SAVE");
        return 0;
    }
    return 1;
}

/* FUN_800A6658.  As on PSX, a CD read failure reports the filename and
 * retries forever; successful transfers are synchronous on the host. */
GDB_CALL void game_file_read(const char *path, void *destination)
{
    sint32 expected = game_file_size(path);

    if (g_cd_file_io_enabled == 0)
    {
        sint32 transferred = 0;
        if (!app_file_read(path, destination, expected, &transferred))
        {
            fatal_error("CANT OPEN");
        }
        else if (transferred != expected)
        {
            fatal_error("LENGTH ERROR");
        }
        return;
    }
    {
        char cd_path[260];
        make_cd_path(cd_path, path);
        while (cd_file_read(cd_path, destination) == 0)
            fatal_error(path);
    }
}

void *game_file_load(const char *path, sint32 *size_out)
{
    sint32 size = game_file_size(path);
    void *data;

    if (size <= 0)
        return 0;
    data = malloc((uint32)size);
    if (data == 0)
        return 0;
    game_file_read(path, data);
    if (size_out != 0)
        *size_out = size;
    return data;
}

/* Host-owned counterpart of A67A8 -> C680 -> A6658, for consumers which
 * retain the complete CD transfer buffer instead of shrinking it to EOF.
 * File length and allocation capacity are deliberately distinct. */
void *game_file_load_sector_buffer(const char *path, sint32 *capacity_out)
{
    sint32 size = game_file_size(path);
    sint32 capacity;
    void *data;
    if (size <= 0)
        return 0;
    capacity = (size + 0x7ff) & ~0x7ff;
    data = calloc(1, (uint32)capacity);
    if (data == 0)
        return 0;
    game_file_read(path, data);
    if (capacity_out != 0)
        *capacity_out = capacity;
    return data;
}
