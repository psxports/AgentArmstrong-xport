#ifndef MODULE_API_AUDIO_GAME_SOUND_H
#define MODULE_API_AUDIO_GAME_SOUND_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

/* Types. */
/* Pair returned by FUN_800AA6F0 and consumed by FUN_800AAA7C/FUN_800AA8FC. */
typedef struct
{
    sint16 volume_left;  /* +0x00 */
    sint16 volume_right; /* +0x02 */
} SOUND_VOLUME_PAIR;

#if defined(AP_32BIT)
typedef char SoundVolumePair_size_04[sizeof(SOUND_VOLUME_PAIR) == 0x04 ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
SOUND_VOLUME_PAIR *sound_spatial_volume_calculate(sint32 type, sint32 x, sint32 y, sint32 z);
SpuVoiceAttr *sound_voice_spatial_volume_update(SOUND_VOLUME_PAIR *volume, sint32 handle);
sint32 sound_play_nonpositional(sint32 id, sint32 note_delta, sint32 volume);
uint16 sound_play_positional(sint32 id, sint32 note_delta, sint32 type, sint32 x, sint32 y, sint32 z);
void sound_bank_load(sint32 bank_index, sint32 first_sound);
void sound_map_reset(void);
void sound_voice_attributes_apply(SpuVoiceAttr *attr);
void sound_voice_stop(void *raw_handle);
void speech_random_request(sint32 kind);
void stage_sound_banks_load(void);
/* END GENERATED MODULE API */

#endif
