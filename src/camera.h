#ifndef MODULE_API_CAMERA_H
#define MODULE_API_CAMERA_H

#include "xport.h"
#include "psx.h"
#include "effect_update.h"

void horizontal_camera_limit_create(EFFECT *src);
void vertical_camera_limit_create(EFFECT *src);
void room_depth_limit_create(void);
void room_camera_limits_update(void);

typedef struct CAMERA_STATE
{
    /* Original DAT_800D3FCC suppresses camera following across transitions */
    sint32 frozen;
    sint32 world_x, world_y, world_z;
    sint32 prev_x, prev_y, prev_z;
    sint32 render_x, render_y, render_z;
    sint32 limit_left, limit_right, limit_top, limit_bottom, limit_near, limit_far;
    sint32 target_y, depth_offset, depth_offset_target, manual_y_offset, horizontal_bias;
    sint32 room_activation_pending, room_player_near_z, room_player_far_z;
    sint32 player_near_z_limit, player_far_z_limit;
    /* Original shared player-motion snapshot, also written by player transitions */
    sint32 prev_player_x, prev_player_y, prev_player_z;
} CAMERA_STATE;

extern CAMERA_STATE g_camera;
/* Read-only view of the camera-owned field shared by void-floor comparisons */
extern const sint32 *const g_void_floor_material;

/* BEGIN GENERATED MODULE API */
void camera_update(sint32 player_x, sint32 player_y, sint32 player_z, sint32 mode);
void camera_depth_limit_set(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
