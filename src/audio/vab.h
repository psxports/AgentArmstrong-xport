#ifndef VAB_h
#define VAB_h

#include "app.h"

#define VAB_bank_count 16
#define VAB_program_count 128
#define VAB_tones_per_program 16

typedef struct VAB_program
{
    uint8 tone_count;
    uint8 volume;
    uint8 priority;
    uint8 mode;
    uint8 pan;
} VAB_program;

typedef struct VAB_tone
{
    uint8 priority;
    uint8 mode;
    uint8 volume;
    uint8 pan;
    uint8 center;
    uint8 shift;
    uint8 note_min;
    uint8 note_max;
    uint8 pitch_bend_min;
    uint8 pitch_bend_max;
    uint16 adsr1;
    uint16 adsr2;
    sint16 program;
    sint16 sample;
} VAB_tone;

/* BEGIN GENERATED MODULE API */
const VAB_program *vab_get_program(sint16 bank, sint16 program);
const VAB_tone *vab_get_tone(sint16 bank, sint16 program, sint16 tone);
sint16 vab_open_head(const uint8 *header, sint16 requested_bank);
sint16 vab_transfer_body(const uint8 *body, sint16 bank);
uint32 vab_get_body_size(sint16 bank);
uint32 vab_get_sample_address(sint16 bank, sint16 sample);
uint8 vab_get_master_volume(sint16 bank);
void vab_init(void);
void vab_shutdown(void);
/* END GENERATED MODULE API */

#endif
