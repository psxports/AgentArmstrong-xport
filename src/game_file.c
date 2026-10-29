#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_config.h"
#include "game_file.h"
#include "render.h"

/* Functions. */
static sint32 normalize_path(const char *src, char *dst, sint32 capacity)
{
    sint32 i = 0;
    while (src[i] != '\0' && src[i] != ';')
    {
        if (i + 1 >= capacity)
            return 0;
        dst[i] = src[i] == '\\' ? '/' : src[i];
        ++i;
    }
    dst[i] = '\0';
    return 1;
}

static FILE *open_data_file(const char *path, const char *mode)
{
    char n[260];
    char candidate[520];
    const char *configured_root = getenv("OPENARMSTRONG_DATA");
    const char *relative;
    FILE *file;

    if (!normalize_path(path, n, sizeof(n)))
        return NULL;
    relative = n;
    /* All original CD paths are relative to the disc's DATA directory in the
       Windows distribution.  Accept an already-prefixed path without adding
       DATA twice, but keep game code using the original PSX names. */
    if ((relative[0] == 'D' || relative[0] == 'd') && (relative[1] == 'A' || relative[1] == 'a') && (relative[2] == 'T' || relative[2] == 't') && (relative[3] == 'A' || relative[3] == 'a') && relative[4] == '/')
        relative += 5;
    if (configured_root != NULL && strlen(configured_root) + strlen(relative) + 7 < sizeof(candidate))
    {
        snprintf(candidate, sizeof(candidate), "%s/DATA/%s", configured_root, relative);
        file = xport_fopen(candidate, mode);
        if (file != NULL)
            return file;
    }
    if (strlen(relative) + 6 >= sizeof(candidate))
        return NULL;
    snprintf(candidate, sizeof(candidate), "DATA/%s", relative);
    return xport_fopen(candidate, mode);
}

/* Original: SLES_004.74:FUN_800A67A8. */
GDB_CALL sint32 game_file_size(const char *path)
{
    FILE *file;
    long len;

    file = open_data_file(path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    len = ftell(file);
    fclose(file);
    return len < 0 || len > INT32_MAX ? 0 : len;
}

static sint32 file_read_partial(const char *path, void *dst, sint32 capacity, sint32 *size_out)
{
    FILE *file;
    sint32 size;

    file = open_data_file(path, "rb");
    if (file == NULL || capacity < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    size = (sint32)fread(dst, 1, capacity, file);
#if defined(_DEBUG)
    if (ferror(file))
    {
        fclose(file);
        return 0;
    }
#endif
    fclose(file);
    if (size_out != NULL)
        *size_out = size;
    return 1;
}

sint32 app_file_write(const char *path, const void *data, sint32 size)
{
    FILE *file = xport_fopen(path, "wb");
    sint32 result;
    size_t write_size;

    if (file == NULL || size < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    write_size = size;
    result = fwrite(data, 1, write_size, file) == write_size;
    return fclose(file) == 0 && result;
}

/* Original: SLES_004.74:FUN_800A6A44. */
sint32 game_file_write(const char *path, const void *data, sint32 size)
{
    if (!app_file_write(path, data, size))
    {
        fatal_error("CANT SAVE");
        return 0;
    }
    return 1;
}

/* Extracted DATA files replace the original CD transport and retry loop */
/* Original: SLES_004.74:FUN_800A6658. */
GDB_CALL void game_file_read(const char *path, void *dst, sint32 capacity)
{
    sint32 expected = game_file_size(path);
    sint32 transferred = 0;
    XASSERT(!(capacity < 0 || expected > capacity));
    sint32 read_success = expected > 0 && file_read_partial(path, dst, expected, &transferred);
    XASSERT(read_success);
    (void)read_success;
    XASSERT(transferred == expected);
}

void *game_file_load(const char *path, sint32 *size_out)
{
    sint32 size = game_file_size(path);
    void *data;

    if (size <= 0)
        return 0;
    data = malloc(size);
    if (data == 0)
        return 0;
    game_file_read(path, data, size);
    if (size_out != 0)
        *size_out = size;
    return data;
}
