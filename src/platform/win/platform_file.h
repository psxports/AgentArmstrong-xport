#ifndef MODULE_API_PLATFORM_WIN_PLATFORM_FILE_H
#define MODULE_API_PLATFORM_WIN_PLATFORM_FILE_H

#include "app.h"

/* BEGIN GENERATED MODULE API */
sint32 app_file_read(const char *path, void *destination, sint32 capacity, sint32 *size_out);
sint32 app_file_size(const char *path);
sint32 app_file_write(const char *path, const void *data, sint32 size);
void *app_file_load(const char *path, sint32 *size_out);
void app_file_free(void *data);
/* END GENERATED MODULE API */

#endif
