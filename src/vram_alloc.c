#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "object.h"
#include "game_runtime.h"
#include "sprite.h"
#include "stubs.h"
#include "vram_alloc.h"

/* Types. */
/* Exact host translation of FUN_8008C8B8/FUN_8008CA48/FUN_8008CC88.
 * A source record is {page_x,page_y,width,height,pixel_mode}. */

typedef struct VramLink
{
    VRAM_SPRITE *descriptor;
    VRAM_SPRITE *next;
    sint32 region;
} VramLink;

/* Variables. */
static VramRegion regions[32];

static VramLink links[256];

static sint32 region_count;

/* Functions. */

static VramLink *find_link(VRAM_SPRITE *d)
{
    VramLink *l;
    for (l = links; l < links + 256; l++)
        if (l->descriptor == d)
            return l;
    return 0;
}

static VramLink *new_link(VRAM_SPRITE *d, sint32 r)
{
    VramLink *l;
    for (l = links; l < links + 256; l++)
        if (!l->descriptor)
        {
            l->descriptor = d;
            l->region = r;
            return l;
        }
    return 0;
}

sint32 vram_alloc_is_anchor(void *anchor)
{
    uintptr_t value = (uintptr_t)anchor;
    return value >= (uintptr_t)regions && value < (uintptr_t)(regions + 32) && (value - (uintptr_t)regions) % sizeof(VramRegion) == 0;
}

void vram_alloc_init_anchor(void *anchor)
{
    VramRegion *region = (VramRegion *)anchor;
    region->head = 0;
    region->tail = 0;
}

void vram_alloc_link_descriptor(void *anchor, void *descriptor)
{
    VramRegion *region = (VramRegion *)anchor;
    VRAM_SPRITE *typed = (VRAM_SPRITE *)descriptor;
    sint32 ri = (sint32)(region - regions);
    VramLink *link = new_link(typed, ri);
    if (link == 0)
        abort();
    if (region->tail)
    {
        VramLink *tail = find_link(region->tail);
        if (tail == 0)
            abort();
        tail->next = typed;
    }
    else
        region->head = typed;
    region->tail = typed;
}

void vram_alloc_unlink_descriptor(void *anchor, void *d)
{
    VRAM_SPRITE *typed = (VRAM_SPRITE *)d;
    VramLink *l = find_link(typed), *p = 0, *q;
    sint32 ri;
    if (!l)
        return;
    ri = l->region;
    if ((sint32)(intptr)anchor != ri + 1)
        return;
    if (regions[ri].head == typed)
        regions[ri].head = l->next;
    else
    {
        for (q = links; q < links + 256; q++)
            if (q->descriptor && q->next == typed)
            {
                p = q;
                break;
            }
        if (p)
            p->next = l->next;
    }
    if (regions[ri].tail == typed)
        regions[ri].tail = p ? p->descriptor : 0;
    memset(l, 0, sizeof(*l));
}

void vram_alloc_reset_storage(void)
{
    memset(regions, 0, sizeof(regions));
    memset(links, 0, sizeof(links));
}

VramRegion *vram_alloc_region(sint32 index)
{
    return regions + index;
}

sint32 vram_alloc_region_count(void)
{
    return region_count;
}

void vram_alloc_set_region_count(sint32 count)
{
    region_count = count;
}

VRAM_SPRITE *vram_alloc_next_descriptor(VRAM_SPRITE *descriptor)
{
    VramLink *link = find_link(descriptor);
    return link ? link->next : 0;
}
