#ifndef MODULE_API_EFFECT_UPDATE_H
#define MODULE_API_EFFECT_UPDATE_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

typedef struct PLAYER PLAYER;

/* Types. */
typedef struct
{
    sint16 x;
    sint16 y;
    sint16 z;
    sint16 type;
    uint16 values[7];
} EFFECT_DATA;

typedef struct EFFECT
{
    sint32 type;
    sint32 x;
    sint32 y;
    sint32 z;
    sint16 handler;
    uint16 values[7];
    uint8 unused_20[4];
} EFFECT;

typedef void (*EFFECT_UPDATE_CALLBACK)(EFFECT *effect);

typedef struct
{
    EFFECT_UPDATE_CALLBACK update;
    uint8 unknown_04[12];
} EFFECT_HANDLER_ENTRY;

typedef char PackedEffectSizeMustBe0x16[sizeof(EFFECT_DATA) == 0x16 ? 1 : -1];
typedef char RuntimeEffectSizeMustBe0x24[sizeof(EFFECT) == 0x24 ? 1 : -1];
typedef char RuntimeEffectHandlerAt10[offsetof(EFFECT, handler) == 0x10 ? 1 : -1];
typedef char RuntimeEffectValuesAt12[offsetof(EFFECT, values) == 0x12 ? 1 : -1];

/* BEGIN GENERATED MODULE API */
GDB_CALL EFFECT *effect_find_next(sint32 type, EFFECT *after);
sint32 effect_count(sint32 type);
sint32 effect_is_visible(EFFECT *effect);
void door_transition_player_update(PLAYER *player);
void effects_initialize(void);
void effects_spawn_initial(void);
void effects_update_visible(void);
/* END GENERATED MODULE API */

#endif
