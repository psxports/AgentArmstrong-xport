#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "animation.h"
#include "app.h"
#include "audio/game_sound.h"
#include "collision.h"
#include "mission.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "stubs.h"

/* Functions. */
/* Animation scripts embedded in SLES contain PSX text addresses.  They are
 * data, not callable pointers in the Windows process. */
static void animation_callback(uint32 psx_address, void *owner)
{
    switch (psx_address)
    {
        case 0x80099874u:
            player_throw_explosive(owner);
            break;
        case 0x800a37c4u:
            mission_enemy_fire(owner);
            break;
        case 0x800a1f9cu:
            airfield_enemy_fire(owner);
            break;
        default:
        {
            FILE *log = fopen("animation_error.log", "w");
            if (log)
            {
                fprintf(log, "unsupported callback=%08X owner=%p\n", psx_address, owner);
                fclose(log);
            }
            fatal_error("BAD ANIM CALLBACK");
            abort();
        }
    }
}

void animation_set_owner(ANIM *anim, void *owner)
{
    anim->owner_address = (uint32)(intptr)owner;
}

static void *animation_owner(ANIM *anim)
{
    return (void *)(intptr)anim->owner_address;
}

const sint32 *animation_get_next(ANIM *anim)
{
    return (const sint32 *)(intptr)anim->next_address;
}

const sint32 *animation_get_start(ANIM *anim)
{
    return (const sint32 *)(intptr)anim->start_address;
}

void animation_set_next(ANIM *anim, const sint32 *p)
{
    anim->next_address = (uint32)(intptr)p;
}

void animation_set_start(ANIM *anim, const sint32 *p)
{
    anim->start_address = (uint32)(intptr)p;
}

void animation_rebase(ANIM *anim, const sint32 *script)
{
    const sint32 *n = animation_get_next(anim), *s = animation_get_start(anim);
    ptrdiff_t d = n && s ? n - s : 0;
    animation_set_start(anim, script);
    animation_set_next(anim, n ? script + d : 0);
}

/* Original: FUN_8009045C. */
void animation_start(ANIM *anim, const sint32 *script)
{
    anim->delay = 0;
    anim->flags = 0;
    anim->reserved = 0;
    anim->frame_offset = 0;
    animation_set_next(anim, script);
    animation_set_start(anim, script);
}

/* Original: FUN_80090478. */
GDB_CALL sint32 animation_update(ANIM *anim)
{
    void *owner = animation_owner(anim);
    for (;;)
    {
        const sint32 *command;
        sint32 opcode;
        if (anim->delay != 0)
        {
            --anim->delay;
            return anim->frame;
        }
        command = animation_get_next(anim);
        if (command == 0)
            return anim->frame;
        opcode = command[0];
        if (opcode == 0)
        {
            animation_set_next(anim, command + 2);
            anim->frame = command[1];
            return anim->frame;
        }
        if (opcode < -11 || opcode > -1)
        {
            sint32 rounded = opcode;
            if (rounded < 0)
                rounded += 3;
            anim->delay = opcode - (rounded >> 2);
            animation_set_next(anim, command + 2);
            anim->frame = command[1] + anim->frame_offset;
            continue;
        }
        if (opcode == -2)
        {
            animation_set_next(anim, animation_get_start(anim));
            continue;
        }
        if (opcode == -3)
        {
            animation_callback((uint32)command[1], owner);
            animation_set_next(anim, command + 2);
            continue;
        }
        if (opcode == -1)
        {
            animation_set_next(anim, 0);
            anim->flags = 0;
            anim->frame_offset = 0;
            return anim->frame;
        }
        if (opcode == -4)
        {
            COLLISION *object = (COLLISION *)owner;
            if (owner == 0)
                fatal_error("BAD ANITRANS");
            if (command[1] == 0)
                object->prim[7] &= (uint8)~2;
            else
                object->prim[7] |= 2;
            animation_set_next(anim, command + 2);
            continue;
        }
        if (opcode == -5)
        {
            anim->flags |= (uint16)command[1];
            animation_set_next(anim, command + 2);
            continue;
        }
        if (opcode == -6)
        {
            sound_play_positional((sint16)command[1], 0, (sint16)command[2], ((COLLISION *)owner)->x, ((COLLISION *)owner)->y, ((COLLISION *)owner)->z);
            animation_set_next(anim, command + 3);
            continue;
        }
        if (opcode == -8)
        {
            sound_play_nonpositional((sint16)command[1], (sint16)command[2], (sint16)command[3]);
            animation_set_next(anim, command + 4);
            continue;
        }
        if (opcode == -7)
        {
            anim->flags &= (uint16)~command[1];
            animation_set_next(anim, command + 2);
            continue;
        }
        if (opcode == -10)
        {
            uint32 frame = (uint32)anim->frame;
            if (frame < (uint32)command[1] || (uint32)command[2] <= frame)
            {
                if (frame == (uint32)command[2])
                    animation_set_next(anim, command + 4);
                else
                {
                    anim->frame = command[1];
                    anim->delay = command[3];
                }
            }
            else
            {
                anim->delay = command[3];
                ++anim->frame;
            }
            continue;
        }
        if (opcode == -11)
        {
            anim->frame = 0;
            animation_set_next(anim, command + 1);
            continue;
        }
        {
            sint32 rounded = opcode;
            if (rounded < 0)
                rounded += 3;
            anim->delay = opcode - (rounded >> 2);
            animation_set_next(anim, command + 2);
            anim->frame = command[1] + anim->frame_offset;
            continue;
        }
    }
}
