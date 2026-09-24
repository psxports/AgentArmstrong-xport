#ifndef MODULE_API_VRAM_ALLOC_H
#define MODULE_API_VRAM_ALLOC_H

#include "xport.h"
#include "psx.h"
#include "sprite.h"

typedef struct VramRegion
{
    void *head;
    void *tail;
    uint16 x, y, width, height, mode, tpage;
} VramRegion;

/* Host storage access; placement policy belongs to sprite.c. */
void vram_alloc_reset_storage(void);
VramRegion *vram_alloc_region(sint32 index);
sint32 vram_alloc_region_count(void);
void vram_alloc_set_region_count(sint32 count);
VRAM_SPRITE *vram_alloc_next_descriptor(VRAM_SPRITE *descriptor);

/* BEGIN GENERATED MODULE API */
sint32 vram_alloc_is_anchor(void *anchor);
void vram_alloc_init_anchor(void *anchor);
void vram_alloc_link_descriptor(void *anchor, void *descriptor);
void vram_alloc_unlink_descriptor(void *anchor, void *d);
/* END GENERATED MODULE API */

#endif
