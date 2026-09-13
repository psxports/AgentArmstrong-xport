#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "../effect_update.h"
#include "../global.h"
#include "../object.h"
#include "../original_file.h"
#include "../player.h"
#include "../random.h"
#include "../runtime_heap.h"
#include "../stubs.h"
#include "app.h"
#include "audio/game_sound.h"
#include "audio/psyq_sound.h"
#include "audio/vab.h"
#include "cc_archive.h"
#include "collision.h"
#include "player.h"
#include "psyq_sound.h"
#include "vab.h"

/* Types. */
typedef struct SOUND_MAP_ENTRY
{
    uint8 sound_code;
    uint8 bank;
    uint8 program;
} SOUND_MAP_ENTRY;

typedef struct
{
    COLLISION collision; /* +0x00 */
    uint32 field_78;     /* +0x78 */
    sint16 speech_id;    /* +0x7C */
    sint16 delay_ticks;  /* +0x7E */
    sint16 voice_handle; /* +0x80 */
    uint16 field_82;     /* +0x82 */
} SPEECH_PLAYBACK;

#if defined(AP_32BIT)
typedef char SpeechPlaybackObject_size_84[sizeof(SPEECH_PLAYBACK) == 0x84 ? 1 : -1];
typedef char SpeechPlaybackObject_id_at_7c[offsetof(SPEECH_PLAYBACK, speech_id) == 0x7C ? 1 : -1];
typedef char SpeechPlaybackObject_delay_at_7e[offsetof(SPEECH_PLAYBACK, delay_ticks) == 0x7E ? 1 : -1];
typedef char SpeechPlaybackObject_handle_at_80[offsetof(SPEECH_PLAYBACK, voice_handle) == 0x80 ? 1 : -1];
#endif

/* Variables. */
static const uint8 original_sound_codes[294] = {
    0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 37, 38, 39, 48, 49, 50, 51, 52, 53, 54, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 65, 40, 41, 52, 53, 70, 71, 72, 73, 32, 33, 34, 35, 36, 65, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 52, 53, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 54, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146,
    147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 42, 43, 44, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 0,  1,  2,   3,   4,   5,   6,   7,   8,  9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  40,  41,  42,  43,  44,  37,  38,  39,  48,  49,  50,  51,  52,  53,  54,  0,   0,
};

static const char *const bank_names[28] = {"AMAIN1", "ABIKE1", "DOCKDAM1", "ENEMY1", "TRCK", "ROB", "AGNTSND1", "GYRO", "FLAME1", "TNK", "AHQ", "JETPACK1", "BIGMAN1", "GUNEMP1", "BFAL", "ENWS", "UWAT", "MAIN", "DOCK", "AIRF", "JNGL", "INDU", "CAVE", "SNWS", "FNWS", "MRF3", "MRF5", "MRF13"};

static SOUND_MAP_ENTRY sound_map[294];

static SOUND_VOLUME_PAIR spatial_volume;

static SPU_voice_attributes voice_attr;

/* Functions. */
__declspec(dllexport) volatile uint32 g_psx_spu_live_volume_calls;

__declspec(dllexport) volatile uint32 g_psx_spu_pitch_update_calls;

__declspec(dllexport) volatile uint32 g_psx_speech_object_calls;

__declspec(dllexport) volatile uint32 g_psx_speech_playback_calls;

__declspec(dllexport) volatile uint32 g_psx_spu_status_calls;

/* Original: FUN_800AA1FC. */
void sound_map_reset(void)
{
    sint32 index;
    for (index = 0; index < 294; ++index)
    {
        sound_map[index].sound_code = original_sound_codes[index];
        sound_map[index].bank = 0xff;
    }
}

/* Original: FUN_800AA000. */
void sound_bank_load(sint32 bank_index, sint32 first_sound)
{
    char header_name[32];
    char body_path[64];
    const uint8 *header;
    uint8 *body;
    sint32 body_size;
    sint16 bank;
    uint16 program_count;
    sint32 program;
    if (bank_index < 0 || bank_index >= 28)
        return;
    strcpy(header_name, bank_names[bank_index]);
    strcat(header_name, ".VH");
    header = (const uint8 *)archive_member_find(header_name, g_sound_archive);
    if (!header)
    {
        fatal_error("VAB ERROR");
        return;
    }
    program_count = (uint16)(header[18] | ((uint16)header[19] << 8));
    bank = psyq_sound_vab_open_head(header, -1);
    if (bank == -1)
    {
        fatal_error("VAB ERROR");
        return;
    }
    strcpy(body_path, "SOUND\\");
    strcat(body_path, bank_names[bank_index]);
    strcat(body_path, ".VB");
    body_size = game_file_size(body_path);
    if (body_size != (sint32)vab_get_body_size(bank))
    {
        fatal_error("VAB BODY ERROR");
        return;
    }
    body = (uint8 *)malloc((uint32)body_size);
    if (!body)
    {
        fatal_error("VAB MEMORY");
        return;
    }
    game_file_read(body_path, body);
    if (psyq_sound_vab_transfer_body(body, bank) == -1 || !psyq_sound_vab_transfer_completed(1))
        fatal_error("VAB TRANSFER");
    free(body);
    runtime_heap_sweep();
    for (program = 0; program < program_count && first_sound + program < 294; ++program)
    {
        sound_map[first_sound + program].bank = (uint8)bank;
        sound_map[first_sound + program].program = (uint8)program;
    }
}

static void resolve_duplicate_sounds(void)
{
    sint32 destination, source;
    for (destination = 0; destination < 294; ++destination)
    {
        if (sound_map[destination].bank != 0xff)
            continue;
        for (source = 0; source < 294; ++source)
        {
            if (sound_map[source].sound_code == sound_map[destination].sound_code && sound_map[source].bank != 0xff)
            {
                sound_map[destination].bank = sound_map[source].bank;
                sound_map[destination].program = sound_map[source].program;
                break;
            }
        }
    }
}

/* Original: FUN_800AA224. */
void stage_sound_banks_load(void)
{
    sint32 level;
    sound_map_reset();
    if (g_attract_mode)
    {
        game_file_read("SOUND\\PVH.CC", g_sound_archive);
        if (g_language_id == 1)
            sound_bank_load(15, 111);
        if (g_language_id == 4)
            sound_bank_load(23, 111);
        if (g_language_id == 2)
            sound_bank_load(24, 111);
        DAT_800d3d70 = DAT_800d3d72 = g_recent_speech_write_index = g_ambient_speech_timer = 0;
        memset(g_recent_speech_samples, 0, sizeof(sint16) * 4);
    }
    else
    {
        game_file_read("SOUND\\VH.CC", g_sound_archive);
        level = g_stage_index;
        if (g_scuba_stage_active)
        {
            sound_bank_load(16, 152);
        }
        else
        {
            if (level != 3 && level != 5 && (uint32)(level - 12) >= 4 && level != 17)
                sound_bank_load(17, 0);
            if (level == 3)
                sound_bank_load(25, 170);
            if (level == 5)
                sound_bank_load(26, 205);
            if (level < 2 || level == 2 || (uint32)(level - 7) < 2)
                sound_bank_load(18, 32);
            if ((uint32)(level - 12) < 2 || (uint32)(level - 14) < 2 || level == 17)
                sound_bank_load(27, 245);
            if ((uint32)(level - 9) < 2 || (uint32)(level - 22) < 2 || (uint32)(level - 24) < 2)
                sound_bank_load(20, 55);
            if ((uint32)(level - 27) < 2 || level == 29 || (uint32)(level - 31) < 2)
                sound_bank_load(21, 66);
            if (level == 11)
                sound_bank_load(10, 104);
            if (level == 1)
                sound_bank_load(4, 80);
            if (level == 4)
                sound_bank_load(5, 84);
            if (level == 8)
                sound_bank_load(7, 96);
            if ((uint32)(level - 12) < 2 || level == 2 || level == 15)
                sound_bank_load(14, 106);
            if (effect_find_next(54, 0))
                sound_bank_load(9, 100);
        }
    }
    resolve_duplicate_sounds();
    psyq_sound_set_tick_mode(1);
    psyq_sound_start();
    runtime_heap_sweep();
    psyq_sound_set_master_volume(127, 127);
}

/* Original: FUN_800AA64C. */
sint32 sound_play_nonpositional(sint32 id, sint32 note_delta, sint32 volume)
{
    sint32 scaled;
    if (g_attract_mode || id < 0 || id >= 294 || sound_map[id].bank == 0xff)
        return -1;
    scaled = volume * g_sfx_playback_volume;
    if (scaled < 0)
        scaled += 127;
    scaled >>= 7;
    return psyq_sound_key_on(sound_map[id].bank, sound_map[id].program, 0, (sint16)(note_delta + 60), 0, (sint16)scaled, (sint16)scaled);
}

/* Original: FUN_800AA6F0. */
SOUND_VOLUME_PAIR *sound_spatial_volume_calculate(sint32 type, sint32 x, sint32 y, sint32 z)
{
    sint32 dz = z - g_camera_world_z, dx = x - g_camera_world_x, dy = (y - g_camera_world_y) / 2;
    sint32 distance = (dz < 0 ? -dz : dz) + (dx < 0 ? -dx : dx) / 2 + (dy < 0 ? -dy : dy);
    sint32 volume = type - distance / 2560, screen, span, ratio, left, right;
    if ((sint16)volume <= 0 || dz == 0)
        return 0;
    if ((sint16)volume >= 128)
        volume = 127;
    span = 40960 / volume;
    screen = 320 * dx / dz;
    if (span <= 0)
        return 0;
    ratio = ((screen < 0 ? -screen : screen) << 8) / span;
    left = volume;
    right = volume;
    /* 800AA868/800AA878 are subu channel,base,pan_delta.  The old port
     * followed a bad decompiler expression (-1-pan_delta), which forced the
     * attenuated side below zero and the following clamp muted it entirely. */
    if (screen <= 0)
        left = volume - (sint16)ratio;
    else
        right = volume - (sint16)ratio;
    left = left * g_sfx_playback_volume / 128;
    right = right * g_sfx_playback_volume / 128;
    if (left < 0)
        left = 0;
    if (right < 0)
        right = 0;
    if (!(left | right))
        return 0;
    spatial_volume.volume_left = (sint16)(right << 7);
    spatial_volume.volume_right = (sint16)(left << 7);
    return &spatial_volume;
}

/* Original: FUN_800AA8FC. */
uint16 sound_play_positional(sint32 id, sint32 note_delta, sint32 type, sint32 x, sint32 y, sint32 z)
{
    SOUND_VOLUME_PAIR *volume;
    sint32 left, right;
    if (g_attract_mode || id < 0 || id >= 294 || sound_map[id].bank == 0xff)
        return 0xffff;
    volume = sound_spatial_volume_calculate(type, x, y, z);
    if (!volume)
        return 0xffff;
    left = volume->volume_left;
    if (left < 0)
        left += 127;
    left >>= 7;
    right = volume->volume_right;
    if (right < 0)
        right += 127;
    right >>= 7;
    return (uint16)psyq_sound_key_on(sound_map[id].bank, sound_map[id].program, 0, (sint16)(note_delta + 60), 0, (sint16)left, (sint16)right);
}

/* Original: FUN_800AAA7C. */
SPU_voice_attributes *sound_voice_spatial_volume_update(SOUND_VOLUME_PAIR *volume, sint32 handle)
{
    sint32 left, right;
    SPU_voice_registers registers;
    if (handle < 0 || handle >= SPU_voice_count || !volume)
        return 0;
    ++g_psx_spu_live_volume_calls;
    left = volume->volume_left * g_sfx_playback_volume;
    if (left < 0)
        left += 127;
    left >>= 7;
    right = volume->volume_right * g_sfx_volume_setting;
    if (right < 0)
        right += 127;
    right >>= 7;
    memset(&voice_attr, 0, sizeof(voice_attr));
    voice_attr.voice_mask = 1u << handle;
    voice_attr.attribute_mask = 3;
    voice_attr.volume_left = (sint16)left;
    voice_attr.volume_right = (sint16)right;
    psyq_sound_set_voice_volume((sint16)handle, (sint16)left, (sint16)right);
    if (psyq_sound_get_voice_registers((sint16)handle, &registers))
    {
        voice_attr.start_address = registers.start_address;
        voice_attr.repeat_address = registers.repeat_address;
        voice_attr.pitch = registers.pitch;
        voice_attr.envelope = psyq_sound_get_voice_envelope((sint16)handle);
    }
    return &voice_attr;
}

/* Original: FUN_800AA9DC. */
void sound_voice_stop(void *raw_handle)
{
    sint16 *handle = (sint16 *)raw_handle;
    sint32 id = g_scuba_stage_active ? 163 : 11;
    if (!handle || *handle == -1 || sound_map[id].bank == 0xff)
        return;
    if (psyq_sound_key_on_voice(*handle, sound_map[id].bank, sound_map[id].program, 0, 127, 0, 0, 0) != -1)
        *handle = -1;
}

/* 800AAB40 is the one-call game wrapper around SpuSetVoiceAttr.  The native
 * compatibility layer commits attributes synchronously at FUN_800AAA7C, so
 * there is no deferred hardware call left at this boundary. */
/* Original: FUN_800AAB40. */
void sound_voice_attributes_apply(SPU_voice_attributes *attr)
{
    sint16 voice;
    if (!attr || attr->voice_mask == 0)
        return;
    for (voice = 0; voice < SPU_voice_count; ++voice)
        if (attr->voice_mask & (1u << voice))
        {
            if (attr->attribute_mask & 3u)
                psyq_sound_set_voice_volume(voice, attr->volume_left, attr->volume_right);
            if (attr->attribute_mask & 0x10u)
            {
                psyq_sound_set_voice_pitch(voice, attr->pitch);
                ++g_psx_spu_pitch_update_calls;
            }
        }
}

/* Original: FUN_800AAB60. */
sint32 sound_voice_status_get(sint8 voice)
{
    ++g_psx_spu_status_calls;
    return psyq_sound_get_voice_status(voice);
}

/* Original: FUN_800AADAC. */
void speech_playback_update(SPEECH_PLAYBACK *object)
{
    sint16 handle;
    sint16 note;
    sint16 timer = (sint16)((uint16)object->delay_ticks + 1u);
    object->delay_ticks = timer;
    if (timer == 30)
    {
        note = (g_language_id == 4 || g_language_id == 2) ? 0 : 3;
        g_attract_mode = 0;
        handle = (sint16)sound_play_nonpositional(object->speech_id, note, 127);
        object->voice_handle = handle;
        ++g_psx_speech_playback_calls;
        g_attract_mode = 1;
    }
    else
    {
        handle = object->voice_handle;
        if (handle != -1 && sound_voice_status_get((sint8)handle) == 0)
            object_destroy(object);
    }
}

/* Exact game-side speech selector at 0x800AABF8..0x800AADA8. */
/* Original: FUN_800AABF8. */
void speech_random_request(sint32 kind)
{
    sint16 candidates[16];
    const uint16 *list;
    SPEECH_PLAYBACK *object;
    sint16 candidate_count = 0;
    sint16 selected = -1;
    sint16 count, index, recent;
    sint32 random;
    if (!g_attract_mode || kind < 0 || kind >= 15)
        return;
    if (object_find_next_by_type(0, 0x61) != 0)
        return;
    list = (const uint16 *)player_assets_executable_pointer(0x800cbab4u + (uint32)kind * 4u);
    if (!list)
        return;
    count = (sint16)list[0];
    for (index = 0; index < count; ++index)
    {
        sint16 sound = (sint16)list[index + 1];
        for (recent = 0; recent < 4; ++recent)
            if (sound == g_recent_speech_samples[recent])
                break;
        if (recent == 4 && candidate_count < 16)
            candidates[candidate_count++] = sound;
    }
    if (candidate_count != 0)
    {
        random = secondary_random_range(candidate_count) - 1;
        if ((sint16)random < 0)
            random = 0;
        selected = candidates[(sint16)random];
        object = (SPEECH_PLAYBACK *)object_create(sizeof(*object), (FUNC_COLLISION_UPDATE)speech_playback_update);
        object->collision.object_type = 0x61;
        object->speech_id = selected;
        object->delay_ticks = 0;
        object->voice_handle = -1;
        g_ambient_speech_timer = 0;
        ++g_psx_speech_object_calls;
    }
    g_recent_speech_samples[(uint16)g_recent_speech_write_index & 3u] = selected;
    g_recent_speech_write_index = (sint16)(((uint16)g_recent_speech_write_index + 1u) & 3u);
}
