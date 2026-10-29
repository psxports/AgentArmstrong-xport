#ifndef MODULE_API_RADAR_DEATH_H
#define MODULE_API_RADAR_DEATH_H

#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "effect_update.h"
#include "map.h"
#include "model.h"
#include "player.h"
#include "sprite.h"

typedef struct DESTRUCTIBLE_HIERARCHY
{
    COLLISION col;
    EFFECT *owner_effect;
    MAP_MODEL_RECORD *map_rec;
} DESTRUCTIBLE_HIERARCHY;

/* BEGIN GENERATED MODULE API */
void destructible_hierarchy_damage(void *raw_object, void *src);
void large_object_destruction_effect_create(COLLISION *object);
void radar_vehicle_create(EFFECT *effect);
/* END GENERATED MODULE API */

extern sint16 g_radar_vehicle_model_base;

extern MODEL_POSE_RESOURCE g_radar_initial_pose;

#endif
