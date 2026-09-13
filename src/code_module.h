#ifndef CODE_MODULE_H
#define CODE_MODULE_H

#include <stddef.h>

#include "app.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL void *fixed_address_copy(void *destination, const void *source, sint32 size);
GDB_CALL void overbins_load(void);
void *overbins_data(sint32 *size_out);
GDB_CALL void overlay_module_load(char *filename);
GDB_CALL void game_psx_flush_cache(void *user);
const void *win_code_module_address(uint32 psx_address);
/* END GENERATED MODULE API */

#endif
