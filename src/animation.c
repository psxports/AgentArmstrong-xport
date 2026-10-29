#include "render.h"
#include "animation.h"
#include "game_sound.h"
#include "collision.h"

/* A live static frame replaces the original delay-only sprite script pointer */
const ANIM_COMMAND g_static_frame_animation[1] = {
    {ANIM_HOLD_FRAME, {0, 0, 0}, 0},
};

/* Functions. */
/* Original: SLES_004.74:FUN_8009045C. */
void animation_start(ANIM *anim, const ANIM_COMMAND *script)
{
    anim->delay = 0;
    anim->flags = 0;
    anim->mirrored = 0;
    anim->frame_offset = 0;
    anim->next = script;
    anim->start = script;
}

/* Original: SLES_004.74:FUN_80090478. */
GDB_CALL sint32 animation_update(ANIM *anim)
{
    for (;;)
    {
        const ANIM_COMMAND *command;
        void *owner;
        if (anim->delay != 0)
        {
            anim->delay = (anim->delay - 1u);
            return anim->frame;
        }
        command = anim->next;
        if (command == 0)
            return anim->frame;
        owner = anim->owner;
        switch (command->op)
        {
            case ANIM_SET_FRAME:
                anim->next = command + 1;
                anim->frame = command->args[0];
                return anim->frame;
            case ANIM_STEP_FRAME:
            {
                sint32 duration = command->args[0];
                sint32 rounded = duration;
                if (rounded < 0)
                    rounded += 3;
                anim->delay = duration - (rounded >> 2);
                anim->next = command + 1;
                anim->frame = ((uint32)command->args[1] + anim->frame_offset);
                continue;
            }
            case ANIM_LOOP:
                anim->next = anim->start;
                continue;
            case ANIM_CALL:
                command->callback(owner);
                anim->next = command + 1;
                continue;
            case ANIM_STOP:
                anim->next = 0;
                anim->flags = 0;
                anim->frame_offset = 0;
                return anim->frame;
            case ANIM_TRANSLUCENCY:
            {
                COLLISION *object = (COLLISION *)owner;
                if (owner == 0)
                    fatal_error("BAD ANITRANS");
                if (command->args[0] == 0)
                    object->prim.rgbc.code &= ~2;
                else
                    object->prim.rgbc.code |= 2;
                anim->next = command + 1;
                continue;
            }
            case ANIM_FLAGS_SET:
                anim->flags |= command->args[0];
                anim->next = command + 1;
                continue;
            case ANIM_SOUND_POSITIONAL:
                sound_play_positional((sint16)command->args[0], 0, (sint16)command->args[1], ((COLLISION *)owner)->x, ((COLLISION *)owner)->y, ((COLLISION *)owner)->z);
                anim->next = command + 1;
                continue;
            case ANIM_SOUND:
                sound_play_nonpositional((sint16)command->args[0], (sint16)command->args[1], (sint16)command->args[2]);
                anim->next = command + 1;
                continue;
            case ANIM_FLAGS_CLEAR:
                anim->flags &= ~command->args[0];
                anim->next = command + 1;
                continue;
            case ANIM_FRAME_RANGE:
            {
                uint32 frame = anim->frame;
                uint32 first_frame = command->args[0];
                uint32 last_frame = command->args[1];
                if (frame < first_frame || last_frame <= frame)
                {
                    if (frame == last_frame)
                        anim->next = command + 1;
                    else
                    {
                        anim->frame = command->args[0];
                        anim->delay = command->args[2];
                    }
                }
                else
                {
                    anim->delay = command->args[2];
                    anim->frame = (anim->frame + 1u);
                }
                continue;
            }
            case ANIM_RESET_FRAME:
                anim->frame = 0;
                anim->next = command + 1;
                continue;
            case ANIM_HOLD_FRAME:
                return anim->frame;
        }
    }
}
