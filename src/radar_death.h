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

typedef struct DESTRUCTIBLE_HIERARCHY DESTRUCTIBLE_HIERARCHY;

/* BEGIN GENERATED MODULE API */
void destructible_hierarchy_damage(DESTRUCTIBLE_HIERARCHY *object, void *source);
void large_object_destruction_effect_create(COLLISION *object);
void radar_vehicle_create(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
