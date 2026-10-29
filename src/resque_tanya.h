#ifndef MODULE_API_RESQUE_TANYA_H
#define MODULE_API_RESQUE_TANYA_H

#include "xport.h"
#include "effect_update.h"
#include "model.h"

/* BEGIN GENERATED MODULE API */
void tanya_create(EFFECT *effect);
void tanya_gun_emplacement_create(EFFECT *effect);
/* END GENERATED MODULE API */

extern sint16 g_gun_emplacement_model_base;

extern MODEL_POSE_RESOURCE g_gun_emplacement_initial_pose;

#endif
