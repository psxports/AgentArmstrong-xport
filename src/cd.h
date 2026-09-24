#ifndef CD_H
#define CD_H

#include "global.h"

extern __declspec(dllexport) uint32 g_psx_cd_event_class;
extern __declspec(dllexport) uint32 g_psx_cd_event_spec;
extern __declspec(dllexport) uint32 g_psx_cd_event_count;

GDB_CALL sint32 cd_track_table_read(void *workspace);
GDB_CALL sint32 cd_driver_control(sint32 mode);
GDB_CALL CD_CALLBACK cd_ready_callback_set(CD_CALLBACK callback);
GDB_CALL CD_CALLBACK cd_sync_callback_set(CD_CALLBACK callback);
GDB_CALL CD_CALLBACK cd_data_callback_set(CD_CALLBACK callback);
GDB_CALL void cd_ready_event_callback(void);
GDB_CALL void cd_completion_event_callback(void);
GDB_CALL void cd_track_table_initialize(void);
GDB_CALL sint32 cd_initialize(void);

#endif
