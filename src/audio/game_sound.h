#ifndef MODULE_API_AUDIO_GAME_SOUND_H
#define MODULE_API_AUDIO_GAME_SOUND_H

#include <stddef.h>

#include "../app.h"

/* Types. */
/* Pair returned by FUN_800AA6F0 and consumed by FUN_800AAA7C/FUN_800AA8FC. */
typedef struct
{
    sint16 volume_left;  /* +0x00 */
    sint16 volume_right; /* +0x02 */
} SOUND_VOLUME_PAIR;

/* PsyQ SpuVoiceAttr layout used by 0x800AAA7C..0x800AAB40. */
typedef struct
{
    uint32 voice_mask;        /* +0x00 */
    uint32 attribute_mask;    /* +0x04 */
    sint16 volume_left;       /* +0x08 */
    sint16 volume_right;      /* +0x0A */
    sint16 volume_mode_left;  /* +0x0C */
    sint16 volume_mode_right; /* +0x0E */
    uint16 start_address;     /* +0x10 */
    uint16 repeat_address;    /* +0x12 */
    uint16 pitch;             /* +0x14 */
    uint16 note;              /* +0x16 */
    uint16 sample_note;       /* +0x18 */
    uint16 envelope;          /* +0x1A */
    uint16 reserved[18];      /* +0x1C */
} SPU_voice_attributes;

#if defined(AP_32BIT)
typedef char SoundVolumePair_size_04[sizeof(SOUND_VOLUME_PAIR) == 0x04 ? 1 : -1];
typedef char SpuVoiceAttributes_size_40[sizeof(SPU_voice_attributes) == 0x40 ? 1 : -1];
typedef char SpuVoiceAttributes_mask_at_04[offsetof(SPU_voice_attributes, attribute_mask) == 0x04 ? 1 : -1];
typedef char SpuVoiceAttributes_pitch_at_14[offsetof(SPU_voice_attributes, pitch) == 0x14 ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
SOUND_VOLUME_PAIR *sound_spatial_volume_calculate(sint32 type, sint32 x, sint32 y, sint32 z);
SPU_voice_attributes *sound_voice_spatial_volume_update(SOUND_VOLUME_PAIR *volume, sint32 handle);
sint32 sound_play_nonpositional(sint32 id, sint32 note_delta, sint32 volume);
uint16 sound_play_positional(sint32 id, sint32 note_delta, sint32 type, sint32 x, sint32 y, sint32 z);
void sound_bank_load(sint32 bank_index, sint32 first_sound);
void sound_map_reset(void);
void sound_voice_attributes_apply(SPU_voice_attributes *attr);
void sound_voice_stop(void *raw_handle);
void speech_random_request(sint32 kind);
void stage_sound_banks_load(void);
/* END GENERATED MODULE API */

#endif
