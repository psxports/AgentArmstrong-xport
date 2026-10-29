#ifndef MODULE_API_PROJECTILE_H
#define MODULE_API_PROJECTILE_H

#include <stddef.h>

#include "animation.h"
#include "game_math.h"
#include "collision.h"
#include "player.h"
#include "sprite.h"

extern const ANIM_COMMAND g_bullet_impact_animation[8];
extern const ANIM_COMMAND g_expl_impact_animation[14];
extern const ANIM_COMMAND g_surface_impact_animation[16];
extern const ANIM_COMMAND g_surface_impact_slow_animation[16];
extern const ANIM_COMMAND g_small_surface_impact_animation[7];
extern const ANIM_COMMAND g_spark_animation[3];
extern const ANIM_COMMAND g_dust_animation0[14];
extern const ANIM_COMMAND g_dust_animation1[8];

extern sint16 g_surface_impact_cooldown;

/* Types. */
/* Original A62AC projectile owns DDA position and step through its lifetime */
typedef struct
{
    COLLISION col;
    FIXED_DDA_LINE motion;
    ANIM animation;
    sint32 wind_enabled;
    sint32 probe_mode;
    FUNC_OBJECT_UPDATE floor_callback;
    FUNC_OBJECT_UPDATE impact_callback;
    sint16 scale;
} PROJECTILE;

/* BEGIN GENERATED MODULE API */
PROJECTILE *projectile_create(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1);
sint32 projectile_test_path(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, sint32 steps);
void dust_effect_create(sint32 x, sint32 y, sint32 z);
SPRITE *expl_flash_create(sint32 x, sint32 y, sint32 z);
SPRITE *surface_impact_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation);
SPRITE *thrown_explosive_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation);
SPRITE *world_spr_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation);
void homing_projectile_steer(void *raw_object);
void projectile_trail_emit(void *raw_object);
/* END GENERATED MODULE API */

#endif
