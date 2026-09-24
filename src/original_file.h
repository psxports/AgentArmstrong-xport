#ifndef MODULE_API_ORIGINAL_FILE_H
#define MODULE_API_ORIGINAL_FILE_H

#include "xport.h"
#include "psx.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL sint32 game_file_size(const char *path);
GDB_CALL void game_file_read(const char *path, void *destination);
sint32 game_file_write(const char *path, const void *data, sint32 size);
void *game_file_load(const char *path, sint32 *size_out);
void *game_file_load_sector_buffer(const char *path, sint32 *capacity_out);
/* END GENERATED MODULE API */

#endif
