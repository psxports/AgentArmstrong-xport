#include <string.h>

#include "input.h"

sint32 g_debug_cheats_enabled;

__declspec(dllexport) INPUT_STATE g_input;
static sint32 direct_pad_initialized;

/* Exact predicate at 0x800AEED4..0x800AEF5C. */
/* Original: FUN_800AEED4. */
sint16 pad_packet_is_digital_pad(const uint8 *packet)
{
    if (packet[0] == 0xff)
        return 0;
    return ((packet[1] >> 4) == 4);
}

/* The application owns registration and communication for both reports */
void input_release(void)
{
    if (direct_pad_initialized != 0)
        PadStopCom();
    direct_pad_initialized = 0;
    memset(&g_input, 0, sizeof(g_input));
}

/* Original: SLES_004.74:FUN_800AED64. */
GDB_CALL uint32 pads_read(void)
{
    uint16 buttons0, buttons1, report;
    uint32 buttons;
    if (direct_pad_initialized == 0)
    {
        PadInitDirect(g_input.packet[0], g_input.packet[1]);
        PadStartCom();
        direct_pad_initialized = 1;
    }
    buttons0 = PadRead(0);
    buttons1 = PadRead(1);
    /* Match the shared host digital transport's packet byte order */
    report = ~buttons0;
    pad_publish(0, 1, ((report >> 8) | (report << 8)));
    report = ~buttons1;
    pad_publish(1, 1, ((report >> 8) | (report << 8)));
    if (pad_packet_is_digital_pad(g_input.packet[0]) == 0)
    {
        g_input.packet[0][2] = g_input.packet[0][3] = 0xff;
        buttons0 = 0;
    }
    if (pad_packet_is_digital_pad(g_input.packet[1]) == 0)
    {
        g_input.packet[1][2] = g_input.packet[1][3] = 0xff;
        buttons1 = 0;
    }
    buttons = buttons0 | (uint32)buttons1 << 16;
    /* Original direction priority applies only to the first controller */
    if (buttons & PADLleft)
        buttons &= ~PADLright;
    if (buttons & PADLdown)
        buttons &= ~PADLup;
    return buttons;
}
