#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "code_module.h"
#include "global.h"
#include "object.h"
#include "original_file.h"
#include "original_tables.h"
#include "runtime_heap.h"

/* Variables. */
/* Windows replacements for FUN_8008B5E4 and FUN_8008B65C.
 *
 * On PSX these functions load MIPS machine code into fixed addresses:
 *   FUN_8008B5E4: file -> 0x800FADD0
 *   FUN_8008B65C: COMMON0/OVERBINS.BIN -> 0x80010000
 *
 * HQ.BIN and similar files loaded by FUN_8008B5E4 are called through entry
 * points in the overwritten 0x800FADD0 range, so those modules must become
 * ordinary native code. OVERBINS.BIN is different: it is an image/resource
 * container, and 0x80010000 is merely a reusable PSX work buffer.
 * The selected name is retained so later dispatch wrappers can choose the
 * correct native implementation when several modules used the same PSX range.
 */

static char g_code_module[260];

static void *g_code_module_data;

static sint32 g_code_module_size;

static void *g_overbins_data;

static sint32 g_overbins_size;

/* Functions. */
__declspec(dllexport) void *g_psx_language_workspace_host;

__declspec(dllexport) volatile uint32 g_psx_fixed_copy_calls;

__declspec(dllexport) uint32 g_psx_fixed_copy_destination;

__declspec(dllexport) const void *g_psx_fixed_copy_source;

__declspec(dllexport) sint32 g_psx_fixed_copy_size;

/* Native equivalents of the two fixed PSX copy destinations.  The wrapper is
 * the platform implementation of PsyQ memcpy at 0x800B0628; its arguments
 * remain the literal MIPS destination, source and byte count. */
/* Original: FUN_800B0628. */
GDB_CALL void *fixed_address_copy(void *destination, const void *source, sint32 size)
{
    void **mapped = destination == (void *)0x800fadd0u ? &g_code_module_data : &g_overbins_data;
    sint32 *mapped_size = destination == (void *)0x800fadd0u ? &g_code_module_size : &g_overbins_size;
    void *copy;
    ++g_psx_fixed_copy_calls;
    g_psx_fixed_copy_destination = (uint32)destination;
    g_psx_fixed_copy_source = source;
    g_psx_fixed_copy_size = size;
    if (destination == (void *)0x80082640u)
    {
        g_psx_language_workspace_host = original_tables_address(0x80082640u);
        return memcpy(g_psx_language_workspace_host, source, (uint32)size);
    }
    if (destination != (void *)0x800fadd0u && destination != (void *)0x80010000u)
        return memcpy(destination, source, (uint32)size);
    /* The original destination is fixed. Keep existing resource pointers valid
     * when reloading the same-sized resident image after BIGDIVER overwrote it. */
    copy = *mapped;
    if (destination != (void *)0x80010000u || copy == 0 || size > *mapped_size)
        copy = realloc(copy, (uint32)size);
    if (copy == 0)
        abort();
    *mapped = copy;
    *mapped_size = size;
    return memcpy(copy, source, (uint32)size);
}

static void code_cache_flush(void)
{
    /* Host code is not executed from the copied MIPS overlay. */
    ++g_psx_flush_cache_boundary_calls;
}

/* Original: FUN_8008B5E4. */
GDB_CALL void overlay_module_load(char *filename)
{
    sint32 size;
    void *work;
    size_t name_length = strlen(filename);
    if (name_length >= sizeof(g_code_module))
        name_length = sizeof(g_code_module) - 1;
    memcpy(g_code_module, filename, name_length);
    g_code_module[name_length] = 0;
    code_cache_flush();
    size = game_file_size(filename);
    work = runtime_heap_allocate_sector_aligned(size);
    game_file_read(filename, work);
    fixed_address_copy((void *)0x800fadd0u, work, size);
    runtime_heap_defer_free(work);
    runtime_heap_sweep();
}

/* Original: FUN_8008B65C. */
GDB_CALL void overbins_load(void)
{
    const char *path = "COMMON0\\OVERBINS.BIN";
    sint32 size = game_file_size(path);
    g_shared_result_pointer = runtime_heap_allocate_sector_aligned(size);
    game_file_read(path, g_shared_result_pointer);
    fixed_address_copy((void *)0x80010000u, g_shared_result_pointer, size);
    runtime_heap_defer_free(g_shared_result_pointer);
    runtime_heap_sweep();
}

const char *win_get_selected_code_module(void)
{
    return g_code_module;
}

void *overbins_data(sint32 *size_out)
{
    if (size_out != NULL)
        *size_out = g_overbins_size;
    return g_overbins_data;
}

const void *win_code_module_address(uint32 psx_address)
{
    sint32 offset;
    if (psx_address < 0x800fadd0u || g_code_module_data == NULL)
        return NULL;
    offset = (sint32)(psx_address - 0x800fadd0u);
    if (offset >= g_code_module_size)
        return NULL;
    return (const uint8 *)g_code_module_data + offset;
}

sint32 win_code_module_has_signature(uint32 psx_address, const uint8 *bytes, sint32 length)
{
    const uint8 *data = (const uint8 *)win_code_module_address(psx_address);
    sint32 offset;
    if (data == NULL || bytes == NULL)
        return 0;
    offset = (sint32)(psx_address - 0x800fadd0u);
    if (length > g_code_module_size - offset)
        return 0;
    return memcmp(data, bytes, length) == 0;
}
