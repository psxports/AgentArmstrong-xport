#ifndef MODULE_API_HQ_H
#define MODULE_API_HQ_H

#include <stddef.h>

#include "animation.h"
#include "app.h"
#include "collision.h"
#include "effect_update.h"

/* Types. */
typedef struct
{
    COLLISION collision; /* +0x00 */
    uint32 field_78;
    sint16 age; /* +0x7C */
    uint16 field_7e;
    EFFECT *marker;         /* +0x80 */
    sint32 render_resource; /* +0x84 */
} HQ_OBJECT;

#if defined(AP_32BIT)
typedef char HqAmbientObject_size_88[sizeof(HQ_OBJECT) == 0x88 ? 1 : -1];
typedef char HqAmbientObject_marker_at_80[offsetof(HQ_OBJECT, marker) == 0x80 ? 1 : -1];
#endif

/* Types. */
typedef struct HQ_MAP_NODE HQ_MAP_NODE;

typedef struct
{
    COLLISION collision;      /* +0x00 */
    uint8 field_78[8];        /* +0x78 */
    uint32 sampled_buttons;   /* +0x80 */
    ANIM animation;           /* +0x84 */
    sint16 briefing_active;   /* +0xA0 */
    sint16 briefing_scroll_y; /* +0xA2 */
    sint16 briefing_extent;   /* +0xA4 */
    sint16 initialized;       /* +0xA6 */
} HQ_WORLD_MAP_SELECTOR;

#if defined(AP_32BIT)
typedef char HqWorldMapSelector_size_a8[sizeof(HQ_WORLD_MAP_SELECTOR) == 0xA8 ? 1 : -1];
typedef char HqWorldMapSelector_buttons_at_80[offsetof(HQ_WORLD_MAP_SELECTOR, sampled_buttons) == 0x80 ? 1 : -1];
typedef char HqWorldMapSelector_animation_at_84[offsetof(HQ_WORLD_MAP_SELECTOR, animation) == 0x84 ? 1 : -1];
typedef char HqWorldMapSelector_briefing_at_a0[offsetof(HQ_WORLD_MAP_SELECTOR, briefing_active) == 0xA0 ? 1 : -1];
typedef char HqWorldMapSelector_initialized_at_a6[offsetof(HQ_WORLD_MAP_SELECTOR, initialized) == 0xA6 ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
GDB_CALL HQ_MAP_NODE *hq_world_map_node_find_by_id(sint16 id);
GDB_CALL sint16 hq_scrolling_text_render(const char *text, sint16 x, sint16 extent, const char *caption, sint32 automatic);
GDB_CALL sint16 text_line_break_count(const char *text);
GDB_CALL void hq_loading_screen_run(void);
GDB_CALL void hq_portrait_trigger_update(EFFECT *effect);
GDB_CALL void hq_world_map_relocate_nodes(void);
sint32 fixed_dda_initialize(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, sint32 *line);
void fixed_dda_advance(sint32 *line);
void hq_level_update(void);
/* END GENERATED MODULE API */

#endif
