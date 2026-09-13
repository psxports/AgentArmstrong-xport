#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "object.h"
#include "sprite.h"
#include "stubs.h"

/* Types. */
typedef union RuntimeArena
{
    uint64 alignment;
    /* PAL InitHeap reports 0x104C4C bytes at 0x800FADD0.  FUN_8008C3C0
     * reserves 0x44C0 for the loaded overlay and aligns the remainder. */
    uint8 bytes[0x100788];
} RuntimeArena;

/* Variables. */
/* Contiguous host arena implementing the PAL allocator at
 * 0x8008C3C0..0x8008C80C. */

static sint32 heap_dirty;

static RuntimeArena runtime_arena;

static sint32 heap_ready;

/* Functions. */
/* PsyQ libc entry used by the original allocator. */
/* Original: FUN_800B0F04. */
static void memory_move(void *destination, const void *source, sint32 size)
{
    memmove(destination, source, (uint32)size);
}

static uint8 *heap_begin(void)
{
    return runtime_arena.bytes;
}

static uint8 *heap_end(void)
{
    return runtime_arena.bytes + sizeof(runtime_arena.bytes);
}

static sint32 block_size(const uint8 *block)
{
    return *(const sint32 *)block;
}

static uint8 block_state(const uint8 *block)
{
    return block[4];
}

static void block_set_state(uint8 *block, uint8 state)
{
    block[4] = state;
}

static void heap_reset_state(void)
{
    uint8 *block = heap_begin();
    memset(block, 0, 8);
    *(sint32 *)block = (sint32)sizeof(runtime_arena.bytes) & ~7;
    heap_ready = 1;
    heap_dirty = 0;
    sprite_vram_reset_deferred();
}

static void heap_ensure(void)
{
    if (!heap_ready)
        heap_reset_state();
}

/* Original: FUN_8008C3C0. */
GDB_CALL void *runtime_heap_initialize(void)
{
    heap_reset_state();
    return heap_begin();
}

/* Original: FUN_8008C414. */
GDB_CALL void *runtime_heap_allocate(sint32 size)
{
    sint32 requested = (size + 15) & ~7;
    uint8 *block, *end;
    heap_ensure();
    block = heap_begin();
    end = heap_end();
    while (block < end && (block_state(block) != 0 || block_size(block) < requested))
        block += block_size(block);
    if (block >= end)
    {
        fatal_error_with_value("OUT OF MEMORY", requested);
        abort();
    }
    {
        sint32 remainder = block_size(block) - requested;
        uint8 *allocated = block + remainder;
        *(sint32 *)block = remainder;
        *(sint32 *)allocated = requested;
        allocated[4] = 1;
        allocated[5] = allocated[6] = allocated[7] = 0;
        memset(allocated + 8, 0, (uint32)(requested - 8));
        return allocated + 8;
    }
}

/* Original: FUN_8008C4E8. */
void *runtime_heap_allocate_best_fit(sint32 size)
{
    sint32 requested = (size + 15) & ~7;
    sint32 best_size = 0x7fffffff;
    uint8 *block, *best = 0, *end;
    heap_ensure();
    block = heap_begin();
    end = heap_end();
    while (block < end)
    {
        sint32 current_size = block_size(block);
        if (block_state(block) == 0 && requested < current_size && current_size < best_size)
        {
            best = block;
            best_size = current_size;
        }
        block += current_size;
    }
    if (best == 0)
    {
        fatal_error_with_value("OUT OF MEMORY", requested);
        abort();
    }
    {
        sint32 remainder = block_size(best) - requested;
        uint8 *allocated = best + remainder;
        *(sint32 *)best = remainder;
        *(sint32 *)allocated = requested;
        allocated[4] = 1;
        allocated[5] = allocated[6] = allocated[7] = 0;
        memset(allocated + 8, 0, (uint32)(requested - 8));
        return allocated + 8;
    }
}

/* Direct control flow of FUN_8008C5E4.  The PSX arena keeps the shrunken
 * allocation at the high end of the old block. */
void *runtime_heap_shrink(void *pointer, sint32 size)
{
    sint32 aligned = (size + 15) & ~7;
    uint8 *old_block = (uint8 *)pointer - 8;
    sint32 old_size = *(sint32 *)old_block;
    sint32 remainder = old_size - aligned;
    uint8 *new_block;

    if ((uint32)remainder < 0x10u)
        return pointer;
    new_block = old_block + remainder;
    memory_move(new_block, old_block, aligned);
    *(sint32 *)new_block = aligned;
    new_block[4] = 1;
    new_block[5] = new_block[6] = new_block[7] = 0;
    *(sint32 *)old_block = remainder;
    old_block[4] = 0;
    return new_block + 8;
}

/* Direct translation of FUN_8008C680. */
GDB_CALL void *runtime_heap_allocate_sector_aligned(sint32 size)
{
    return runtime_heap_allocate_best_fit((size + 0x7ff) & ~0x7ff);
}

/* Direct translation of FUN_8008C700. */
GDB_CALL void runtime_heap_defer_free(void *pointer)
{
    if (pointer == 0)
        return;
    *((uint8 *)pointer - 4) = 2;
    heap_dirty = 1;
}

/* Direct translation of FUN_8008C76C. */
GDB_CALL void runtime_heap_sweep(void)
{
    uint8 *current, *previous, *end;
    if (!heap_dirty)
        return;
    heap_dirty = 0;
    current = heap_begin();
    end = heap_end();
    while (current < end)
    {
        previous = current;
        if (block_state(current) == 1)
        {
            current += block_size(current);
            continue;
        }
        block_set_state(current, 0);
        for (;;)
        {
            current += block_size(current);
            if (current >= end)
                return;
            if (block_state(current) == 1)
                break;
            *(sint32 *)previous += block_size(current);
            current = previous;
        }
    }
}
