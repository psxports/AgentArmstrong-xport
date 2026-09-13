#ifndef IMA_ADPCM_H
#define IMA_ADPCM_H

#include "app.h"

typedef struct IMA_ADPCM_STREAM
{
    const uint8 *file_data;
    uint32 file_size;
    const uint8 *audio_data;
    uint32 audio_size;
    uint32 total_frames;
    uint32 decoded_frames;
    uint32 block_offset;
    uint32 sample_in_block;
    uint16 block_align;
    uint16 samples_per_block;
    uint16 channels;
    uint32 sample_rate;
    sint32 predictor[2];
    sint32 step_index[2];
    sint32 block_loaded;
} IMA_ADPCM_STREAM;

sint32 ima_adpcm_open(IMA_ADPCM_STREAM *stream, const uint8 *data, uint32 size);
uint32 ima_adpcm_decode(IMA_ADPCM_STREAM *stream, sint16 *stereo, uint32 frames);

#endif
