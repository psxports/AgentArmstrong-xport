#ifndef PSYQ_SOUND_H
#define PSYQ_SOUND_H

#include "app.h"
#include "audio/spu_core.h"

/* BEGIN GENERATED MODULE API */
sint16 psyq_sound_key_on(sint16 bank, sint16 program, sint16 tone, sint16 note, sint16 fine, sint16 left, sint16 right);
sint16 psyq_sound_key_on_voice(sint16 voice, sint16 bank, sint16 program, sint16 tone, sint16 note, sint16 fine, sint16 left, sint16 right);
sint16 psyq_sound_vab_open_head(const uint8 *header, sint16 requested_bank);
sint16 psyq_sound_vab_transfer_body(const uint8 *body, sint16 bank);
sint32 psyq_sound_get_voice_registers(sint16 voice, SPU_voice_registers *registers);
sint32 psyq_sound_get_voice_status(sint16 voice);
sint32 psyq_sound_vab_transfer_completed(sint16 mode);
uint16 psyq_sound_get_voice_envelope(sint16 voice);
void psyq_sound_end(void);
void psyq_sound_init(void);
void psyq_sound_music_play(sint32 track, sint32 volume);
void psyq_sound_music_pause(void);
void psyq_sound_music_resume(void);
void psyq_sound_music_set_volume(sint32 volume);
void psyq_sound_music_stop(void);
void psyq_sound_quit(void);
void psyq_sound_set_master_volume(sint16 left, sint16 right);
void psyq_sound_set_tick_mode(sint32 mode);
void psyq_sound_set_voice_pitch(sint16 voice, uint16 pitch);
void psyq_sound_set_voice_volume(sint16 voice, sint16 left, sint16 right);
void psyq_sound_start(void);
/* END GENERATED MODULE API */

#endif
