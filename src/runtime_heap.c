#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "runtime_heap.h"
#include "collision.h"
#include "input_record.h"
#include "hq.h"
#include "mission.h"
#include "effect_update.h"
#include "game_runtime.h"
#include "level_data.h"
#include "map.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "render.h"
#include "spatial.h"
#include "sprite.h"

typedef enum
{
    HEAP_BLOCK_FREE,
    HEAP_BLOCK_ACTIVE,
    HEAP_BLOCK_PENDING
} HEAP_BLOCK_STATE;

typedef union
{
    long double alignment;
    void *pointer_alignment;

    struct
    {
        sint32 size;
        HEAP_BLOCK_STATE state;
    } info;
} HEAP_BLOCK;

enum { HEAP_BLOCK_BYTES = sizeof(HEAP_BLOCK) };

typedef struct
{
    HEAP_BLOCK *begin;
    sint32 capacity;
    uint8 dirty;
} RUNTIME_HEAP;

/* Original payload budget after the loaded overlay reservation */
enum
{
    RUNTIME_HEAP_CAPACITY = 0x100788
};

static RUNTIME_HEAP heap;

/* Native lifecycle only; never dereference objects whose arena is ending */
static void heap_invalidate_references(void)
{
    linked_list_initialize(&g_object_list);
    linked_list_initialize(&g_auxiliary_object_list);
    linked_list_initialize(&g_ledge_trigger_list);
    player_heap_reset();
    hq_heap_reset();
    input_rec_heap_reset();
    effects_heap_reset();
    model_heap_reset();
    col_heap_reset();
    map_heap_reset();
    level_data_heap_reset();
    game_runtime_heap_reset();
    g_spatial_cell_buckets = 0;
    g_spatial_cell_count = 0;
    spatial_buckets_reset();
    spr_vram_reset_deferred();
    g_next_frame_object = g_frame_objects;
    g_next_transient_prim = g_transient_prims;
    g_mission.timer = 0;
    g_model_draw.target_frame = 0;
}

static void heap_reset_state(void)
{
    heap_invalidate_references();
    if (heap.begin == 0)
    {
        heap.capacity = RUNTIME_HEAP_CAPACITY / sizeof(HEAP_BLOCK) * sizeof(HEAP_BLOCK);
        heap.begin = malloc(heap.capacity);
        XASSERT(!(heap.begin == 0));
    }
    memset(heap.begin, 0, sizeof(*heap.begin));
    heap.begin->info.size = heap.capacity;
    heap.begin->info.state = HEAP_BLOCK_FREE;
    heap.dirty = 0;
}

static void heap_ensure(void)
{
    if (heap.begin == 0)
        heap_reset_state();
}

/* Original: FUN_8008C3C0. */
GDB_CALL void *runtime_heap_initialize(void)
{
    heap_reset_state();
    return heap.begin;
}

void runtime_heap_release(void)
{
    heap_invalidate_references();
    free(heap.begin);
    memset(&heap, 0, sizeof(heap));
}

/* Original: FUN_8008C414. */
GDB_CALL void *runtime_heap_allocate(sint32 size)
{
    HEAP_BLOCK *block;
    uint8 *end;
    sint32 requested, remainder;
    HEAP_BLOCK *allocated;
    heap_ensure();
    if (size < 0 || size > heap.capacity - HEAP_BLOCK_BYTES)
    {
        fatal_error_with_val("HEAP SIZE", size);
        abort();
    }
    requested = size + sizeof(HEAP_BLOCK);
    requested += (sizeof(HEAP_BLOCK) - requested % sizeof(HEAP_BLOCK)) % sizeof(HEAP_BLOCK);
    block = heap.begin;
    end = (uint8 *)heap.begin + heap.capacity;
    while ((uint8 *)block < end && (block->info.state != HEAP_BLOCK_FREE || block->info.size < requested))
        block = (HEAP_BLOCK *)((uint8 *)block + block->info.size);
    if ((uint8 *)block >= end)
    {
        fatal_error_with_val("OUT OF MEMORY", requested);
        abort();
    }
    remainder = block->info.size - requested;
    allocated = (HEAP_BLOCK *)((uint8 *)block + remainder);
    block->info.size = remainder;
    memset(allocated, 0, sizeof(*allocated));
    allocated->info.size = requested;
    allocated->info.state = HEAP_BLOCK_ACTIVE;
    memset(allocated + 1, 0, requested - sizeof(*allocated));
    return allocated + 1;
}

/* Original: FUN_8008C4E8. */
void *runtime_heap_allocate_best_fit(sint32 size)
{
    HEAP_BLOCK *block, *best = 0, *allocated;
    uint8 *end;
    sint32 requested, best_size = 0x7fffffff, remainder;
    heap_ensure();
    if (size < 0 || size > heap.capacity - HEAP_BLOCK_BYTES)
    {
        fatal_error_with_val("HEAP SIZE", size);
        abort();
    }
    requested = size + sizeof(HEAP_BLOCK);
    requested += (sizeof(HEAP_BLOCK) - requested % sizeof(HEAP_BLOCK)) % sizeof(HEAP_BLOCK);
    block = heap.begin;
    end = (uint8 *)heap.begin + heap.capacity;
    while ((uint8 *)block < end)
    {
        sint32 size = block->info.size;
        if (block->info.state == HEAP_BLOCK_FREE && requested < size && size < best_size)
        {
            best = block;
            best_size = size;
        }
        block = (HEAP_BLOCK *)((uint8 *)block + size);
    }
    if (best == 0)
    {
        fatal_error_with_val("OUT OF MEMORY", requested);
        abort();
    }
    remainder = best->info.size - requested;
    allocated = (HEAP_BLOCK *)((uint8 *)best + remainder);
    best->info.size = remainder;
    memset(allocated, 0, sizeof(*allocated));
    allocated->info.size = requested;
    allocated->info.state = HEAP_BLOCK_ACTIVE;
    memset(allocated + 1, 0, requested - sizeof(*allocated));
    return allocated + 1;
}

/* Original: FUN_8008C5E4. */
void *runtime_heap_shrink(void *pointer, sint32 size)
{
    HEAP_BLOCK *old_block = (HEAP_BLOCK *)pointer - 1;
    sint32 requested, remainder;
    HEAP_BLOCK *new_block;
    if (size < 0 || size > old_block->info.size - HEAP_BLOCK_BYTES)
    {
        fatal_error_with_val("HEAP SHRINK SIZE", size);
        abort();
    }
    requested = size + sizeof(HEAP_BLOCK);
    requested += (sizeof(HEAP_BLOCK) - requested % sizeof(HEAP_BLOCK)) % sizeof(HEAP_BLOCK);
    remainder = old_block->info.size - requested;
    if (remainder < 2 * sizeof(HEAP_BLOCK))
        return pointer;
    new_block = (HEAP_BLOCK *)((uint8 *)old_block + remainder);
    memmove(new_block + 1, old_block + 1, requested - sizeof(HEAP_BLOCK));
    memset(new_block, 0, sizeof(*new_block));
    new_block->info.size = requested;
    new_block->info.state = HEAP_BLOCK_ACTIVE;
    old_block->info.size = remainder;
    old_block->info.state = HEAP_BLOCK_FREE;
    return new_block + 1;
}

/* Original: FUN_8008C680. */
GDB_CALL void *runtime_heap_allocate_sector_aligned(sint32 size)
{
    if (size < 0 || size > 0x7ffff800)
    {
        fatal_error_with_val("SECTOR BUFFER SIZE", size);
        abort();
    }
    return runtime_heap_allocate_best_fit((size + 0x7ff) & ~0x7ff);
}

/* Original: FUN_8008C700. */
GDB_CALL void runtime_heap_defer_free(void *pointer)
{
    HEAP_BLOCK *block;
    if (pointer == 0)
        return;
    block = (HEAP_BLOCK *)pointer - 1;
    block->info.state = HEAP_BLOCK_PENDING;
    heap.dirty = 1;
}

/* Original: FUN_8008C76C. */
GDB_CALL void runtime_heap_sweep(void)
{
    HEAP_BLOCK *cur, *prev;
    uint8 *end;
    if (!heap.dirty)
        return;
    heap.dirty = 0;
    cur = heap.begin;
    end = (uint8 *)heap.begin + heap.capacity;
    while ((uint8 *)cur < end)
    {
        prev = cur;
        if (cur->info.state == HEAP_BLOCK_ACTIVE)
        {
            cur = (HEAP_BLOCK *)((uint8 *)cur + cur->info.size);
            continue;
        }
        cur->info.state = HEAP_BLOCK_FREE;
        for (;;)
        {
            cur = (HEAP_BLOCK *)((uint8 *)cur + cur->info.size);
            if ((uint8 *)cur >= end)
                return;
            if (cur->info.state == HEAP_BLOCK_ACTIVE)
                break;
            prev->info.size += cur->info.size;
            cur = prev;
        }
    }
}
