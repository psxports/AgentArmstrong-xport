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
void explosive_projectile_impact(void *raw_projectile);
void explosive_projectile_trail_update(void *raw_projectile);
void jetpack_enemy_create(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
