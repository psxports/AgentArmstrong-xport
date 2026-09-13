#ifndef AUDIO_RUNTIME_H
#define AUDIO_RUNTIME_H

#include "app.h"
#include "audio/spu_core.h"
#include "audio/vab.h"

/* BEGIN GENERATED MODULE API */
SPU_adsr_phase audio_runtime_voice_phase(sint32 voice);
sint16 audio_runtime_vab_open_head(const uint8 *header, sint16 requested_bank);
sint16 audio_runtime_vab_transfer_body(const uint8 *body, sint16 bank);
sint32 audio_runtime_get_voice_registers(sint32 voice, SPU_voice_registers *registers);
uint16 audio_runtime_voice_envelope(sint32 voice);
void audio_runtime_init(void);
void audio_runtime_key_off(uint32 voice_mask);
void audio_runtime_key_on(uint32 voice_mask);
sint32 audio_runtime_music_play(uint8 *data, uint32 size, sint32 volume);
void audio_runtime_music_set_paused(sint32 paused);
void audio_runtime_music_set_volume(sint32 volume);
void audio_runtime_music_stop(void);
void audio_runtime_render(sint16 *stereo, uint32 frames);
void audio_runtime_set_master_volume(sint16 left, sint16 right);
void audio_runtime_set_tick_mode(sint32 mode);
void audio_runtime_set_voice_pitch(sint32 voice, uint16 pitch);
void audio_runtime_set_voice_registers(sint32 voice, const SPU_voice_registers *registers);
void audio_runtime_set_voice_volume(sint32 voice, sint16 left, sint16 right);
void audio_runtime_shutdown(void);
void audio_runtime_start_ticks(void);
/* END GENERATED MODULE API */

#endif
