#ifndef MODULE_API_V2_H
#define MODULE_API_V2_H

#include "xport.h"
#include "effect_update.h"
#include "model.h"

/* BEGIN GENERATED MODULE API */
void v2_expl_sequence_begin(void);
void v2_radar_marker_create(EFFECT *effect);
void v2_rocket_create(EFFECT *effect);
/* END GENERATED MODULE API */

extern sint16 g_v2_rocket_model_base;

extern MODEL_POSE_RESOURCE g_v2_rocket_init_pose;

#endif
