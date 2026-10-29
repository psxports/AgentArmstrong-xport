#include <string.h>

#include "code_module.h"
#include "menu.h"
#include "runtime_heap.h"

/* Overlay code and constants are compiled into their native owning modules */
/* Original: SLES_004.74:FUN_8008B5E4. */
GDB_CALL void overlay_module_select(const char *filename)
{
    if (strcmp(filename, "HQ\\HQ.BIN") == 0)
        g_hq_level_menu[2].choice = 0;
    runtime_heap_sweep();
}
