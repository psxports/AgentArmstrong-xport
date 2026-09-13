#include <stddef.h>
#include <string.h>
#include "audio/spu_core.h"
#include "audio/vab.h"
#include "global.h"

/* Types. */
typedef struct
{
    uint32 magic;         /* +0x00 */
    uint8 field_04[0x0e]; /* +0x04 */
    uint16 program_count; /* +0x12 */
    uint16 tone_count;    /* +0x14 */
    uint16 sample_count;  /* +0x16 */
    uint8 master_volume;  /* +0x18 */
    uint8 master_pan;     /* +0x19 */
    uint8 field_1a[6];    /* +0x1A */
} VAB_header_record;

typedef struct
{
    uint8 tone_count;     /* +0x00 */
    uint8 volume;         /* +0x01 */
    uint8 priority;       /* +0x02 */
    uint8 mode;           /* +0x03 */
    uint8 pan;            /* +0x04 */
    uint8 field_05[0x0b]; /* +0x05 */
} VAB_program_record;

typedef struct
{
    uint8 priority;       /* +0x00 */
    uint8 mode;           /* +0x01 */
    uint8 volume;         /* +0x02 */
    uint8 pan;            /* +0x03 */
    uint8 center;         /* +0x04 */
    uint8 shift;          /* +0x05 */
    uint8 note_min;       /* +0x06 */
    uint8 note_max;       /* +0x07 */
    uint8 field_08[4];    /* +0x08 */
    uint8 pitch_bend_min; /* +0x0C */
    uint8 pitch_bend_max; /* +0x0D */
    uint8 field_0e[2];    /* +0x0E */
    uint16 adsr1;         /* +0x10 */
    uint16 adsr2;         /* +0x12 */
    sint16 program;       /* +0x14 */
    sint16 sample;        /* +0x16 */
    uint8 field_18[8];    /* +0x18 */
} VAB_tone_record;

#if defined(AP_32BIT)
typedef char VabHeaderRecord_size_20[sizeof(VAB_header_record) == 0x20 ? 1 : -1];
typedef char VabHeaderRecord_programs_12[offsetof(VAB_header_record, program_count) == 0x12 ? 1 : -1];
typedef char VabProgramRecord_size_10[sizeof(VAB_program_record) == 0x10 ? 1 : -1];
typedef char VabToneRecord_size_20[sizeof(VAB_tone_record) == 0x20 ? 1 : -1];
typedef char VabToneRecord_adsr1_10[offsetof(VAB_tone_record, adsr1) == 0x10 ? 1 : -1];
typedef char VabToneRecord_sample_16[offsetof(VAB_tone_record, sample) == 0x16 ? 1 : -1];
#endif

typedef struct VAB_bank
{
    VAB_program programs[VAB_program_count];
    VAB_tone tones[VAB_program_count][VAB_tones_per_program];
    uint32 sample_addresses[256];
    uint32 body_address;
    uint32 body_size;
    uint8 master_volume;
    uint8 master_pan;
    uint8 program_count;
    uint8 valid;
    uint8 transferred;
} VAB_bank;

/* Variables. */
static VAB_bank banks[VAB_bank_count];

static uint32 next_sound_ram_address;

/* Functions. */
void vab_init(void)
{
    memset(banks, 0, sizeof(banks));
    /* Original runtime probe: the first MAIN sample starts at register 0x202,
     * i.e. byte address 0x1010. The preceding area is reserved by libsnd. */
    next_sound_ram_address = 0x1010;
}

void vab_shutdown(void)
{
    /* Both lifecycle paths reset the same registry and allocation cursor. */
    vab_init();
}

sint16 vab_open_head(const uint8 *header, sint16 requested_bank)
{
    const VAB_header_record *header_record = (const VAB_header_record *)header;
    VAB_bank parsed;
    uint16 program_count;
    uint16 tone_count;
    uint16 sample_count;
    uint32 tone_group;
    uint32 program;
    uint32 size_table_offset;
    uint32 sample_offset;
    sint16 bank;

    if (header == 0 || header_record->magic != 0x56414270u)
        return -1;
    program_count = header_record->program_count;
    tone_count = header_record->tone_count;
    sample_count = header_record->sample_count;
    if (program_count > VAB_program_count || tone_count > 2048 || sample_count > 255)
        return -1;

    if (requested_bank < 0)
    {
        for (bank = 0; bank < VAB_bank_count; ++bank)
            if (!banks[bank].valid)
                break;
        if (bank == VAB_bank_count)
            return -1;
    }
    else
    {
        if (requested_bank >= VAB_bank_count || banks[requested_bank].valid)
            return -1;
        bank = requested_bank;
    }

    memset(&parsed, 0, sizeof(parsed));
    parsed.master_volume = header_record->master_volume;
    parsed.master_pan = header_record->master_pan;
    parsed.program_count = (uint8)program_count;
    tone_group = 0;
    for (program = 0; program < VAB_program_count; ++program)
    {
        const VAB_program_record *source = (const VAB_program_record *)(header_record + 1) + program;
        VAB_program *destination = parsed.programs + program;
        uint32 tone;
        if (source->tone_count == 0)
            continue;
        if (source->tone_count > VAB_tones_per_program || tone_group >= program_count)
            return -1;
        destination->tone_count = source->tone_count;
        destination->volume = source->volume;
        destination->priority = source->priority;
        destination->mode = source->mode;
        destination->pan = source->pan;
        for (tone = 0; tone < source->tone_count; ++tone)
        {
            const VAB_tone_record *tone_source = (const VAB_tone_record *)((const VAB_program_record *)(header_record + 1) + VAB_program_count) + tone_group * VAB_tones_per_program + tone;
            VAB_tone *tone_destination = &parsed.tones[program][tone];
            tone_destination->priority = tone_source->priority;
            tone_destination->mode = tone_source->mode;
            tone_destination->volume = tone_source->volume;
            tone_destination->pan = tone_source->pan;
            tone_destination->center = tone_source->center;
            tone_destination->shift = tone_source->shift;
            tone_destination->note_min = tone_source->note_min;
            tone_destination->note_max = tone_source->note_max;
            tone_destination->pitch_bend_min = tone_source->pitch_bend_min;
            tone_destination->pitch_bend_max = tone_source->pitch_bend_max;
            tone_destination->adsr1 = tone_source->adsr1;
            tone_destination->adsr2 = tone_source->adsr2;
            tone_destination->program = tone_source->program;
            tone_destination->sample = tone_source->sample;
            if (tone_destination->sample < 1 || tone_destination->sample > (sint16)sample_count)
                return -1;
        }
        ++tone_group;
    }
    if (tone_group != program_count)
        return -1;

    size_table_offset = 32 + 128 * 16 + (uint32)program_count * 16 * 32;
    sample_offset = 0;
    for (program = 0; program < 256; ++program)
    {
        uint32 size = (uint32)read_u16_le(header + size_table_offset + program * 2) * 8u;
        if (program >= 1 && program <= sample_count)
            parsed.sample_addresses[program] = next_sound_ram_address + sample_offset;
        sample_offset += size;
    }
    parsed.body_size = sample_offset;
    if (parsed.body_size == 0 || next_sound_ram_address > SPU_ram_size || parsed.body_size > SPU_ram_size - next_sound_ram_address)
        return -1;
    parsed.body_address = next_sound_ram_address;
    parsed.valid = 1;
    banks[bank] = parsed;
    next_sound_ram_address = (next_sound_ram_address + parsed.body_size + 7u) & ~7u;
    return bank;
}

sint16 vab_transfer_body(const uint8 *body, sint16 bank)
{
    VAB_bank *entry;
    if (bank < 0 || bank >= VAB_bank_count || body == 0)
        return -1;
    entry = banks + bank;
    if (!entry->valid || !spu_core_upload(entry->body_address, body, entry->body_size))
        return -1;
    entry->transferred = 1;
    return bank;
}

sint32 vab_is_valid(sint16 bank)
{
    return bank >= 0 && bank < VAB_bank_count && banks[bank].valid && banks[bank].transferred;
}

const VAB_program *vab_get_program(sint16 bank, sint16 program)
{
    if (!vab_is_valid(bank) || program < 0 || program >= VAB_program_count || banks[bank].programs[program].tone_count == 0)
        return 0;
    return &banks[bank].programs[program];
}

const VAB_tone *vab_get_tone(sint16 bank, sint16 program, sint16 tone)
{
    const VAB_program *program_entry = vab_get_program(bank, program);
    if (program_entry == 0 || tone < 0 || tone >= program_entry->tone_count)
        return 0;
    return &banks[bank].tones[program][tone];
}

uint32 vab_get_sample_address(sint16 bank, sint16 sample)
{
    if (!vab_is_valid(bank) || sample < 1 || sample > 255)
        return 0;
    return banks[bank].sample_addresses[sample];
}

uint32 vab_get_body_size(sint16 bank)
{
    if (bank < 0 || bank >= VAB_bank_count || !banks[bank].valid)
        return 0;
    return banks[bank].body_size;
}

uint8 vab_get_master_volume(sint16 bank)
{
    if (bank < 0 || bank >= VAB_bank_count || !banks[bank].valid)
        return 0;
    return banks[bank].master_volume;
}

uint8 vab_get_master_pan(sint16 bank)
{
    if (bank < 0 || bank >= VAB_bank_count || !banks[bank].valid)
        return 0;
    return banks[bank].master_pan;
}
