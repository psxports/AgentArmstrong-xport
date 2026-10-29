#ifndef MODULE_API_GAME_LOOP_H
#define MODULE_API_GAME_LOOP_H

#include "xport.h"
#include "psx.h"
#include "level_data.h"
#include "mission.h"
#include "render.h"

extern const char g_loading_caption_spanish[];
GDB_CALL const char *loading_image_next_path(void);

/* BEGIN GENERATED MODULE API */
GDB_CALL void loading_caption_show(const char *text);
GDB_CALL void loading_image_show(const char *path, const char *text);
GDB_CALL void startup_warning_show(void);
void mission_game_loop(void);
/* END GENERATED MODULE API */

#endif
