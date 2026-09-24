#ifndef AIRSHIP_H
#define AIRSHIP_H

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "effect_update.h"
#include "map.h"
#include "model.h"
#include "player.h"
#include "projectile.h"
#include "sprite.h"

/* BEGIN GENERATED MODULE API */
void airship_create(EFFECT *effect);
void airship_switch_create(EFFECT *effect);
void explosive_projectile_impact(PROJECTILE *projectile);
void explosive_projectile_trail_update(PROJECTILE *projectile);
void hierarchy_collision_box_update(COLLISION *object, MODEL_NODE *first_node, sint32 count);
void jetpack_enemy_create(EFFECT *effect);
void lift_create(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
