#ifndef MODULE_API_RUNTIME_HEAP_H
#define MODULE_API_RUNTIME_HEAP_H

#include "app.h"
#include "sprite.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL void *runtime_heap_allocate(sint32 size);
GDB_CALL void *runtime_heap_allocate_sector_aligned(sint32 size);
GDB_CALL void *runtime_heap_initialize(void);
GDB_CALL void runtime_heap_defer_free(void *pointer);
GDB_CALL void runtime_heap_sweep(void);
void *runtime_heap_allocate_best_fit(sint32 size);
void *runtime_heap_shrink(void *pointer, sint32 size);
/* END GENERATED MODULE API */

#endif
