#ifndef MODULE_API_MEMORY_CARD_PLATFORM_H
#define MODULE_API_MEMORY_CARD_PLATFORM_H

#include "xport.h"
#include "psx.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL sint16 hq_save_payload_deserialize(void *payload);
GDB_CALL sint16 hq_unlocked_map_node_count(void);
GDB_CALL sint16 memory_card_entry_map_build(void *card, uint8 *map, sint32 mode, const void *layout);
GDB_CALL sint16 memory_card_event_poll(sint32 event, sint32 mode);
GDB_CALL sint32 memory_card_file_close(sint32 handle);
GDB_CALL sint32 memory_card_file_delete(const char *name);
GDB_CALL sint32 memory_card_file_open(const char *name, sint32 mode);
GDB_CALL sint32 memory_card_file_read(sint32 handle, void *data, sint32 size);
GDB_CALL sint32 memory_card_file_write(sint32 handle, const void *data, sint32 size);
GDB_CALL sint32 memory_card_usable_entry_count(void *card);
GDB_CALL void hq_memory_card_error_acknowledge(void);
GDB_CALL void hq_save_payload_serialize(void *payload);
GDB_CALL void memory_card_context_select_slot(void *card, sint32 slot);
GDB_CALL void memory_card_directory_scan(void *card);
GDB_CALL void memory_card_grid_render(void *card, uint8 *map, uint8 selected, sint32 columns);
GDB_CALL void memory_card_header_build(void *payload, const void *header);
GDB_CALL void memory_card_status_refresh(void *card);
GDB_CALL void memory_card_system_initialize(void);
GDB_CALL void memory_card_system_shutdown(void);
GDB_CALL void vertical_sync_wait(sint32 mode);
/* END GENERATED MODULE API */

#endif
