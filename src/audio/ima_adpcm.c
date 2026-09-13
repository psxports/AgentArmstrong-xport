#include <string.h>
#include "global.h"
#include "ima_adpcm.h"
#include "sample.h"

static const sint32 index_table[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static const sint32 step_table[89] = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};

static sint16 decode_nibble(IMA_ADPCM_STREAM *stream, sint32 channel, uint8 nibble)
{
    sint32 step = step_table[stream->step_index[channel]];
    sint32 difference = step >> 3;
    if (nibble & 1)
        difference += step >> 2;
    if (nibble & 2)
        difference += step >> 1;
    if (nibble & 4)
        difference += step;
    if (nibble & 8)
        stream->predictor[channel] -= difference;
    else
        stream->predictor[channel] += difference;
    stream->predictor[channel] = audio_sample_clamp(stream->predictor[channel]);
    stream->step_index[channel] += index_table[nibble & 7];
    if (stream->step_index[channel] < 0)
        stream->step_index[channel] = 0;
    if (stream->step_index[channel] > 88)
        stream->step_index[channel] = 88;
    return (sint16)stream->predictor[channel];
}

static sint32 load_block(IMA_ADPCM_STREAM *stream)
{
    const uint8 *block;
    uint32 header_size = stream->channels * 4u;
    sint32 channel;
    if (stream->block_offset >= stream->audio_size || stream->audio_size - stream->block_offset < header_size)
        return 0;
    block = stream->audio_data + stream->block_offset;
    for (channel = 0; channel < stream->channels; ++channel)
    {
        stream->predictor[channel] = (sint16)read_u16_le(block + channel * 4);
        stream->step_index[channel] = block[channel * 4 + 2];
        if (stream->step_index[channel] > 88)
            return 0;
    }
    stream->sample_in_block = 0;
    stream->block_loaded = 1;
    return 1;
}

sint32 ima_adpcm_open(IMA_ADPCM_STREAM *stream, const uint8 *data, uint32 size)
{
    uint32 offset = 12;
    uint32 fact_frames = 0;
    sint32 have_format = 0;
    memset(stream, 0, sizeof(*stream));
    if (data == 0 || size < 12 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0)
        return 0;
    while (offset + 8 <= size)
    {
        const uint8 *chunk = data + offset;
        uint32 chunk_size = read_u32_le(chunk + 4);
        uint32 payload = offset + 8;
        if (chunk_size > size - payload)
            return 0;
        if (memcmp(chunk, "fmt ", 4) == 0)
        {
            if (chunk_size < 20 || read_u16_le(data + payload) != 0x11)
                return 0;
            stream->channels = read_u16_le(data + payload + 2);
            stream->sample_rate = read_u32_le(data + payload + 4);
            stream->block_align = read_u16_le(data + payload + 12);
            if (read_u16_le(data + payload + 14) != 4 || read_u16_le(data + payload + 16) < 2)
                return 0;
            stream->samples_per_block = read_u16_le(data + payload + 18);
            have_format = 1;
        }
        else if (memcmp(chunk, "fact", 4) == 0 && chunk_size >= 4)
            fact_frames = read_u32_le(data + payload);
        else if (memcmp(chunk, "data", 4) == 0)
        {
            stream->audio_data = data + payload;
            stream->audio_size = chunk_size;
        }
        offset = payload + chunk_size + (chunk_size & 1u);
    }
    if (!have_format || stream->audio_data == 0 || stream->channels == 0 || stream->channels > 2 || stream->sample_rate == 0 || stream->block_align < stream->channels * 4u || stream->samples_per_block == 0)
        return 0;
    stream->file_data = data;
    stream->file_size = size;
    stream->total_frames = fact_frames;
    if (stream->total_frames == 0)
        stream->total_frames = (stream->audio_size / stream->block_align) * stream->samples_per_block;
    return 1;
}

uint32 ima_adpcm_decode(IMA_ADPCM_STREAM *stream, sint16 *stereo, uint32 frames)
{
    uint32 written = 0;
    while (written < frames && stream->decoded_frames < stream->total_frames)
    {
        const uint8 *block;
        uint32 encoded_sample;
        uint32 group;
        uint32 within;
        sint16 sample[2];
        sint32 channel;
        if (!stream->block_loaded && !load_block(stream))
            break;
        block = stream->audio_data + stream->block_offset;
        if (stream->sample_in_block == 0)
        {
            sample[0] = (sint16)stream->predictor[0];
            sample[1] = stream->channels == 2 ? (sint16)stream->predictor[1] : sample[0];
        }
        else
        {
            encoded_sample = stream->sample_in_block - 1;
            group = encoded_sample / 8u;
            within = encoded_sample & 7u;
            for (channel = 0; channel < stream->channels; ++channel)
            {
                uint32 byte_offset = stream->channels * 4u + group * stream->channels * 4u + channel * 4u + within / 2u;
                uint8 byte;
                uint8 nibble;
                if (byte_offset >= stream->block_align || stream->block_offset + byte_offset >= stream->audio_size)
                    return written;
                byte = block[byte_offset];
                nibble = (within & 1u) ? byte >> 4 : byte & 15;
                sample[channel] = decode_nibble(stream, channel, nibble);
            }
            if (stream->channels == 1)
                sample[1] = sample[0];
        }
        stereo[written * 2] = sample[0];
        stereo[written * 2 + 1] = sample[1];
        ++written;
        ++stream->decoded_frames;
        ++stream->sample_in_block;
        if (stream->sample_in_block >= stream->samples_per_block)
        {
            uint32 remaining = stream->audio_size - stream->block_offset;
            stream->block_offset += remaining < stream->block_align ? remaining : stream->block_align;
            stream->block_loaded = 0;
        }
    }
    return written;
}
