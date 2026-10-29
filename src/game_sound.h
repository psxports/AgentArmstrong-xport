#ifndef GAME_SOUND_H
#define GAME_SOUND_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

extern sint16 g_ambient_speech_timer;
extern sint32 g_sound_handles_invalidated;

extern sint32 g_sfx_playback_volume;
extern sint32 g_sfx_volume_setting;
extern sint32 g_music_volume_setting;
extern sint32 g_cur_music_track;
extern sint32 g_cd_audio_track_offset;
extern const uint8 g_music_track_volumes[24];

GDB_CALL void music_track_select(sint32 id);

/* Types. */
/* Borrowed scratch pair survives until the next spatial-volume calculation */
typedef struct SOUND_VOLUME_PAIR
{
    sint16 volume_left;
    sint16 volume_right;
} SOUND_VOLUME_PAIR;

/* BEGIN GENERATED MODULE API */
const SOUND_VOLUME_PAIR *sound_spatial_volume_calculate(sint32 type, sint32 x, sint32 y, sint32 z);
SpuVoiceAttr *sound_voice_spatial_volume_update(const SOUND_VOLUME_PAIR *volume, sint32 handle);
sint16 sound_play_nonpositional(sint32 id, sint32 note_delta, sint32 volume);
uint16 sound_play_positional(sint32 id, sint32 note_delta, sint32 type, sint32 x, sint32 y, sint32 z);
void sound_bank_load(sint32 bank_idx, sint32 first_sound);
void sound_map_reset(void);
void sound_all_voices_stop(void);
void sound_voice_attributes_apply(SpuVoiceAttr *attr);
void sound_voice_stop(sint16 *handle);
void speech_random_request(sint32 kind);
void stage_sound_banks_load(void);
/* END GENERATED MODULE API */

extern void *g_sound_archive;
extern sint32 g_sound_archive_size;

#endif
