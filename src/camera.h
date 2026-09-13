#ifndef MODULE_API_CAMERA_H
#define MODULE_API_CAMERA_H

#include "app.h"
#include "effect_update.h"

/* BEGIN GENERATED MODULE API */
void camera_update(sint32 player_x, sint32 player_y, sint32 player_z, sint32 mode, sint32 unchanged_direction_frames);
void camera_shake_start(void);
void camera_depth_limit_set(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
