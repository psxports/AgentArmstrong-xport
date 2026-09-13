#include <stdlib.h>
#include <string.h>
#include "../platform/win/windows_compat.h"
#include "audio/audio_runtime.h"
#include "ima_adpcm.h"
#include "spu_core.h"
#include "vab.h"

/* Variables. */
static CRITICAL_SECTION audio_lock;

static volatile LONG audio_initialized;

static sint32 audio_tick_mode;

static uint32 audio_tick_accumulator;

static uint32 audio_tick_count;

static sint32 audio_ticks_running;

typedef struct AUDIO_MUSIC_STATE
{
    uint8 *data;
    IMA_ADPCM_STREAM stream;
    sint32 volume;
    sint32 active;
    sint32 paused;
} AUDIO_MUSIC_STATE;

static AUDIO_MUSIC_STATE music;

static sint16 mix_sample(sint16 destination, sint16 source, sint32 volume)
{
    sint32 mixed = destination + ((sint32)source * volume) / 127;
    if (mixed < -32768)
        return -32768;
    if (mixed > 32767)
        return 32767;
    return (sint16)mixed;
}

/* Functions. */
void audio_runtime_init(void)
{
    if (InterlockedCompareExchange(&audio_initialized, 1, 0) != 0)
        return;
    InitializeCriticalSection(&audio_lock);
    spu_core_init();
    vab_init();
    audio_tick_mode = 0;
    audio_tick_accumulator = 0;
    audio_tick_count = 0;
    audio_ticks_running = 0;
    memset(&music, 0, sizeof(music));
}

void audio_runtime_shutdown(void)
{
    if (InterlockedCompareExchange(&audio_initialized, 0, 1) != 1)
        return;
    EnterCriticalSection(&audio_lock);
    free(music.data);
    memset(&music, 0, sizeof(music));
    vab_shutdown();
    spu_core_shutdown();
    LeaveCriticalSection(&audio_lock);
    DeleteCriticalSection(&audio_lock);
}

sint32 audio_runtime_is_initialized(void)
{
    return InterlockedCompareExchange(&audio_initialized, 1, 1) != 0;
}

void audio_runtime_render(sint16 *stereo, uint32 frames)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_render(stereo, frames);
    if (music.active && !music.paused)
    {
        sint16 decoded[256 * 2];
        uint32 offset = 0;
        while (offset < frames)
        {
            uint32 requested = frames - offset;
            uint32 produced;
            uint32 sample;
            if (requested > 256)
                requested = 256;
            produced = ima_adpcm_decode(&music.stream, decoded, requested);
            for (sample = 0; sample < produced * 2; ++sample)
                stereo[offset * 2 + sample] = mix_sample(stereo[offset * 2 + sample], decoded[sample], music.volume);
            offset += produced;
            if (produced < requested)
            {
                music.active = 0;
                break;
            }
        }
    }
    if (audio_ticks_running)
    {
        /* SsSetTickMode(1) selects the 60 Hz libsnd maintenance cadence.
         * Native attributes are committed synchronously, so this clock has
         * no sequence parser work but retains the exact 735-sample cadence. */
        audio_tick_accumulator += frames;
        while (audio_tick_accumulator >= SPU_sample_rate / 60u)
        {
            audio_tick_accumulator -= SPU_sample_rate / 60u;
            ++audio_tick_count;
        }
    }
    LeaveCriticalSection(&audio_lock);
}

sint32 audio_runtime_music_play(uint8 *data, uint32 size, sint32 volume)
{
    if (!audio_runtime_is_initialized())
    {
        free(data);
        return 0;
    }
    EnterCriticalSection(&audio_lock);
    free(music.data);
    memset(&music, 0, sizeof(music));
    music.data = data;
    if (volume < 0)
        volume = 0;
    if (volume > 127)
        volume = 127;
    music.volume = volume;
    if (data != 0 && ima_adpcm_open(&music.stream, data, size) && music.stream.sample_rate == SPU_sample_rate && music.stream.channels == 2)
    {
        music.active = 1;
        LeaveCriticalSection(&audio_lock);
        return 1;
    }
    else
    {
        free(music.data);
        memset(&music, 0, sizeof(music));
    }
    LeaveCriticalSection(&audio_lock);
    return 0;
}

void audio_runtime_music_set_paused(sint32 paused)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    music.paused = paused != 0;
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_music_set_volume(sint32 volume)
{
    if (!audio_runtime_is_initialized())
        return;
    if (volume < 0)
        volume = 0;
    if (volume > 127)
        volume = 127;
    EnterCriticalSection(&audio_lock);
    music.volume = volume;
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_music_stop(void)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    free(music.data);
    memset(&music, 0, sizeof(music));
    LeaveCriticalSection(&audio_lock);
}

sint16 audio_runtime_vab_open_head(const uint8 *header, sint16 requested_bank)
{
    sint16 result;
    if (!audio_runtime_is_initialized())
        return -1;
    EnterCriticalSection(&audio_lock);
    result = vab_open_head(header, requested_bank);
    LeaveCriticalSection(&audio_lock);
    return result;
}

sint16 audio_runtime_vab_transfer_body(const uint8 *body, sint16 bank)
{
    sint16 result;
    if (!audio_runtime_is_initialized())
        return -1;
    EnterCriticalSection(&audio_lock);
    result = vab_transfer_body(body, bank);
    LeaveCriticalSection(&audio_lock);
    return result;
}

void audio_runtime_set_voice_registers(sint32 voice, const SPU_voice_registers *registers)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_set_voice_registers(voice, registers);
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_set_voice_volume(sint32 voice, sint16 left, sint16 right)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_set_voice_volume(voice, left, right);
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_set_voice_pitch(sint32 voice, uint16 pitch)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_set_voice_pitch(voice, pitch);
    LeaveCriticalSection(&audio_lock);
}

sint32 audio_runtime_get_voice_registers(sint32 voice, SPU_voice_registers *registers)
{
    sint32 result;
    if (!audio_runtime_is_initialized())
        return 0;
    EnterCriticalSection(&audio_lock);
    result = spu_core_get_voice_registers(voice, registers);
    LeaveCriticalSection(&audio_lock);
    return result;
}

void audio_runtime_key_on(uint32 voice_mask)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_key_on(voice_mask);
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_key_off(uint32 voice_mask)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_key_off(voice_mask);
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_set_master_volume(sint16 left, sint16 right)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    spu_core_set_master_volume(left, right);
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_set_tick_mode(sint32 mode)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    audio_tick_mode = mode;
    audio_tick_accumulator = 0;
    LeaveCriticalSection(&audio_lock);
}

void audio_runtime_start_ticks(void)
{
    if (!audio_runtime_is_initialized())
        return;
    EnterCriticalSection(&audio_lock);
    audio_ticks_running = (audio_tick_mode == 1);
    LeaveCriticalSection(&audio_lock);
}

uint32 audio_runtime_tick_count(void)
{
    uint32 result;
    if (!audio_runtime_is_initialized())
        return 0;
    EnterCriticalSection(&audio_lock);
    result = audio_tick_count;
    LeaveCriticalSection(&audio_lock);
    return result;
}

SPU_adsr_phase audio_runtime_voice_phase(sint32 voice)
{
    SPU_adsr_phase result;
    if (!audio_runtime_is_initialized())
        return SPU_adsr_off;
    EnterCriticalSection(&audio_lock);
    result = spu_core_voice_phase(voice);
    LeaveCriticalSection(&audio_lock);
    return result;
}

uint16 audio_runtime_voice_envelope(sint32 voice)
{
    uint16 result;
    if (!audio_runtime_is_initialized())
        return 0;
    EnterCriticalSection(&audio_lock);
    result = spu_core_voice_envelope(voice);
    LeaveCriticalSection(&audio_lock);
    return result;
}
