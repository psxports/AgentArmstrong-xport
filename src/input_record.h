#ifndef MODULE_API_INPUT_RECORD_H
#define MODULE_API_INPUT_RECORD_H

#include "xport.h"
#include "psx.h"

typedef enum INPUT_RECORD_MODE
{
    INPUT_RECORD_NONE,
    INPUT_RECORD_WRITE,
    INPUT_RECORD_PLAYBACK
} INPUT_RECORD_MODE;

typedef struct INPUT_RECORD_STATE
{
    /* Originals: DAT_800D41D8, DAT_800D3F58, DAT_800D3F78, DAT_800D418C */
    uint8 *data;
    uint32 size, cursor;
    sint32 run_len, run_val;
    INPUT_RECORD_MODE mode;
} INPUT_RECORD_STATE;

extern INPUT_RECORD_STATE g_input_rec;
extern const char g_input_playback_label[5];
void input_rec_heap_reset(void);

/* BEGIN GENERATED MODULE API */
sint32 input_playback_next(void);
void input_rec_append(sint32 input);
/* END GENERATED MODULE API */

#endif
