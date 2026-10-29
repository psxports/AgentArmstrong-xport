#ifndef GUNSHIP_H
#define GUNSHIP_H

#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "effect_update.h"
#include "model.h"
#include "projectile.h"
#include "sprite.h"

/* BEGIN GENERATED MODULE API */
void gunship_create(EFFECT *e);
/* END GENERATED MODULE API */

extern sint16 g_gunship_model_base;

extern MODEL_POSE_RESOURCE g_gunship_initial_pose;

#endif
