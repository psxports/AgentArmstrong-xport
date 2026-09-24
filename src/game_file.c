#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "object.h"
#include "global.h"

/* Functions. */
static void normalize_path(const char *source, char *destination, sint32 size)
{
    sint32 i = 0;

    if (size <= 0)
    {
        return;
    }
    while (source[i] != '\0' && i + 1 < size)
    {
        if (source[i] == ';')
        { /* Strip the ISO-9660 version suffix, e.g. ;1. */
            break;
        }
        destination[i] = source[i] == '\\' ? '/' : source[i];
        ++i;
    }
    destination[i] = '\0';
}

static FILE *open_data_file(const char *path, const char *mode)
{
    char normalized[260];
    char candidate[520];
    const char *configured_root = getenv("OPENARMSTRONG_DATA");
    const char *relative;
    FILE *file;

    normalize_path(path, normalized, sizeof(normalized));
    relative = normalized;
    /* All original CD paths are relative to the disc's DATA directory in the
       Windows distribution.  Accept an already-prefixed path without adding
       DATA twice, but keep game code using the original PSX names. */
    if ((relative[0] == 'D' || relative[0] == 'd') && (relative[1] == 'A' || relative[1] == 'a') && (relative[2] == 'T' || relative[2] == 't') && (relative[3] == 'A' || relative[3] == 'a') && relative[4] == '/')
        relative += 5;
    if (configured_root != NULL && strlen(configured_root) + strlen(relative) + 7 < sizeof(candidate))
    {
        sprintf(candidate, "%s/DATA/%s", configured_root, relative);
        file = fopen(candidate, mode);
        if (file != NULL)
            return file;
    }
    if (strlen(relative) + 6 >= sizeof(candidate))
        return NULL;
    sprintf(candidate, "DATA/%s", relative);
    return fopen(candidate, mode);
}

static void *load_open_file(FILE *file, sint32 *size_out)
{
    sint32 length;
    void *data;

    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != NULL)
            fclose(file);
        return NULL;
    }
    length = ftell(file);
    if (length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return NULL;
    }
    data = malloc(length);
    if (data == NULL || (sint32)fread(data, 1, length, file) != length)
    {
        free(data);
        fclose(file);
        return NULL;
    }
    fclose(file);
    if (size_out != NULL)
        *size_out = length;
    return data;
}

void *app_file_load(const char *path, sint32 *size_out)
{
    char native_path[260];
    normalize_path(path, native_path, sizeof(native_path));
    return load_open_file(open_data_file(native_path, "rb"), size_out);
}

sint32 app_file_size(const char *path)
{
    char native_path[260];
    FILE *file;
    sint32 length;

    normalize_path(path, native_path, sizeof(native_path));
    file = open_data_file(native_path, "rb");
    if (file == NULL || fseek(file, 0, SEEK_END) != 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    length = (sint32)ftell(file);
    fclose(file);
    return length < 0 ? 0 : length;
}

sint32 app_file_read(const char *path, void *destination, sint32 capacity, sint32 *size_out)
{
    char native_path[260];
    FILE *file;
    sint32 size;

    normalize_path(path, native_path, sizeof(native_path));
    file = open_data_file(native_path, "rb");
    if (file == NULL || capacity < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    size = (sint32)fread(destination, 1, capacity, file);
    if (ferror(file))
    {
        fclose(file);
        return 0;
    }
    fclose(file);
    if (size_out != NULL)
        *size_out = size;
    return 1;
}

sint32 app_file_write(const char *path, const void *data, sint32 size)
{
    FILE *file = fopen(path, "wb");
    sint32 result;

    if (file == NULL || size < 0)
    {
        if (file != NULL)
            fclose(file);
        return 0;
    }
    result = fwrite(data, 1, (size_t)size, file) == (size_t)size;
    return fclose(file) == 0 && result;
}

void app_file_free(void *data)
{
    free(data);
}
