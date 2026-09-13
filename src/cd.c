#include <stdio.h>
#include "cd.h"
#include "global.h"

/* PAL 0x800B85F4 is the BIOS DeliverEvent gate (t1=7, jump 0xB0).
 * Windows has no BIOS event queue; retain its complete observable arguments
 * at the platform boundary so the three original callback adapters remain
 * independently testable. */
__declspec(dllexport) uint32 g_psx_cd_event_class, g_psx_cd_event_spec, g_psx_cd_event_count;

/* Original: FUN_800B82A4. */
GDB_CALL sint32 cd_track_table_read(void *workspace)
{
    return psx_cd_read_track_table(1, workspace);
}

/* Original: FUN_800B8644. */
GDB_CALL sint32 cd_driver_control(sint32 mode)
{
    if (mode == 2)
    {
        psx_cd_stop_driver();
        return 1;
    }
    if (psx_cd_start_driver() != 0)
        return 0;
    if (mode == 1)
        return psx_cd_configure_driver() == 0;
    return 1;
}

/* Original: FUN_800B8790. */
GDB_CALL CD_CALLBACK cd_ready_callback_set(CD_CALLBACK callback)
{
    CD_CALLBACK previous = g_cd_ready_callback;
    g_cd_ready_callback = callback;
    return previous;
}

/* Original: FUN_800B87A8. */
GDB_CALL CD_CALLBACK cd_sync_callback_set(CD_CALLBACK callback)
{
    CD_CALLBACK previous = g_cd_sync_callback;
    g_cd_sync_callback = callback;
    return previous;
}

/* Original: FUN_800BB404. */
GDB_CALL CD_CALLBACK cd_data_callback_set(CD_CALLBACK callback)
{
    CD_CALLBACK previous = g_cd_data_callback;
    g_cd_data_callback = callback;
    return previous;
}

/* Original: FUN_800B85F4. */
static void cd_bios_event_deliver(uint32 event_class, uint32 event_spec)
{
    g_psx_cd_event_class = event_class;
    g_psx_cd_event_spec = event_spec;
    ++g_psx_cd_event_count;
}

/* Original: FUN_800B857C. */
GDB_CALL void cd_ready_event_callback(void)
{
    cd_bios_event_deliver(0xf0000003u, 0x20);
}

/* Originals: SLES_004.74:FUN_800B85A4 (800B85A4..800B85CC),
 * SLES_004.74:FUN_800B85CC (800B85CC..800B85F4). Byte-identical.
 * Former native names: cd_sync_event_callback, cd_data_event_callback.
 * Current callers only install/invoke callbacks; no pointer-identity comparison. */
GDB_CALL void cd_completion_event_callback(void)
{
    cd_bios_event_deliver(0xf0000003u, 0x40);
}

/* Original: FUN_800A99C4. */
GDB_CALL void cd_track_table_initialize(void)
{
    g_cd_track_table_status = cd_track_table_read(g_cd_track_table);
}

/* Original: FUN_800B84EC. */
GDB_CALL sint32 cd_initialize(void)
{
    sint32 retries = 4;
    do
    {
        if (cd_driver_control(1) == 1)
        {
            cd_ready_callback_set(cd_ready_event_callback);
            cd_sync_callback_set(cd_completion_event_callback);
            cd_data_callback_set(cd_completion_event_callback);
            return 1;
        }
        --retries;
    } while (retries != -1);
    printf("CdInit: Init failed\n");
    return 0;
}
