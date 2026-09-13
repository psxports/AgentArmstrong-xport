#include <stdio.h>
#include <string.h>
#include "../object.h"
#include "../platform/win/audio_waveout.h"
#include "../platform/win/platform_file.h"
#include "audio/audio_runtime.h"
#include "audio/psyq_sound.h"
#include "audio/vab.h"
#include "audio_runtime.h"
#include "platform/win/audio_waveout.h"
#include "vab.h"

/* Types. */
typedef struct PSYQ_SOUND_VOICE
{
    sint16 bank;
    sint16 program;
    sint16 tone;
    uint16 age;
    uint8 priority;
    uint8 keyed;
} PSYQ_SOUND_VOICE;

/* Variables. */
static PSYQ_SOUND_VOICE voices[SPU_voice_count];

static sint32 vab_transfer_complete;

static sint32 music_track = -1;

/* Exact 800D31AC pitch table consumed by PAL note2pitch at 800BE71C. */
static const uint16 pitch_table[192] = {
    0x1000, 0x100e, 0x101d, 0x102c, 0x103b, 0x104a, 0x1059, 0x1068, 0x1078, 0x1087, 0x1096, 0x10a5, 0x10b5, 0x10c4, 0x10d4, 0x10e3, 0x10f3, 0x1103, 0x1113, 0x1122, 0x1132, 0x1142, 0x1152, 0x1162, 0x1172, 0x1182, 0x1193, 0x11a3, 0x11b3, 0x11c4, 0x11d4, 0x11e5, 0x11f5, 0x1206, 0x1216, 0x1227, 0x1238, 0x1249, 0x125a, 0x126b, 0x127c, 0x128d, 0x129e, 0x12af, 0x12c1, 0x12d2, 0x12e3, 0x12f5, 0x1306, 0x1318, 0x132a, 0x133c, 0x134d, 0x135f, 0x1371, 0x1383, 0x1395, 0x13a7, 0x13ba, 0x13cc, 0x13de, 0x13f1, 0x1403, 0x1416, 0x1428, 0x143b, 0x144e, 0x1460, 0x1473, 0x1486, 0x1499, 0x14ac, 0x14bf, 0x14d3, 0x14e6, 0x14f9, 0x150d, 0x1520, 0x1534, 0x1547, 0x155b, 0x156f, 0x1583, 0x1597, 0x15ab, 0x15bf, 0x15d3, 0x15e7, 0x15fb, 0x1610, 0x1624, 0x1638, 0x164d, 0x1662, 0x1676, 0x168b,
    0x16a0, 0x16b5, 0x16ca, 0x16df, 0x16f4, 0x170a, 0x171f, 0x1734, 0x174a, 0x175f, 0x1775, 0x178b, 0x17a1, 0x17b6, 0x17cc, 0x17e2, 0x17f9, 0x180f, 0x1825, 0x183b, 0x1852, 0x1868, 0x187f, 0x1896, 0x18ac, 0x18c3, 0x18da, 0x18f1, 0x1908, 0x191f, 0x1937, 0x194e, 0x1965, 0x197d, 0x1995, 0x19ac, 0x19c4, 0x19dc, 0x19f4, 0x1a0c, 0x1a24, 0x1a3c, 0x1a55, 0x1a6d, 0x1a85, 0x1a9e, 0x1ab7, 0x1acf, 0x1ae8, 0x1b01, 0x1b1a, 0x1b33, 0x1b4c, 0x1b66, 0x1b7f, 0x1b98, 0x1bb2, 0x1bcc, 0x1be5, 0x1bff, 0x1c19, 0x1c33, 0x1c4d, 0x1c67, 0x1c82, 0x1c9c, 0x1cb7, 0x1cd1, 0x1cec, 0x1d07, 0x1d22, 0x1d3d, 0x1d58, 0x1d73, 0x1d8e, 0x1da9, 0x1dc5, 0x1de0, 0x1dfc, 0x1e18, 0x1e34, 0x1e50, 0x1e6c, 0x1e88, 0x1ea4, 0x1ec1, 0x1edd, 0x1efa, 0x1f16, 0x1f33, 0x1f50, 0x1f6d, 0x1f8a, 0x1fa7, 0x1fc5, 0x1fe2,
};

/* Functions. */
__declspec(dllexport) volatile uint32 g_psx_spu_key_on_requests;

__declspec(dllexport) volatile uint32 g_psx_spu_key_on_successes;

__declspec(dllexport) volatile uint32 g_psx_spu_key_on_no_voice;

__declspec(dllexport) volatile uint32 g_psx_spu_key_on_free_voice;

__declspec(dllexport) volatile uint32 g_psx_spu_key_on_voice_steals;

static uint16 note_to_pitch(sint16 note, uint16 fine, const VAB_tone *tone)
{
    sint32 fine_index = ((sint32)fine + tone->shift) / 8;
    sint32 carry = 0;
    sint32 semitone;
    sint32 octave;
    sint32 remainder;
    uint32 pitch;
    if (fine_index >= 16)
    {
        carry = 1;
        fine_index -= 16;
    }
    semitone = carry + note + 60 - tone->center;
    octave = semitone / 12;
    if (semitone < 0 && semitone % 12)
        --octave;
    remainder = semitone - octave * 12;
    pitch = pitch_table[remainder * 16 + fine_index];
    if (octave > 5)
        pitch <<= octave - 5;
    else if (octave < 5)
        pitch >>= 5 - octave;
    return (uint16)pitch;
}

static sint16 compose_volume(sint16 primary, sint16 secondary, uint8 bank_volume, const VAB_program *program, const VAB_tone *tone, sint32 left)
{
    uint32 maximum;
    uint32 pan;
    uint32 volume;
    uint32 channel;
    if (primary < 0)
        primary = 0;
    if (secondary < 0)
        secondary = 0;
    if (primary == secondary)
    {
        maximum = (uint32)primary;
        pan = 64;
    }
    else if (secondary >= primary)
    {
        maximum = (uint32)secondary;
        pan = secondary ? 127u - ((uint32)primary << 6) / (uint32)secondary : 64;
    }
    else
    {
        maximum = (uint32)primary;
        pan = primary ? ((uint32)secondary << 6) / (uint32)primary : 64;
    }
    volume = maximum * 0x3fffu * bank_volume / 16129u;
    volume = volume * program->volume * tone->volume / 0x3f01u;
    channel = volume;
    if (tone->pan >= 64)
    {
        if (left)
            channel = channel * (127 - tone->pan) / 63;
    }
    else if (!left)
        channel = channel * tone->pan / 63;
    if (program->pan >= 64)
    {
        if (left)
            channel = channel * (127 - program->pan) / 63;
    }
    else if (!left)
        channel = channel * program->pan / 63;
    if (pan >= 64)
    {
        if (left)
            channel = channel * (127 - pan) / 63;
    }
    else if (!left)
        channel = channel * pan / 63;
    return (sint16)(channel * channel / 0x3fff);
}

static sint16 start_voice(sint16 voice, sint16 bank, sint16 program, sint16 tone_index, sint16 note, sint16 fine, sint16 left, sint16 right)
{
    const VAB_program *program_entry = vab_get_program(bank, program);
    const VAB_tone *tone = vab_get_tone(bank, program, tone_index);
    SPU_voice_registers registers;
    uint32 sample_address;
    if (voice < 0 || voice >= SPU_voice_count || program_entry == 0 || tone == 0)
        return -1;
    sample_address = vab_get_sample_address(bank, tone->sample);
    if (sample_address == 0)
        return -1;
    memset(&registers, 0, sizeof(registers));
    registers.volume_left = compose_volume(left, right, vab_get_master_volume(bank), program_entry, tone, 1);
    registers.volume_right = compose_volume(left, right, vab_get_master_volume(bank), program_entry, tone, 0);
    registers.pitch = note_to_pitch(note, (uint16)fine, tone);
    registers.start_address = (uint16)(sample_address / 8u);
    registers.repeat_address = registers.start_address;
    registers.adsr1 = tone->adsr1;
    registers.adsr2 = tone->adsr2;
    audio_runtime_set_voice_registers(voice, &registers);
    audio_runtime_key_on(1u << voice);
    voices[voice].bank = bank;
    voices[voice].program = program;
    voices[voice].tone = tone_index;
    voices[voice].priority = tone->priority;
    voices[voice].age = 0;
    voices[voice].keyed = 1;
    return voice;
}

void psyq_sound_init(void)
{
    sint32 voice;
    memset(voices, 0, sizeof(voices));
    g_psx_spu_key_on_requests = 0;
    g_psx_spu_key_on_successes = 0;
    g_psx_spu_key_on_no_voice = 0;
    g_psx_spu_key_on_free_voice = 0;
    g_psx_spu_key_on_voice_steals = 0;
    /* SsInitHot initializes the libsnd age halfword at +2 to the voice count. */
    for (voice = 0; voice < SPU_voice_count; ++voice)
        voices[voice].age = SPU_voice_count;
    audio_runtime_init();
    waveout_init();
    music_track = -1;
    vab_transfer_complete = 1;
}

void psyq_sound_end(void)
{
    sint32 voice;
    for (voice = 0; voice < SPU_voice_count; ++voice)
        voices[voice].keyed = 0;
    audio_runtime_key_off(0x00ffffffu);
    audio_runtime_music_stop();
    music_track = -1;
}

void psyq_sound_quit(void)
{
    waveout_shutdown();
    audio_runtime_shutdown();
}

void psyq_sound_set_master_volume(sint16 left, sint16 right)
{
    audio_runtime_set_master_volume((sint16)(left * 129), (sint16)(right * 129));
}

void psyq_sound_music_play(sint32 track, sint32 volume)
{
    char path[32];
    sint32 size = 0;
    uint8 *data;
    if (track == music_track)
    {
        audio_runtime_music_set_volume(volume);
        return;
    }
    sprintf(path, "MUSIC\\%d.WAV", track);
    data = (uint8 *)app_file_load(path, &size);
    music_track = audio_runtime_music_play(data, size > 0 ? (uint32)size : 0, volume) ? track : -1;
}

void psyq_sound_music_stop(void)
{
    audio_runtime_music_stop();
    music_track = -1;
}

void psyq_sound_music_pause(void)
{
    audio_runtime_music_set_paused(1);
}

void psyq_sound_music_resume(void)
{
    audio_runtime_music_set_paused(0);
}

void psyq_sound_music_set_volume(sint32 volume)
{
    audio_runtime_music_set_volume(volume);
}

void psyq_sound_set_tick_mode(sint32 mode)
{
    audio_runtime_set_tick_mode(mode);
}

void psyq_sound_start(void)
{
    audio_runtime_start_ticks();
}

sint16 psyq_sound_vab_open_head(const uint8 *header, sint16 requested_bank)
{
    sint16 bank = audio_runtime_vab_open_head(header, requested_bank);
    if (bank >= 0)
        vab_transfer_complete = 0;
    return bank;
}

sint16 psyq_sound_vab_transfer_body(const uint8 *body, sint16 bank)
{
    sint16 result = audio_runtime_vab_transfer_body(body, bank);
    vab_transfer_complete = (result >= 0);
    return result;
}

sint32 psyq_sound_vab_transfer_completed(sint16 mode)
{
    (void)mode;
    return vab_transfer_complete;
}

sint16 psyq_sound_key_on(sint16 bank, sint16 program, sint16 tone, sint16 note, sint16 fine, sint16 left, sint16 right)
{
    const VAB_tone *requested_tone = vab_get_tone(bank, program, tone);
    sint16 voice;
    sint16 selected = -1;
    sint16 best_priority;
    uint16 best_envelope = 0xffffu;
    sint16 best_age = 0;
    sint32 selected_was_free = 0;
    ++g_psx_spu_key_on_requests;
    if (requested_tone == 0)
    {
        ++g_psx_spu_key_on_no_voice;
        return -1;
    }
    best_priority = requested_tone->priority;
    for (voice = 0; voice < SPU_voice_count; ++voice)
    {
        SPU_adsr_phase phase = audio_runtime_voice_phase(voice);
        uint16 envelope = audio_runtime_voice_envelope(voice);
        sint16 priority = voices[voice].priority;
        if (phase == SPU_adsr_off && envelope == 0)
        {
            selected = voice;
            selected_was_free = 1;
            break;
        }
        /* Literal selection ordering of 800BDBBC..800BDC60: the smallest
         * eligible priority, then envelope, then greatest age. */
        if (priority < best_priority)
        {
            best_priority = priority;
            best_envelope = envelope;
            best_age = (sint16)voices[voice].age;
            selected = voice;
        }
        else if (priority == best_priority)
        {
            if (envelope < best_envelope || (envelope == best_envelope && (sint16)voices[voice].age > best_age))
            {
                best_envelope = envelope;
                best_age = (sint16)voices[voice].age;
                selected = voice;
            }
        }
    }
    if (selected < 0)
    {
        ++g_psx_spu_key_on_no_voice;
        return -1;
    }
    for (voice = 0; voice < SPU_voice_count; ++voice)
        ++voices[voice].age;
    voice = start_voice(selected, bank, program, tone, note, fine, left, right);
    if (voice < 0)
    {
        ++g_psx_spu_key_on_no_voice;
        return -1;
    }
    ++g_psx_spu_key_on_successes;
    if (selected_was_free)
        ++g_psx_spu_key_on_free_voice;
    else
        ++g_psx_spu_key_on_voice_steals;
    return voice;
}

sint16 psyq_sound_key_on_voice(sint16 voice, sint16 bank, sint16 program, sint16 tone, sint16 note, sint16 fine, sint16 left, sint16 right)
{
    return start_voice(voice, bank, program, tone, note, fine, left, right);
}

void psyq_sound_set_voice_volume(sint16 voice, sint16 left, sint16 right)
{
    if (voice < 0 || voice >= SPU_voice_count)
        return;
    /* FUN_800AAA7C has already formed the final SPU voice registers.  Its
     * mask=1<<voice and attr=3 reach SpuSetVoiceAttr at 800C1C1C; applying
     * the VAB bank/program/tone gain here a second time is not PsyQ behavior. */
    audio_runtime_set_voice_volume(voice, left, right);
}

void psyq_sound_set_voice_pitch(sint16 voice, uint16 pitch)
{
    if (voice < 0 || voice >= SPU_voice_count)
        return;
    audio_runtime_set_voice_pitch(voice, pitch);
}

sint32 psyq_sound_get_voice_registers(sint16 voice, SPU_voice_registers *registers)
{
    return audio_runtime_get_voice_registers(voice, registers);
}

uint16 psyq_sound_get_voice_envelope(sint16 voice)
{
    return audio_runtime_voice_envelope(voice);
}

sint32 psyq_sound_get_voice_status(sint16 voice)
{
    SPU_adsr_phase phase;
    uint16 envelope;
    if (voice < 0 || voice >= SPU_voice_count)
        return -1;
    phase = audio_runtime_voice_phase(voice);
    envelope = audio_runtime_voice_envelope(voice);
    if (phase == SPU_adsr_off)
    {
        voices[voice].keyed = 0;
        return 0;
    }
    if (voices[voice].keyed)
        return envelope ? 1 : 3;
    return phase != SPU_adsr_off && envelope ? 2 : 0;
}
