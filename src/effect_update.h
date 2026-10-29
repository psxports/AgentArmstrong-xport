#ifndef MODULE_API_EFFECT_UPDATE_H
#define MODULE_API_EFFECT_UPDATE_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

typedef struct PLAYER PLAYER;

/* Types. */

typedef struct EFFECT
{
    sint32 type;
    sint32 x;
    sint32 y;
    sint32 z;
    sint16 handler;
    union
    {
        uint16 vals[7];
        sint16 signed_vals[7];
    };
} EFFECT;

typedef void (*EFFECT_UPDATE_CALLBACK)(EFFECT *effect);

extern EFFECT *g_effects;
extern sint32 g_effect_count;

/* BEGIN GENERATED MODULE API */
GDB_CALL EFFECT *effect_find_next(sint32 type, EFFECT *after);
sint16 effect_count(sint32 type);
sint32 effect_is_visible(EFFECT *effect);
void door_transition_player_update(void *raw_player);
void effects_initialize(const void *data, uint32 size);
void effects_spawn_initial(void);
void effects_update_visible(void);
/* END GENERATED MODULE API */

/* Native stage-heap lifetime boundary */
void effects_heap_reset(void);

#endif
