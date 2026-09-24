#ifndef MECHANOID_H
#define MECHANOID_H

#include <stddef.h>

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "model.h"
#include "player.h"
#include "sprite.h"

typedef struct
{
    COLLISION collision;           /* +0x0000 */
    sint16 health;                 /* +0x0078 */
    sint16 hit_flash_ticks;        /* +0x007A */
    MODEL_NODE nodes[37];          /* +0x007C: root + 36 script nodes */
    uint8 reserved_e5c[0xC0];      /* +0x0E5C */
    MODEL_NODE *node_table[36];    /* +0x0F1C */
    uint8 reserved_fac[8];         /* +0x0FAC */
    ANIM animation;                /* +0x0FB4 */
    sint32 movement_line[6];       /* +0x0FD0 */
    sint32 movement_z;             /* +0x0FE8 */
    sint16 target_yaw;             /* +0x0FEC */
    sint16 yaw_step;               /* +0x0FEE */
    uint8 reserved_ff0[4];         /* +0x0FF0 */
    sint32 target_x;               /* +0x0FF4 */
    sint32 target_z;               /* +0x0FF8 */
    sint16 aim_yaw;                /* +0x0FFC */
    sint16 target_reached;         /* +0x0FFE */
    sint16 moving;                 /* +0x1000 */
    sint16 aim_jitter;             /* +0x1002 */
    sint16 attack_sequence_active; /* +0x1004 */
    uint16 field_1006;             /* +0x1006 */
    sint32 velocity_y;             /* +0x1008 */
    sint32 base_y;                 /* +0x100C */
    sint16 attack_variant;         /* +0x1010 */
    sint16 sound_handle;           /* +0x1012 */
    sint32 max_x;                  /* +0x1014 */
    sint32 min_x;                  /* +0x1018 */
    sint32 max_z;                  /* +0x101C */
    sint32 min_z;                  /* +0x1020 */
    sint16 phase;                  /* +0x1024 */
    uint16 field_1026;             /* +0x1026 */
} MECHANOID_ACTOR;

#if defined(AP_32BIT)
    #define MECHANOID_OFFSET_ASSERT(field, offset) typedef char MechanoidActor_##field##_at_##offset[offsetof(MECHANOID_ACTOR, field) == 0x##offset ? 1 : -1]
MECHANOID_OFFSET_ASSERT(nodes, 007c);
MECHANOID_OFFSET_ASSERT(node_table, 0f1c);
MECHANOID_OFFSET_ASSERT(animation, 0fb4);
MECHANOID_OFFSET_ASSERT(movement_line, 0fd0);
MECHANOID_OFFSET_ASSERT(movement_z, 0fe8);
MECHANOID_OFFSET_ASSERT(target_x, 0ff4);
MECHANOID_OFFSET_ASSERT(velocity_y, 1008);
MECHANOID_OFFSET_ASSERT(phase, 1024);
typedef char MechanoidActor_size_1028[sizeof(MECHANOID_ACTOR) == 0x1028 ? 1 : -1];
    #undef MECHANOID_OFFSET_ASSERT
#endif

/* BEGIN GENERATED MODULE API */
sint16 angle_approach_wrapped(sint16 target, sint32 step, sint16 current);
sint32 mechanoid_load_resources(void);
void mechanoid_create(sint32 x, sint32 y, sint32 z);
/* END GENERATED MODULE API */

#endif
