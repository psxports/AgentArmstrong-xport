#ifndef MODULE_API_STUBS_H
#define MODULE_API_STUBS_H

#include "app.h"
#include "render.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL char *integer_to_text(sint32 value);
GDB_CALL const char *loading_image_next_path(void);
GDB_CALL sint32 video_mode_set(sint32 value);
GDB_CALL void fatal_error(const char *message);
void fatal_error_with_value(const char *prefix, sint32 value);
GDB_CALL void music_track_select(sint32 id, sint32 unused);
GDB_CALL void spu_shutdown(void);
sint32 game_psx_draw_sync(void *user, sint32 mode);
void game_psx_sound_initialize(void *user);
void game_psx_sound_set_master_volume(void *user, sint16 left, sint16 right);
void game_psx_sound_set_serial_attributes(void *user, sint8 serial, sint8 attribute, sint8 value);
void game_psx_sound_set_serial_volume(void *user, sint8 serial, sint16 left, sint16 right);
void game_psx_sound_set_tick_mode(void *user, sint32 mode);
void game_psx_sound_shutdown(void *user);
void game_psx_sound_start(void *user);
/* END GENERATED MODULE API */

#endif
