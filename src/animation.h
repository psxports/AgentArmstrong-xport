#ifndef ANIMATION_H
#define ANIMATION_H

#include "xport.h"
#include "psx.h"

typedef void (*ANIM_CALLBACK)(void *owner);

typedef enum
{
    ANIM_SET_FRAME,
    ANIM_STEP_FRAME,
    ANIM_STOP,
    ANIM_LOOP,
    ANIM_CALL,
    ANIM_TRANSLUCENCY,
    ANIM_FLAGS_SET,
    ANIM_SOUND_POSITIONAL,
    ANIM_FLAGS_CLEAR,
    ANIM_SOUND,
    ANIM_FRAME_RANGE,
    ANIM_RESET_FRAME,
    ANIM_HOLD_FRAME,
    ANIM_OP_COUNT
} ANIM_OP;

typedef struct
{
    ANIM_OP op;
    sint32 args[3];
    ANIM_CALLBACK callback;
} ANIM_COMMAND;

extern const ANIM_COMMAND g_static_frame_animation[1];

typedef struct
{
    const ANIM_COMMAND *next;
    const ANIM_COMMAND *start;
    sint32 frame;
    sint32 delay;
    sint32 frame_offset;
    uint16 flags;
    uint16 mirrored;
    void *owner;
} ANIM;

/* BEGIN GENERATED MODULE API */
GDB_CALL sint32 animation_update(ANIM *anim);
void animation_start(ANIM *anim, const ANIM_COMMAND *script);
/* END GENERATED MODULE API */

#endif
