#include <string.h>
#include "global.h"
#include "object.h"
#include "game_runtime.h"
#include "psx.h"

/* Functions. */
/* Host boundaries used by the exact HQ memory-card state machine.
 * The default host has no attached PSX card; integrations can replace these
 * exported boundaries without changing game-side state transitions. */

__declspec(dllexport) volatile uint32 g_psx_card_boundary_calls;

static void card_boundary(void)
{
    ++g_psx_card_boundary_calls;
}

/* Original: FUN_800AF1CC. */
GDB_CALL void memory_card_system_initialize(void)
{
    card_boundary();
}

/* Original: FUN_800AF184. */
GDB_CALL void memory_card_context_select_slot(void *card, sint32 slot)
{
    card_boundary();
    if (card != 0)
        memset(card, 0, 0x624);
}

/* Original: FUN_800AFB78. */
GDB_CALL void memory_card_directory_scan(void *card)
{
    card_boundary();
}

/* Original: FUN_800AFB18. */
GDB_CALL sint32 memory_card_usable_entry_count(void *card)
{
    card_boundary();
    return 0;
}

/* Original: FUN_800AF428. */
GDB_CALL sint16 memory_card_event_poll(sint32 event, sint32 mode)
{
    card_boundary();
    return 0;
}

/* Original: FUN_800AF65C. */
GDB_CALL sint16 memory_card_entry_map_build(void *card, uint8 *map, sint32 mode, const void *layout)
{
    card_boundary();
    return card == 0 ? 0 : ((uint8 *)card)[0x61a];
}

/* Original: FUN_800AF778. */
GDB_CALL void memory_card_grid_render(void *card, uint8 *map, uint8 selected, sint32 columns)
{
    card_boundary();
}

/* Original: FUN_800AFD10. */
GDB_CALL void memory_card_status_refresh(void *card)
{
    card_boundary();
}

/* Original: FUN_800AF3A0. */
GDB_CALL void memory_card_system_shutdown(void)
{
    card_boundary();
}

/* Original: FUN_800FDFE8. */
GDB_CALL void hq_memory_card_error_acknowledge(void)
{
    card_boundary();
}

/* Original: FUN_80087D70. */
GDB_CALL sint16 hq_unlocked_map_node_count(void)
{
    card_boundary();
    return 0;
}

/* Original: FUN_800AEFEC. */
GDB_CALL void memory_card_header_build(void *payload, const void *header)
{
    card_boundary();
}

/* Original: FUN_800FE050. */
GDB_CALL void hq_save_payload_serialize(void *payload)
{
    card_boundary();
}

/* Original: FUN_800FE160. */
GDB_CALL sint16 hq_save_payload_deserialize(void *payload)
{
    card_boundary();
    return 0;
}

/* Original: FUN_800B0140. */
GDB_CALL sint32 memory_card_file_open(const char *name, sint32 mode)
{
    card_boundary();
    return -1;
}

/* Original: FUN_800B0150. */
GDB_CALL sint32 memory_card_file_read(sint32 handle, void *data, sint32 size)
{
    card_boundary();
    return -1;
}

/* Original: FUN_800B0160. */
GDB_CALL sint32 memory_card_file_write(sint32 handle, const void *data, sint32 size)
{
    card_boundary();
    return -1;
}

/* Original: FUN_800B0170. */
GDB_CALL sint32 memory_card_file_close(sint32 handle)
{
    card_boundary();
    return -1;
}

/* Original: FUN_800B0180. */
GDB_CALL sint32 memory_card_file_delete(const char *name)
{
    card_boundary();
    return -1;
}

/* Original: FUN_800B1304. */
GDB_CALL void vertical_sync_wait(sint32 mode)
{
    card_boundary();
    VSync(0);
}
