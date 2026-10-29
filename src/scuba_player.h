#ifndef SCUBA_PLAYER_H
#define SCUBA_PLAYER_H

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "effect_update.h"
#include "model.h"
#include "player.h"
#include "sprite.h"

SPRITE *scuba_projectile_create(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, sint32 lifetime);

extern const sint8 g_scuba_mini_gyro_model_offsets[8];

void *scuba_death_particle_emit(void *raw_emitter);
void scuba_blast_effect_create(COLLISION *src);

/* BEGIN GENERATED MODULE API */
void scuba_submarine_create(EFFECT *effect);
void scuba_drifting_hazard_create(EFFECT *effect);
void scuba_mine_create(EFFECT *effect);
void scuba_tug_target_create(EFFECT *effect);
void scuba_bubble_emitter_update(EFFECT *effect);
void scuba_diver_enemy_create(EFFECT *effect);
void scuba_noop_effect_update(EFFECT *effect);
void scuba_ship_target_create(EFFECT *effect);
void scuba_mission_complete_player_update(void *raw_player);
void scuba_model_target_create(sint32 x, sint32 y, sint32 z, sint32 model);
void scuba_player_create(sint32 x, sint32 y, sint32 z);
/* END GENERATED MODULE API */

extern sint16 g_mini_sub_model_base;

extern MODEL_POSE_RESOURCE g_mini_sub_initial_pose;

extern sint16 g_depth_charge_model_id;

extern sint16 g_scuba_tug_model_base;

extern MODEL_POSE_RESOURCE g_scuba_tug_initial_pose;

#endif
