#ifndef MODULE_API_INPUT_H
#define MODULE_API_INPUT_H

#include "xport.h"
#include "psx.h"

extern sint32 g_debug_cheats_enabled;

typedef struct INPUT_STATE
{
    uint32 held, prev, pressed;
    char pause_latched;
    /* Shared PsyQ digital reports retain their eight-byte device layout */
    uint8 packet[2][8];
} INPUT_STATE;

extern __declspec(dllexport) INPUT_STATE g_input;
void input_release(void);
sint16 pad_packet_is_digital_pad(const uint8 *packet);

/* BEGIN GENERATED MODULE API */
GDB_CALL uint32 pads_read(void);
/* END GENERATED MODULE API */

#endif
