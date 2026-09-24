#ifndef ANIMATION_H
#define ANIMATION_H

#include "xport.h"
#include "psx.h"

typedef struct
{
    uint32 next_address;
    uint32 start_address;
    sint32 frame;
    sint32 delay;
    sint32 frame_offset;
    uint16 flags;
    uint16 reserved;
    uint32 owner_address;
} ANIM;

typedef char AnimationCursor_size_1c[sizeof(ANIM) == 0x1c ? 1 : -1];

/* BEGIN GENERATED MODULE API */
GDB_CALL sint32 animation_update(ANIM *anim);
const sint32 *animation_get_next(ANIM *anim);
const sint32 *animation_get_start(ANIM *anim);
void animation_rebase(ANIM *anim, const sint32 *script);
void animation_set_next(ANIM *anim, const sint32 *p);
void animation_set_owner(ANIM *anim, void *owner);
void animation_set_start(ANIM *anim, const sint32 *p);
void animation_start(ANIM *anim, const sint32 *script);
/* END GENERATED MODULE API */

#endif
