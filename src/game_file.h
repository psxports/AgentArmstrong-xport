#ifndef GAME_FILE_H
#define GAME_FILE_H

#include "xport.h"

GDB_CALL sint32 game_file_size(const char *path);
GDB_CALL void game_file_read(const char *path, void *dst, sint32 capacity);
sint32 game_file_write(const char *path, const void *data, sint32 size);
void *game_file_load(const char *path, sint32 *size_out);
sint32 app_file_write(const char *path, const void *data, sint32 size);

#endif
