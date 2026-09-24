#ifndef MODULE_API_STUBS_H
#define MODULE_API_STUBS_H

#include "xport.h"
#include "psx.h"
#include "render.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL char *integer_to_text(sint32 value);
GDB_CALL const char *loading_image_next_path(void);
GDB_CALL sint32 video_mode_set(sint32 value);
GDB_CALL void fatal_error(const char *message);
void fatal_error_with_value(const char *prefix, sint32 value);
GDB_CALL void music_track_select(sint32 id, sint32 unused);
GDB_CALL void sound_system_shutdown(void);
/* END GENERATED MODULE API */

#endif
