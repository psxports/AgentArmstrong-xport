#include <stdlib.h>
#include "global.h"
#include "object.h"
#include "psx.h"
#include "psx_pad.h"

static sint32 text_equal_ignore_case(const char *left, const char *right)
{
    uint8 a;
    uint8 b;
    do
    {
        a = (uint8)*left++;
        b = (uint8)*right++;
        if (a >= 'A' && a <= 'Z')
            a = (uint8)(a + ('a' - 'A'));
        if (b >= 'A' && b <= 'Z')
            b = (uint8)(b + ('a' - 'A'));
        if (a != b)
            return 0;
    } while (a != 0);
    return 1;
}

GDB_CALL uint32 controllers_read(sint32 controller)
{
    static sint32 direct_pad_initialized;
    uint32 buttons = 0;
    /* The PSX routine ignored its argument and packed pads 1/2 into the low
     * and high 16 bits.  Hold Right Shift to route the I/J/K/L face-button
     * cluster to pad 2; the original cheat sequences contain pad-2 masks. */

    if (!direct_pad_initialized)
    {
        PadInitDirect(g_controller_packet, 0);
        PadStartCom();
        direct_pad_initialized = 1;
    }

    if (getenv("OA_MUZZLE_LIGHT_TRACE") != 0)
    {
        /* Audit-only deterministic Square hold used by the paired native
         * muzzle-light CDB capture. */
        buttons = PADRleft;
    }
    else if (getenv("OA_COMPLETION_TRACE") != 0)
    {
        /* Deterministic audit input only: hold Circle for the flare, then
         * navigate the original completion menu and confirm its requested
         * branch.  No completion state is written here. */
        static sint32 reads, menu_wait, menu_presses, release, confirmed;
        const char *mode = getenv("OA_COMPLETION_TRACE");
        if (g_player == 0 || g_effect_count != 109)
        {
            buttons = 0;
        }
        else if (reads < 180)
            buttons = PADRright;
        else if (g_next_stage_index != 0)
        {
            sint32 wanted = text_equal_ignore_case(mode, "hq") ? 2 : 1;
            /* Let FUN_800894b4 establish its initial selection and input
             * baseline before producing any menu edge. */
            if (menu_wait < 16)
                ++menu_wait;
            else if (release)
                release = 0;
            else if (menu_presses < wanted)
            {
                buttons = PADLdown;
                ++menu_presses;
                release = 1;
            }
            else if (!confirmed)
            {
                buttons = PADRdown;
                confirmed = 1;
            }
        }
        if (g_player != 0 && g_effect_count == 109)
            ++reads;
    }
    else if (getenv("OA_MISSION_LAUNCH_TRACE") != 0)
    {
        static sint32 reads;
        if ((reads >= 5 && reads < 9) || (reads >= 180 && reads < 184) || (reads >= 260 && reads < 264))
            buttons = PADRdown;
        ++reads;
    }
    else
        buttons = PadRead(controller);

    /* Preserve the original conflict resolution from FUN_800AED64. */
    if (buttons & PADLleft)
        buttons &= ~PADLright;
    if (buttons & PADLdown)
        buttons &= ~PADLup;
    pad_publish(0, 1, (uint16)~buttons);
    return buttons;
}
