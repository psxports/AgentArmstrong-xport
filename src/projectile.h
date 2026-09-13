#ifndef MODULE_API_PROJECTILE_H
#define MODULE_API_PROJECTILE_H

#include <stddef.h>

#include "animation.h"
#include "collision.h"
#include "player.h"
#include "sprite.h"

/* Types. */
/* 0xC4 projectile/probe allocated by FUN_800A62AC. The six-word motion
 * buffer remains neutral: FUN_8009EE30 initializes all six words and the
 * update consumes both halves in distinct roles. */
typedef struct
{
    COLLISION collision;                   /* +0x00 */
    sint16 damage;                         /* +0x78 */
    sint16 flash_clut_ticks;               /* +0x7A */
    sint32 motion[6];                      /* +0x7C */
    ANIM animation;                        /* +0x94 */
    sint32 wind_enabled;                   /* +0xB0 */
    sint32 probe_mode;                     /* +0xB4 */
    FUNC_COLLISION_UPDATE floor_callback;  /* +0xB8 */
    FUNC_COLLISION_UPDATE impact_callback; /* +0xBC */
    sint16 scale;                          /* +0xC0 */
    uint16 field_c2;                       /* +0xC2 */
} PROJECTILE;

#if defined(AP_32BIT)
    #define LINE_PROJECTILE_OFFSET_ASSERT(field, offset) typedef char LineProjectile_##field##_at_##offset[offsetof(PROJECTILE, field) == 0x##offset ? 1 : -1]
LINE_PROJECTILE_OFFSET_ASSERT(motion, 7c);
LINE_PROJECTILE_OFFSET_ASSERT(damage, 78);
LINE_PROJECTILE_OFFSET_ASSERT(animation, 94);
LINE_PROJECTILE_OFFSET_ASSERT(wind_enabled, b0);
LINE_PROJECTILE_OFFSET_ASSERT(probe_mode, b4);
LINE_PROJECTILE_OFFSET_ASSERT(floor_callback, b8);
LINE_PROJECTILE_OFFSET_ASSERT(impact_callback, bc);
LINE_PROJECTILE_OFFSET_ASSERT(scale, c0);
typedef char LineProjectile_size_c4[sizeof(PROJECTILE) == 0xc4 ? 1 : -1];
    #undef LINE_PROJECTILE_OFFSET_ASSERT
#endif

/* BEGIN GENERATED MODULE API */
SPRITE *expl_flash_create(sint32 x, sint32 y, sint32 z);
SPRITE *surface_impact_create(sint32 x, sint32 y, sint32 z, const sint32 *animation);
SPRITE *world_sprite_create(sint32 x, sint32 y, sint32 z, const sint32 *animation);
sint16 swept_collision_test(sint32 old_x, sint32 old_y, sint32 old_z, sint32 *new_x, sint32 *new_y, sint32 *new_z);
sint32 fixed_angle_from_vector(sint32 x, sint32 z);
sint32 world_object_is_visible(sint32 x, sint32 y, sint32 z, sint32 diameter);
void homing_projectile_steer(SPRITE *object);
void projectile_surface_impact_emit(sint32 x, sint32 y, sint32 z);
void projectile_trail_emit(SPRITE *object);
void world_sprite_update(SPRITE *object);
/* END GENERATED MODULE API */

#endif
