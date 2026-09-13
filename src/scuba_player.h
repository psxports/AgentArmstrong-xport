#ifndef SCUBA_PLAYER_H
#define SCUBA_PLAYER_H

#include "animation.h"
#include "app.h"
#include "collision.h"
#include "effect_update.h"
#include "model.h"
#include "player.h"
#include "sprite.h"

typedef struct SCUBA_PLAYER SCUBA_PLAYER;

/* Tail view shared by resident inventory/cheat code while the WATER overlay
 * owns the 0x6AC-byte player allocation. */
typedef struct
{
    uint8 field_000[0x6AA];
    uint16 air; /* +0x6AA */
} SCUBA_PLAYER_AIR_VIEW;

typedef char ScubaPlayerAirView_size_6ac[sizeof(SCUBA_PLAYER_AIR_VIEW) == 0x6ac ? 1 : -1];

/* BEGIN GENERATED MODULE API */
void scuba_submarine_create(EFFECT *effect);
void scuba_drifting_hazard_create(EFFECT *effect);
void scuba_mine_create(EFFECT *effect);
void scuba_tug_target_create(EFFECT *effect);
void scuba_bubble_emitter_update(EFFECT *effect);
void scuba_diver_enemy_create(EFFECT *effect);
void scuba_noop_effect_update(EFFECT *effect);
void scuba_ship_target_create(EFFECT *effect);
void scuba_mission_complete_player_update(SCUBA_PLAYER *player);
void scuba_model_target_create(sint32 x, sint32 y, sint32 z, sint32 model);
void scuba_player_create(sint32 x, sint32 y, sint32 z);
/* END GENERATED MODULE API */

#endif
