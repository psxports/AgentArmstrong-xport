#ifndef MECHANISM_H
#define MECHANISM_H

#include "effect_update.h"

/* Original: DAT_800D74D8, byte states reset on original stage load */
extern uint8 g_mechanism_switch_states[128];

void mechanism_switch_create(EFFECT *effect);
void lift_create(EFFECT *effect);

#endif
