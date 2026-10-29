#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_math.h"
#include "native_config.h"
#include "level_data.h"
#include "map.h"
#include "cc_archive.h"
#include "render.h"
#include "spatial.h"
#include "game.h"
#include "map_render.h"
#include "camera.h"
#include "input_record.h"
#include "object.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "resource_table.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "text_renderer.h"

static uint16 spr_clut_entry_count(uint32 resource_id);

sint32 g_vram_error_reporting_enabled;

/* Original: DAT_800D3CC0, immutable default used by sprite consumers */
const sint32 g_default_spr_scale = 0x10fa;

/* Originals: DAT_800D75D8, DAT_800D3E74, DAT_800D3E80, DAT_800D41D4 */
SPRITE_PALETTE_STATE g_spr_pal;
/* Original: DAT_800D41DC, retained only by upload diagnostics */

/* Original data: SLES_004.74:0x800C8BC8..0x800C8C33 */
const ANIM_COMMAND g_particle_animation[14] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {3, 17427, 0}, 0}, {ANIM_STEP_FRAME, {3, 17428, 0}, 0}, {ANIM_STEP_FRAME, {3, 17429, 0}, 0}, {ANIM_STEP_FRAME, {3, 17430, 0}, 0}, {ANIM_STEP_FRAME, {3, 17431, 0}, 0}, {ANIM_STEP_FRAME, {3, 17432, 0}, 0}, {ANIM_STEP_FRAME, {3, 17433, 0}, 0}, {ANIM_STEP_FRAME, {3, 17434, 0}, 0}, {ANIM_STEP_FRAME, {3, 17435, 0}, 0}, {ANIM_STEP_FRAME, {3, 17436, 0}, 0}, {ANIM_STEP_FRAME, {3, 17437, 0}, 0}, {ANIM_STEP_FRAME, {3, 17438, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Variables. */
/* Semantic native-port module; original address comments are preserved. */
/* Native descriptors retain the original device coordinates and resource IDs */

static VRAM_SPRITE desc_pool[190];

static VRAM_SPRITE **deferred_resources;
static uint32 deferred_resource_count;
static uint32 deferred_resource_capacity;

static VRAM_SPRITE *desc_next = desc_pool;

typedef struct
{
    VRAM_SPRITE *data;
    uint32 count;
} SPRITE_DESCRIPTOR_BANK;

struct VRAM_REGION
{
    VRAM_SPRITE *head, *tail;
    uint16 x, y, width, height, mode, tpage;
};

static SPRITE_DESCRIPTOR_BANK desc_banks[RESOURCE_SLOT_COUNT];
static VRAM_REGION vram_regions[32];
static sint32 vram_region_count;

/* Original: DAT_800C8260. */
static const VRAM_REGION_CFG game_vram_regions[] = {
    {0x140, 0x0, 0x100, 0xc0, 0x0}, {0x180, 0x0, 0x100, 0xc0, 0x0}, {0x1c0, 0x0, 0x100, 0xc0, 0x0}, {0x200, 0x0, 0x100, 0xc0, 0x0}, {0x240, 0x0, 0x100, 0x100, 0x0}, {0x280, 0x0, 0x100, 0x100, 0x0}, {0x2c0, 0x0, 0x100, 0x100, 0x0}, {0x300, 0x0, 0x100, 0x100, 0x0}, {0x340, 0x0, 0x100, 0x100, 0x0}, {0x380, 0x0, 0x100, 0x100, 0x0}, {0x3c0, 0x0, 0x100, 0x100, 0x0}, {0x140, 0x100, 0x100, 0x100, 0x0}, {0x180, 0x100, 0x100, 0x100, 0x0}, {0x1c0, 0x100, 0x100, 0x100, 0x0}, {0x200, 0x100, 0x100, 0x100, 0x0}, {0x240, 0x100, 0x100, 0x100, 0x1}, {0x2c0, 0x100, 0x100, 0x100, 0x1}, {0x340, 0x100, 0x100, 0x100, 0x0}, {0x380, 0x100, 0x100, 0x100, 0x0}, {0x3c0, 0x100, 0x100, 0x100, 0x0},
};
const VRAM_LAYOUT g_game_vram_layout = {game_vram_regions, sizeof(game_vram_regions) / sizeof(game_vram_regions[0])};

/* Original: DAT_800C8334. */
static const VRAM_REGION_CFG hq_vram_regions[] = {
    {0x140, 0x0, 0x100, 0xc0, 0x0}, {0x180, 0x0, 0x100, 0xc0, 0x0}, {0x1c0, 0x0, 0x100, 0xc0, 0x0}, {0x200, 0x0, 0x100, 0xc0, 0x0}, {0x240, 0x0, 0x100, 0x100, 0x0}, {0x280, 0x0, 0x100, 0x100, 0x0}, {0x2c0, 0x0, 0x100, 0x100, 0x0}, {0x300, 0x0, 0x100, 0x100, 0x0}, {0x340, 0x0, 0x100, 0x100, 0x0}, {0x380, 0x0, 0x100, 0x100, 0x0}, {0x3c0, 0x0, 0x100, 0x100, 0x0}, {0x140, 0x100, 0x100, 0x100, 0x1}, {0x1c0, 0x100, 0x100, 0x100, 0x0}, {0x200, 0x100, 0x100, 0x100, 0x0}, {0x240, 0x100, 0x100, 0x100, 0x1}, {0x2c0, 0x100, 0x100, 0x100, 0x1}, {0x340, 0x100, 0x100, 0x100, 0x0}, {0x380, 0x100, 0x100, 0x100, 0x0}, {0x3c0, 0x100, 0x100, 0x100, 0x0},
};
const VRAM_LAYOUT g_hq_vram_layout = {hq_vram_regions, sizeof(hq_vram_regions) / sizeof(hq_vram_regions[0])};

/* Original data: SLES_004.74:0x800C2E70..0x800C3E6F, 10-bit angle and signed 16.16 samples */
static const sint32 spr_s_samples[1024] = {
    0,      402,    804,    1206,   1608,   2010,   2412,   2814,   3215,   3617,   4018,   4420,   4821,   5222,   5622,   6023,   6423,   6823,   7223,   7623,   8022,   8421,   8819,   9218,   9616,   10013,  10410,  10807,  11204,  11600,  11995,  12390,  12785,  13179,  13573,  13966,  14359,  14751,  15142,  15533,  15923,  16313,  16702,  17091,  17479,  17866,  18253,  18638,  19024,  19408,  19792,  20175,  20557,  20938,  21319,  21699,  22078,  22456,  22833,  23210,  23586,  23960,  24334,  24707,  25079,  25450,  25820,  26189,  26557,  26925,  27291,  27656,  28020,  28383,  28745,  29105,  29465,  29824,  30181,  30538,  30893,  31247,  31600,  31952,  32302,  32651,  32999,  33346,  33692,  34036,  34379,  34721,  35061,  35400,  35738,  36074,  36409,  36743,  37075,  37406,  37736,  38064,  38390,  38716,  39039,  39362,  39682,  40002,  40319,  40636,  40950,  41263,  41575,  41885,
    42194,  42501,  42806,  43110,  43412,  43712,  44011,  44308,  44603,  44897,  45189,  45480,  45768,  46055,  46340,  46624,  46906,  47186,  47464,  47740,  48015,  48288,  48558,  48828,  49095,  49360,  49624,  49886,  50146,  50403,  50660,  50914,  51166,  51416,  51665,  51911,  52155,  52398,  52639,  52877,  53114,  53348,  53581,  53811,  54040,  54266,  54491,  54713,  54933,  55152,  55368,  55582,  55794,  56004,  56212,  56417,  56621,  56822,  57022,  57219,  57414,  57606,  57797,  57986,  58172,  58356,  58538,  58718,  58895,  59070,  59243,  59414,  59583,  59749,  59913,  60075,  60235,  60392,  60547,  60700,  60850,  60998,  61144,  61288,  61429,  61568,  61705,  61839,  61971,  62100,  62228,  62353,  62475,  62596,  62714,  62829,  62942,  63053,  63162,  63268,  63371,  63473,  63571,  63668,  63762,  63854,  63943,  64030,  64115,  64197,  64276,  64353,  64428,  64501,
    64571,  64638,  64703,  64766,  64826,  64884,  64939,  64992,  65043,  65091,  65136,  65179,  65220,  65258,  65294,  65327,  65358,  65386,  65412,  65436,  65457,  65475,  65491,  65505,  65516,  65524,  65531,  65534,  65536,  65534,  65531,  65524,  65516,  65505,  65491,  65475,  65457,  65436,  65412,  65386,  65358,  65327,  65294,  65258,  65220,  65179,  65136,  65091,  65043,  64992,  64939,  64884,  64826,  64766,  64703,  64638,  64571,  64501,  64428,  64353,  64276,  64197,  64115,  64030,  63943,  63854,  63762,  63668,  63571,  63473,  63371,  63268,  63162,  63053,  62942,  62829,  62714,  62596,  62475,  62353,  62228,  62100,  61971,  61839,  61705,  61568,  61429,  61288,  61144,  60998,  60850,  60700,  60547,  60392,  60235,  60075,  59913,  59749,  59583,  59414,  59243,  59070,  58895,  58718,  58538,  58356,  58172,  57986,  57797,  57606,  57414,  57219,  57022,  56822,
    56621,  56417,  56212,  56004,  55794,  55582,  55368,  55152,  54933,  54713,  54491,  54266,  54040,  53811,  53581,  53348,  53114,  52877,  52639,  52398,  52155,  51911,  51665,  51416,  51166,  50914,  50660,  50403,  50146,  49886,  49624,  49360,  49095,  48828,  48558,  48288,  48015,  47740,  47464,  47186,  46906,  46624,  46340,  46055,  45768,  45480,  45189,  44897,  44603,  44308,  44011,  43712,  43412,  43110,  42806,  42501,  42194,  41885,  41575,  41263,  40950,  40636,  40319,  40002,  39682,  39362,  39039,  38716,  38390,  38064,  37736,  37406,  37075,  36743,  36409,  36074,  35738,  35400,  35061,  34721,  34379,  34036,  33692,  33346,  32999,  32651,  32302,  31952,  31600,  31247,  30893,  30538,  30181,  29824,  29465,  29105,  28745,  28383,  28020,  27656,  27291,  26925,  26557,  26189,  25820,  25450,  25079,  24707,  24334,  23960,  23586,  23210,  22833,  22456,
    22078,  21699,  21319,  20938,  20557,  20175,  19792,  19408,  19024,  18638,  18253,  17866,  17479,  17091,  16702,  16313,  15923,  15533,  15142,  14751,  14359,  13966,  13573,  13179,  12785,  12390,  11995,  11600,  11204,  10807,  10410,  10013,  9616,   9218,   8819,   8421,   8022,   7623,   7223,   6823,   6423,   6023,   5622,   5222,   4821,   4420,   4018,   3617,   3215,   2814,   2412,   2010,   1608,   1206,   804,    402,    0,      -402,   -804,   -1206,  -1608,  -2010,  -2412,  -2814,  -3215,  -3617,  -4018,  -4420,  -4821,  -5222,  -5622,  -6023,  -6423,  -6823,  -7223,  -7623,  -8022,  -8421,  -8819,  -9218,  -9616,  -10013, -10410, -10807, -11204, -11600, -11995, -12390, -12785, -13179, -13573, -13966, -14359, -14751, -15142, -15533, -15923, -16313, -16702, -17091, -17479, -17866, -18253, -18638, -19024, -19408, -19792, -20175, -20557, -20938, -21319, -21699, -22078, -22456,
    -22833, -23210, -23586, -23960, -24334, -24707, -25079, -25450, -25820, -26189, -26557, -26925, -27291, -27656, -28020, -28383, -28745, -29105, -29465, -29824, -30181, -30538, -30893, -31247, -31600, -31952, -32302, -32651, -32999, -33346, -33692, -34036, -34379, -34721, -35061, -35400, -35738, -36074, -36409, -36743, -37075, -37406, -37736, -38064, -38390, -38716, -39039, -39362, -39682, -40002, -40319, -40636, -40950, -41263, -41575, -41885, -42194, -42501, -42806, -43110, -43412, -43712, -44011, -44308, -44603, -44897, -45189, -45480, -45768, -46055, -46340, -46624, -46906, -47186, -47464, -47740, -48015, -48288, -48558, -48828, -49095, -49360, -49624, -49886, -50146, -50403, -50660, -50914, -51166, -51416, -51665, -51911, -52155, -52398, -52639, -52877, -53114, -53348, -53581, -53811, -54040, -54266, -54491, -54713, -54933, -55152, -55368, -55582, -55794, -56004, -56212, -56417, -56621, -56822,
    -57022, -57219, -57414, -57606, -57797, -57986, -58172, -58356, -58538, -58718, -58895, -59070, -59243, -59414, -59583, -59749, -59913, -60075, -60235, -60392, -60547, -60700, -60850, -60998, -61144, -61288, -61429, -61568, -61705, -61839, -61971, -62100, -62228, -62353, -62475, -62596, -62714, -62829, -62942, -63053, -63162, -63268, -63371, -63473, -63571, -63668, -63762, -63854, -63943, -64030, -64115, -64197, -64276, -64353, -64428, -64501, -64571, -64638, -64703, -64766, -64826, -64884, -64939, -64992, -65043, -65091, -65136, -65179, -65220, -65258, -65294, -65327, -65358, -65386, -65412, -65436, -65457, -65475, -65491, -65505, -65516, -65524, -65531, -65534, -65536, -65534, -65531, -65524, -65516, -65505, -65491, -65475, -65457, -65436, -65412, -65386, -65358, -65327, -65294, -65258, -65220, -65179, -65136, -65091, -65043, -64992, -64939, -64884, -64826, -64766, -64703, -64638, -64571, -64501,
    -64428, -64353, -64276, -64197, -64115, -64030, -63943, -63854, -63762, -63668, -63571, -63473, -63371, -63268, -63162, -63053, -62942, -62829, -62714, -62596, -62475, -62353, -62228, -62100, -61971, -61839, -61705, -61568, -61429, -61288, -61144, -60998, -60850, -60700, -60547, -60392, -60235, -60075, -59913, -59749, -59583, -59414, -59243, -59070, -58895, -58718, -58538, -58356, -58172, -57986, -57797, -57606, -57414, -57219, -57022, -56822, -56621, -56417, -56212, -56004, -55794, -55582, -55368, -55152, -54933, -54713, -54491, -54266, -54040, -53811, -53581, -53348, -53114, -52877, -52639, -52398, -52155, -51911, -51665, -51416, -51166, -50914, -50660, -50403, -50146, -49886, -49624, -49360, -49095, -48828, -48558, -48288, -48015, -47740, -47464, -47186, -46906, -46624, -46340, -46055, -45768, -45480, -45189, -44897, -44603, -44308, -44011, -43712, -43412, -43110, -42806, -42501, -42194, -41885,
    -41575, -41263, -40950, -40636, -40319, -40002, -39682, -39362, -39039, -38716, -38390, -38064, -37736, -37406, -37075, -36743, -36409, -36074, -35738, -35400, -35061, -34721, -34379, -34036, -33692, -33346, -32999, -32651, -32302, -31952, -31600, -31247, -30893, -30538, -30181, -29824, -29465, -29105, -28745, -28383, -28020, -27656, -27291, -26925, -26557, -26189, -25820, -25450, -25079, -24707, -24334, -23960, -23586, -23210, -22833, -22456, -22078, -21699, -21319, -20938, -20557, -20175, -19792, -19408, -19024, -18638, -18253, -17866, -17479, -17091, -16702, -16313, -15923, -15533, -15142, -14751, -14359, -13966, -13573, -13179, -12785, -12390, -11995, -11600, -11204, -10807, -10410, -10013, -9616,  -9218,  -8819,  -8421,  -8022,  -7623,  -7223,  -6823,  -6423,  -6023,  -5622,  -5222,  -4821,  -4420,  -4018,  -3617,  -3215,  -2814,  -2412,  -2010,  -1608,  -1206,  -804,   -402,
};

/* Functions. */
/* FUN_8008D108/FUN_8008F54C build this upload rectangle on their stacks.
 * It describes the current packed frame, not the full descriptor allocation. */
static void upload_dynamic_frame(VRAM_SPRITE *d, uint32 resource_id, uint8 width_minus_one, sint32 height)
{
    PSX_RECT rect;
    sint32 rounded;
    uint8 *frame = animation_frame_resource(resource_id);
    const RESOURCE_DATA *resource = resource_table_data(resource_id);
    XASSERT(!(frame == 0 || resource == 0));
    if (d->pixel_mode == 0)
    {
        rounded = (width_minus_one + 8) & 0xf8;
        rect.w = (rounded / 4);
        rect.x = (d->page_x + d->u0 / 4);
    }
    else if (d->pixel_mode == 1)
    {
        rounded = (width_minus_one + 4) & 0xfc;
        rect.w = (rounded / 2);
        rect.x = (d->page_x + d->u0 / 2);
    }
    else
    {
        fatal_error("SPRITE PIXEL MODE");
        abort();
    }
    rect.y = (d->page_y + d->v0);
    rect.h = height;
    XASSERT(!(rect.w < 0 || rect.h < 0 || rect.w * 2u * rect.h > resource->size - (frame - resource->data) - 4));
    LoadImagePSX(&rect, (uint32 *)(frame + 4));
}

/* Original: SLES_004.74:FUN_8008D0C0 (0x8008D0C0..0x8008D0EC).
 * Full ten-bit descriptor index for explicit lookup (including screen sprites).
 * Not equivalent to the six-bit lookup in the world-sprite renderers. */
VRAM_SPRITE *spr_vram_desc_for_resource(uint32 resource_id)
{
    uint32 bank = resource_id >> 10;
    uint32 idx = resource_id & 0x3ff;
    if (bank >= RESOURCE_SLOT_COUNT || idx >= desc_banks[bank].count)
        return 0;
    return desc_banks[bank].data + idx;
}

/* Inlined original lookup: SLES_004.74 8008D1FC..8008D220 and
 * 8008F5B4..8008F5D8. ANDI at 8008D204/8008F5BC is 0x3F, not 0x3FF.
 * Animation frame lookup still consumes the unmodified resource ID. */
static VRAM_SPRITE *spr_render_desc(uint32 resource_id)
{
    uint32 bank = resource_id >> 10;
    uint32 idx = resource_id & 0x3f;
    if (bank >= RESOURCE_SLOT_COUNT || idx >= desc_banks[bank].count)
        return 0;
    return desc_banks[bank].data + idx;
}

/* Original: FUN_8008D108. */
GDB_CALL void render_world_spr(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *poly0, sint16 ot_bucket, sint32 split_mode, VRAM_SPRITE *desc, sint32 rotation)
{
    FRAME_OBJECT *rec = g_next_frame_object, *next = rec;
    VRAM_SPRITE *d = desc;
    uint8 *frame;
    POLY_FT4 *poly1 = poly0;
    sint32 px, py, w, h, left, top, width_world, height_world, sort_left, split_width;
    sint32 depth, numerator, sx0, sx1, sy0, sy1;
    if (rec < g_frame_objects || rec >= g_frame_objects + 80)
        return;
    if (!poly1)
    {
        XASSERT(!(g_next_transient_prim >= g_transient_prims + TRANSIENT_PRIM_LIMIT));
        poly1 = g_next_transient_prim;
        ++g_next_transient_prim;
        SetPolyFT4(poly1);
    }
    if (div_trunc_256(z) <= (g_map.depth << 7))
        return;
    if (!d)
        d = spr_render_desc(resource);
    if (!d)
        return;
    frame = animation_frame_resource(resource);
    if (!frame)
        return;
    poly1->p[1].attr = d->tpage;
    if (d->cached_resource == resource)
    {
        px = d->pivot_x;
        py = d->pivot_y; /* 8008D3AC loads cached +0x2E and applies the original second -1. */
        w = (uint8)(d->width_minus_one - 1);
        h = d->height;
        if (flip_or_velocity > 0)
        {
            px = -(w + px);
            w = -w;
        }
    }
    else
    {
        px = (sint8)frame[2];
        py = (sint8)frame[3];
        w = (uint8)(frame[0] - 1);
        h = frame[1];
        d->pivot_x = frame[2];
        d->pivot_y = frame[3];
        d->width_minus_one = w;
        d->height = h;
        if (flip_or_velocity > 0)
        {
            px = -(w + px);
            w = -w;
        }
        /* s4 retains the positive frame width while s5 is negated for flip. */
        if (frame_render_work_enabled() != 0)
            upload_dynamic_frame(d, resource, frame[0] - 1, h);
        d->cached_resource = resource;
    }
    left = div_4096_trunc(mul_lo(px * 256, scale_x));
    if (scale_x == 0x1000)
        width_world = w * 256;
    else
        width_world = div_pow2_trunc(mul_lo(w, scale_x), 4);
    top = div_4096_trunc(mul_lo(py * 256, scale_y));
    if (scale_y == 0x1000)
        height_world = h << 8;
    else
        height_world = (div_pow2_trunc(mul_lo(h, scale_y), 4) - 0x100);
    x = ((uint32)x + left);
    y = ((uint32)y + top);
    if (width_world < 0)
        width_world = -width_world;
    sort_left = x;
    split_width = width_world;
    /* Do not clear the record: callers intentionally preset +0x24/+0x26/+0x29
     * immediately before D108 (lighting and force-light flags).  The original
     * writes only the fields below and clears +0x1c plus byte +0x28. */
    rec->prim = poly1;
    rec->world_x = x;
    rec->world_y = ((uint32)y - top);
    rec->world_z = z;
    rec->sort_x = x;
    rec->ot_bucket = ot_bucket;
    rec->cell_x_offset = 0;
    rec->clip_width = 0;
    rec->model_plus_one = 0;
    rec->drawn = 0;
    poly1->p[0].attr = d->clut;
    if (flip_or_velocity < 1)
    {
        poly1->p[0].uv.u = d->u0;
        poly1->p[1].uv.u = (d->u0 + w);
    }
    else
    {
        poly1->p[0].uv.u = (d->u0 - w);
        poly1->p[1].uv.u = d->u0;
    }
    poly1->p[0].uv.v = d->v0;
    poly1->p[2].uv.u = poly1->p[0].uv.u;
    poly1->p[1].uv.v = d->v0;
    poly1->p[2].uv.v = (d->v0 + h - 1);
    poly1->p[3].uv.u = poly1->p[1].uv.u;
    poly1->p[3].uv.v = poly1->p[2].uv.v;
    depth = ((uint32)z - g_camera.world_z);
    if (rotation == 0)
    {
        if (depth == 0)
        {
#if defined(_DEBUG)
            fputs("WORLD SPRITE BREAK 7\n", stderr);
#endif
            abort();
        }

        numerator = (((uint32)x - g_camera.world_x) * 0x140u);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("WORLD SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sx0 = numerator / depth;
        poly1->p[0].xy.x = sx0;
        if (poly1->p[0].xy.x > g_render_state.half_width)
            return;
        numerator = mul_lo(width_world, 0x140);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("WORLD SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sx1 = numerator / depth;
        sx1 = ((uint32)sx0 + sx1);
        poly1->p[1].xy.x = sx1;
        if (poly1->p[1].xy.x < - g_render_state.half_width)
            return;
        numerator = (((uint32)y - g_camera.world_y) * 0x163u);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("WORLD SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sy0 = numerator / depth;
        poly1->p[2].xy.x = poly1->p[0].xy.x;
        poly1->p[3].xy.x = poly1->p[1].xy.x;
        poly1->p[0].xy.y = sy0;
        if (poly1->p[0].xy.y > (div_pow2_trunc(g_render_state.height, 1) + 0x40))
            return;
        numerator = (((uint32)y + height_world - g_camera.world_y) * 0x163u);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("WORLD SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sy1 = numerator / depth;
        poly1->p[1].xy.y = sy0;
        poly1->p[2].xy.y = sy1;
        if (poly1->p[2].xy.y < (-0x40 - div_pow2_trunc(g_render_state.height, 1)))
            return;
        poly1->p[3].xy.y = sy1;
    }
    else
    {
        const uint32 c = div_256_trunc(spr_s_samples[(rotation + 0x100u) & 0x3ff]);
        sint32 sample;
        if ((rotation >= 0 && rotation < 1024))
            sample = spr_s_samples[rotation];
        else if (rotation == 1024)
            sample = 0;
        else
        {
            /* Original unmasked sine reads continue into the adjacent angle table */
            uint32 angle_idx = rotation - 1025u;
            if (angle_idx >= 64 * 64)
            {
#if defined(_DEBUG)
                fputs("SPRITE ANGLE TABLE RANGE\n", stderr);
#endif
                abort();
            }
            sample = g_vector_angles[angle_idx >> 6][angle_idx & 0x3f];
        }
        const uint32 sn = div_256_trunc(sample);
        sint32 right = width_world >> 1, bottom = height_world >> 1;
        POINT_2D offset[4] = {{(-width_world) >> 1, (-height_world) >> 1}, {right, (-height_world) >> 1}, {(-width_world) >> 1, bottom}, {right, bottom}};
        sint32 cx = (uint32)x + right;
        sint32 cy = (uint32)y + bottom;
        POINT_2D world[4], scr[4];
        sint32 i, min_x = 0, max_x = 0, sum_x = 0, sum_y = 0;
        for (i = 0; i < 4; ++i)
        {
            sint32 tx = offset[i].x * c - offset[i].y * sn;
            sint32 ty = offset[i].y * c + offset[i].x * sn;
            sint32 wx = cx + (uint32)div_256_trunc(tx);
            sint32 wy = cy + (uint32)div_256_trunc(ty);
            world[i].x = wx;
            world[i].y = wy;
            if (i == 0 || wx < min_x)
                min_x = wx;
            if (i == 0 || wx > max_x)
                max_x = wx;
        }
        rec->sort_x = min_x;
        sort_left = min_x;
        split_width = ((uint32)max_x - min_x);
        if (depth == 0)
        {
#if defined(_DEBUG)
            fputs("WORLD SPRITE BREAK 7\n", stderr);
#endif
            abort();
        }
        for (i = 0; i < 4; ++i)
        {
            numerator = (((uint32)world[i].x - g_camera.world_x) * 0x140u);
            if (depth == -1 && numerator == INT32_MIN)
            {
#if defined(_DEBUG)
                fputs("WORLD SPRITE X BREAK 6\n", stderr);
#endif
                abort();
            }
            scr[i].x = numerator / depth;
            numerator = (((uint32)world[i].y - g_camera.world_y) * 0x163u);
            if (depth == -1 && numerator == INT32_MIN)
            {
#if defined(_DEBUG)
                fputs("WORLD SPRITE Y BREAK 6\n", stderr);
#endif
                abort();
            }
            scr[i].y = numerator / depth;
        }
        for (i = 0; i < 4; ++i)
        {
            poly1->p[i].xy.x = scr[i].x;
            poly1->p[i].xy.y = scr[i].y;
            sum_x += poly1->p[i].xy.x;
            sum_y += poly1->p[i].xy.y;
        }
        if (div_pow2_trunc(sum_x, 2) + 0x100u > 0x200u || div_pow2_trunc(sum_y, 2) + 0x100u > 0x200u)
            return;
    }
    next = rec + 1;
    {
        sint32 start = 0x40 - (div_trunc_256(sort_left) & 0x3f);
        sint32 remaining = div_trunc_256(split_width), offset = 0;
        if (start < remaining && split_mode != 1 && remaining > 0x17 && (split_mode != 0 || g_map_render.spr_subdiv == 0))
        {
            remaining -= start;
            rec->clip_width = start;
            offset = start;
            while (remaining > 0)
            {
                XASSERT(!(next >= g_frame_objects + FRAME_OBJECT_LIMIT - 1));
                sint32 span = remaining > 0x40 ? 0x40 : remaining;
                *next = *rec;
                next->cell_x_offset = offset;
                next->clip_width = span;
                offset += span;
                remaining -= span;
                next++;
            }
        }
    }
    g_next_frame_object = next;
}

/* Screen SPRT followed by its TPage DR_MODE packet */
/* Original: FUN_8008E5D4. */
GDB_CALL SPRT *render_scr_spr(sint16 x, sint16 y, uint32 resource, sint16 ot_bucket)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    VRAM_SPRITE *d = spr_vram_desc_for_resource(resource);
    SPRT *spr;
    DR_MODE *mode;
    sint32 width, height;
    XASSERT(!(d == 0 || g_cur_render_frame == 0 || ot_bucket < 0 || ot_bucket >= RENDER_OT_LENGTH));
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(SPRT) > RENDER_PRIM_CAPACITY - arena->used));
    spr = (SPRT *)(arena->data.bytes + arena->used);
    SetSprt(spr);
    spr->rgbc.r = spr->rgbc.g = spr->rgbc.b = 0x80;
    width = d->width_minus_one;
    width = width ? width : 0x100;
    height = d->height;
    height = height ? height : 0x100;
    spr->w = width;
    spr->h = height;
    /* Packet coordinates precede the device DRAWENV offset */
    spr->p[0].xy.x = (x + d->pivot_x);
    spr->p[0].xy.y = (y + 0x40 + d->pivot_y);
    spr->p[0].uv.u = d->u0;
    spr->p[0].uv.v = d->v0;
    spr->p[0].attr = (d->clut + (ot_bucket == 0 ? g_map_render.foreground_clut : 0));

    AddPrim(g_cur_render_frame->ot + ot_bucket, spr);
    arena->used += sizeof(SPRT);
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(DR_MODE) > RENDER_PRIM_CAPACITY - arena->used));
    mode = (DR_MODE *)(arena->data.bytes + arena->used);
    memset(mode, 0, sizeof(*mode));
    SetDrawMode(mode, 0, 0, d->tpage, 0);
    AddPrim(g_cur_render_frame->ot + ot_bucket, mode);
    arena->used += sizeof(DR_MODE);
    return spr;
}

/* Original: FUN_8008F54C. */
void render_world_spr_immediate(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *poly0, sint32 ot_bucket, VRAM_SPRITE *desc, sint32 rotation)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    POLY_FT4 poly2, *poly1 = poly0;
    VRAM_SPRITE *d = spr_render_desc(resource);
    uint8 *frame;
    uint8 w;
    sint32 h, signed_w, left, top, width_world, height_world, depth, numerator, sx0, sx1, sy0, sy1;
    if (!poly1)
    {
        memset(&poly2, 0, sizeof(poly2));
        SetPolyFT4(&poly2);
        poly2.rgbc.r = poly2.rgbc.g = poly2.rgbc.b = 0x80;
        poly1 = &poly2;
    }
    if (!d)
        d = desc;
    if (!d)
        return;
    frame = animation_frame_resource(resource);
    if (!frame)
        return;
    poly1->p[1].attr = d->tpage;
    if (d->cached_resource == resource)
    { /* 8008F6F4 has the same cached-width -1 as D108. */
        w = d->width_minus_one - 1;
        h = d->height;
    }
    else
    {
        d->pivot_x = frame[2];
        d->pivot_y = frame[3];
        w = frame[0] - 1;
        h = frame[1];
        d->width_minus_one = w;
        d->height = h;
        upload_dynamic_frame(d, resource, w, h);
        d->cached_resource = resource;
    }
    signed_w = flip_or_velocity > 0 ? -w : w;
    left = div_4096_trunc(mul_lo(-w * 128, scale_x));
    width_world = scale_x == 0x1000 ? signed_w * 256 : div_pow2_trunc(mul_lo(signed_w, scale_x), 4);
    top = div_4096_trunc(mul_lo(-h * 256, scale_y));
    height_world = scale_y == 0x1000 ? h << 8 : (div_pow2_trunc(mul_lo(h, scale_y), 4) - 0x100);
    x = ((uint32)x + left);
    y = ((uint32)y + top);
    if (width_world < 0)
        width_world = -width_world;
    poly1->p[0].attr = d->clut;
    poly1->p[0].uv.u = d->u0;
    poly1->p[0].uv.v = d->v0;
    poly1->p[1].uv.u = (d->u0 + signed_w);
    poly1->p[1].uv.v = d->v0;
    poly1->p[2].uv.u = poly1->p[0].uv.u;
    poly1->p[2].uv.v = (d->v0 + h - 1);
    poly1->p[3].uv.u = poly1->p[1].uv.u;
    poly1->p[3].uv.v = poly1->p[2].uv.v;
    depth = ((uint32)z - g_camera.world_z);
    if (rotation == 0)
    {
        if (depth == 0)
        {
#if defined(_DEBUG)
            fputs("IMMEDIATE SPRITE BREAK 7\n", stderr);
#endif
            abort();
        }

        numerator = (((uint32)x - g_camera.world_x) * 0x140u);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("IMMEDIATE SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sx0 = numerator / depth;
        poly1->p[0].xy.x = sx0;
        if (poly1->p[0].xy.x > g_render_state.half_width)
            return;
        numerator = mul_lo(width_world, 0x140);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("IMMEDIATE SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sx1 = numerator / depth;
        sx1 = ((uint32)sx0 + sx1);
        poly1->p[1].xy.x = sx1;
        if (poly1->p[1].xy.x < - g_render_state.half_width)
            return;
        numerator = (((uint32)y - g_camera.world_y) * 0x163u);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("IMMEDIATE SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sy0 = numerator / depth;
        poly1->p[2].xy.x = poly1->p[0].xy.x;
        poly1->p[3].xy.x = poly1->p[1].xy.x;
        poly1->p[0].xy.y = sy0;
        if (poly1->p[0].xy.y > div_pow2_trunc(g_render_state.height, 1))
            return;
        numerator = (((uint32)y + height_world - g_camera.world_y) * 0x163u);
        if (depth == -1 && numerator == INT32_MIN)
        {
#if defined(_DEBUG)
            fputs("IMMEDIATE SPRITE BREAK 6\n", stderr);
#endif
            abort();
        }
        sy1 = numerator / depth;
        poly1->p[1].xy.y = sy0;
        poly1->p[2].xy.y = sy1;
        if (poly1->p[2].xy.y < -div_pow2_trunc(g_render_state.height, 1))
            return;
        poly1->p[3].xy.y = sy1;
    }
    else
    {
        sint32 c = div_256_trunc(spr_s_samples[(rotation + 0x100u) & 0x3ff]);
        sint32 sample, sn;
        if ((rotation >= 0 && rotation < 1024))
            sample = spr_s_samples[rotation];
        else if (rotation == 1024)
            sample = 0;
        else
        {
            /* Original unmasked sine reads continue into the adjacent angle table */
            uint32 angle_idx = rotation - 1025u;
            if (angle_idx >= 64 * 64)
            {
#if defined(_DEBUG)
                fputs("SPRITE ANGLE TABLE RANGE\n", stderr);
#endif
                abort();
            }
            sample = g_vector_angles[angle_idx >> 6][angle_idx & 0x3f];
        }
        sn = div_256_trunc(sample);
        sint32 width_units = div_256_trunc(width_world), height_units = div_256_trunc(height_world);
        sint32 right = width_units >> 1, bottom = height_units >> 1;
        POINT_2D offset[4] = {{(-width_units) >> 1, (-height_units) >> 1}, {right, (-height_units) >> 1}, {(-width_units) >> 1, bottom}, {right, bottom}};
        sint32 cx = div_256_trunc(((uint32)x + (width_world >> 1)));
        sint32 cy = div_256_trunc(((uint32)y + (height_world >> 1)));
        POINT_2D scr[4];
        sint32 i, sum_x = 0, sum_y = 0;
        if (depth == 0)
        {
#if defined(_DEBUG)
            fputs("IMMEDIATE SPRITE BREAK 7\n", stderr);
#endif
            abort();
        }
        for (i = 0; i < 4; ++i)
        {
            sint32 tx = offset[i].x * c - offset[i].y * sn;
            sint32 ty = offset[i].y * c + offset[i].x * sn;
            uint32 wx = (cx + (uint32)div_256_trunc(tx)) << 8;
            uint32 wy = (cy + (uint32)div_256_trunc(ty)) << 8;
            numerator = ((wx - g_camera.world_x) * 0x140u);
            if (depth == -1 && numerator == INT32_MIN)
            {
#if defined(_DEBUG)
                fputs("IMMEDIATE SPRITE X BREAK 6\n", stderr);
#endif
                abort();
            }
            scr[i].x = numerator / depth;
            numerator = ((wy - g_camera.world_y) * 0x163u);
            if (depth == -1 && numerator == INT32_MIN)
            {
#if defined(_DEBUG)
                fputs("IMMEDIATE SPRITE Y BREAK 6\n", stderr);
#endif
                abort();
            }
            scr[i].y = numerator / depth;
        }
        for (i = 0; i < 4; ++i)
        {
            poly1->p[i].xy.x = scr[i].x;
            poly1->p[i].xy.y = scr[i].y;
            sum_x += poly1->p[i].xy.x;
            sum_y += poly1->p[i].xy.y;
        }
        if (div_pow2_trunc(sum_x, 2) + 0x100u > 0x200u || div_pow2_trunc(sum_y, 2) + 0x100u > 0x200u)
            return;
    }
    if (!arena || !g_cur_render_frame)
        return;
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(POLY_FT4) > RENDER_PRIM_CAPACITY - arena->used));
    POLY_FT4 *poly3 = (POLY_FT4 *)(arena->data.bytes + arena->used);
    memcpy(poly3, poly1, sizeof(*poly3));
    AddPrim(g_cur_render_frame->ot + ot_bucket, poly3);
    arena->used += sizeof(POLY_FT4);
}

static void spr_resource_reset(void)
{
    uint32 idx;
    for (idx = 0; idx < RESOURCE_SLOT_COUNT; ++idx)
        resource_table_register(idx << 10, 0, 0, RESOURCE_BORROW);
    memset(desc_pool, 0, sizeof(desc_pool));
    memset(desc_banks, 0, sizeof(desc_banks));
    desc_next = desc_pool;
    text_renderer_resource_reset();
}

/* Original: FUN_8008D0EC. */
static void spr_desc_table_register(uint32 resource_id, uint32 count, VRAM_SPRITE *data)
{
    uint32 bank = resource_id >> 10;
    XASSERT(bank < RESOURCE_SLOT_COUNT);
    desc_banks[bank].data = data;
    desc_banks[bank].count = count;
}

/* Original: FUN_8008CFB0. */
void spr_upload_pix(VRAM_SPRITE *d, uint32 resource_id, sint32 mode)
{
    const RESOURCE_DATA *resource = resource_table_data(resource_id);
    uint8 *frame = animation_frame_resource(resource_id);
    uint8 *pix;
    PSX_RECT rect;
    sint32 width;
    XASSERT(!(d == 0 || frame == 0 || resource == 0));
    XASSERT(!(mode != 0 && mode != 1));
    pix = frame + 4;
    d->pivot_x = pix[-2];
    d->pivot_y = pix[-1];
    d->width_minus_one = pix[-4];
    d->height = pix[-3];
    rect.x = (d->page_x + (mode == 0 ? d->u0 / 4 : d->u0 / 2));
    rect.y = (d->v0 + d->page_y);
    width = d->u1 - d->u0 + 1;
    rect.w = (mode == 0 ? width / 4 : width / 2);
    rect.h = (d->v1 - d->v0 + 1);
    XASSERT(!(rect.w < 0 || rect.h < 0 || rect.w * 2u * rect.h > resource->size - (pix - resource->data)));
    LoadImagePSX(&rect, (uint32 *)pix);
    DrawSync(0);
}

/* Original: FUN_8008CE24. */
GDB_CALL VRAM_SPRITE *spr_create_vram_descs(uint32 resource_id, sint32 frame_count, uint16 clut, sint16 abr)
{
    VRAM_SPRITE *first = desc_next;
    uint16 clut_width = spr_clut_entry_count(resource_id);
    sint32 mode = (clut_width == 0x100);
    sint32 alignment_mask = (clut_width == 0x10) ? 7 : (mode ? 3 : 0);
    sint32 i;
    XASSERT(!(frame_count < 0 || (frame_count > 0 && frame_count >= (desc_pool + 190) - desc_next)));
    spr_desc_table_register(resource_id, frame_count, first);
    for (i = 0; i < frame_count; i++)
    {
        VRAM_SPRITE *d = desc_next;
        uint8 *frame = animation_frame_resource(resource_id);
        uint16 width, height;
        XASSERT(!(frame == 0));
        width = frame[0] ? frame[0] : 0x100;
        height = frame[1] ? frame[1] : 0x100;
        spr_vram_allocate(d, ((width + alignment_mask) & ~alignment_mask), height, mode);
        /* 8008CF04 passes descriptor +0x22,+0x1e,+0x20.  The local UV
         * coordinates are not VRAM page coordinates. */
        d->tpage = GetTPage(d->pixel_mode, abr, d->page_x, d->page_y);
        spr_upload_pix(d, resource_id, mode);
        d->cached_resource = resource_id;
        d->clut = clut;
        desc_next++;
        resource_id++;
    }
    return first;
}

/* Original: FUN_8008CD98. */
void spr_assign_clut_range(uint32 resource_id, sint16 frame_count, uint16 clut)
{
    while (frame_count > 0)
    {
        VRAM_SPRITE *desc;
        uint32 bank = resource_id >> 10;
        XASSERT(bank < SPRITE_CLUT_SLOT_COUNT);
        desc = spr_vram_desc_for_resource(resource_id);
        if (desc != 0)
            desc->clut = clut;
        g_spr_pal.cluts[resource_id >> 10] = clut;
        --frame_count;
        ++resource_id;
    }
}

/* Original: FUN_800963E0. */
GDB_CALL uint32 spr_clut_upload(uint32 resource_id, sint32 semi_transparent)
{
    uint16 converted[256], count = spr_clut_entry_count(resource_id);
    sint16 x;
    uint8 *frame = animation_frame_resource(resource_id), *src;
    const RESOURCE_DATA *resource = resource_table_data(resource_id);
    PSX_RECT rect;
    sint32 i;
    if (!frame || !resource || count == 0 || count > 256 || frame - resource->data < count * 2)
        return 0;
    if (count * 2 == 0x20)
    {
        if (g_spr_pal.cursor_x == 0x240)
        {
            g_spr_pal.cursor_x = 0x140;
            g_spr_pal.cursor_y = (g_spr_pal.cursor_y + 1);
        }
    }
    else if (g_spr_pal.cursor_x >= 0x141)
    {
        g_spr_pal.cursor_x = 0x140;
        g_spr_pal.cursor_y = (g_spr_pal.cursor_y + 1);
    }
    src = frame - count * 2;
    for (i = 0; i < count; i++)
    {
        uint32 color = xport_load_le16(src + i * 2);
        uint16 out = color;
        if (g_session.attract_mode != 0)
        {
            uint32 grey = (((color >> 10) & 31) + ((color >> 5) & 31) + (color & 31)) / 3;
            out = ((((grey * 2) / 3) << 10) + (grey << 5) + grey);
            if ((out & 0x7fff) == 0 && i != 0)
                out = 1;
        }
        if (semi_transparent && i != 0)
            out |= 0x8000;
        converted[i] = out;
    }
    rect.x = g_spr_pal.cursor_x;
    rect.y = g_spr_pal.cursor_y;
    rect.w = count;
    rect.h = 1;
    LoadImagePSX(&rect, (uint32 *)converted);
    DrawSync(0);
    x = g_spr_pal.cursor_x;
    g_spr_pal.cursor_x = (g_spr_pal.cursor_x + count);
    XASSERT(g_spr_pal.cursor_y < 0x40);
    return (g_spr_pal.cursor_y * 0x40u + (x / 16)) & 0xffffu;
}

/* Original: FUN_8008E7DC. */
static void render_background_strips(uint16 x, sint32 y, sint32 rows, sint8 u_offset)
{
    while (rows > 0)
    {
        sint32 column;
        y = (y + 0x40u);
        for (column = 0; column < 6; column++)
        {
            SPRT *p = render_scr_spr(((x & 0x3f) - 0xe0 + column * 0x3f), y, 0x30400, 0x480);
            sint32 shade = g_map_render.background_brightness + 0x80u;
            if (shade < 0)
                shade = 0;
            setRGB0(p, shade, shade, shade);
            setWH(p, 0x40, 0x40);
            /* Original E880 replaces the helper's internal Y offset */
            p->p[0].xy.y = y;
            p->p[0].uv.u = (p->p[0].uv.u + u_offset);
        }
        --rows;
    }
}

/* Original: FUN_8008E8D0. */
void render_stage_background(void)
{
    uint32 resource = g_stage_env ? g_stage_env->background_resource : 0;
    sint32 x; sint32 y; sint32 top; sint32 rows; sint32 column;
    if (!resource)
        return;
    x = ((sint32)(-(uint32)g_camera.world_x) / 6) / 0x100;
    if (g_session.stage == 3)
        x += g_render_state.frame;
    x &= 0xff;
    if (g_camera.prev_z != -1)
    {
        sint32 delta = (uint32)g_camera.world_z - g_camera.prev_z;
        g_map_render.background_y = ((uint32)g_map_render.background_y - (delta / 0x18));
        delta = ((uint32)g_camera.world_y - g_camera.prev_y);
        g_map_render.background_y = ((uint32)g_map_render.background_y - (delta / 7));
    }
    y = g_map_render.background_y / 0x100;
    top = (g_map_render.vertical_extent + 0x100u - g_render_state.height);
    if (y < -top)
    {
        sint32 distance = top - 0x100u - y;
        if (distance < 0)
            distance = (-(uint32)distance);
        render_background_strips(x, (y + 0x100u), (sint16)distance / 0x40 + 1, 0x40);
    }
    if (-(g_render_state.height / 2) < y)
    {
        sint32 distance = y + g_map_render.vertical_extent;
        if (distance < 0)
            distance = (-(uint32)distance);
        rows = (sint16)distance / 0x40 + 1;
        render_background_strips(x, (y - rows * 0x40u), rows, 0);
    }
    for (column = 0; column < 3; column++)
    {
        SPRT *p = render_scr_spr((x - 0x1a0 + column * 0x100), y, resource, 0x480);
        sint32 shade = g_map_render.background_brightness + 0x80u;
        if (shade < 0)
            shade = 0;
        p->h = 0x100;
        setRGB0(p, shade, shade, shade);
    }
}

/* Direct translation of FUN_800A68E8.  CLUT allocation follows in each
 * original caller and therefore deliberately remains outside this helper. */
void spr_archive_resource_load(const char *name, uint32 resource, sint32 frames, sint32 mode, const void *archive, sint32 archive_size)
{
    RESOURCE_DATA member = archive_member_find(name, archive, archive_size);

    if (member.data == 0)
    {
        fatal_error(name);
        abort();
    }
#if defined(_DEBUG)
    sint32 frames_valid = member.size >= 4 && xport_load_le_s32(member.data) >= frames;
    XASSERT(frames_valid);
#endif
    resource_table_register(resource, member.data, member.size, RESOURCE_COPY);
    spr_create_vram_descs(resource, frames, 0, mode);
}

/* FUN_8008CD54: the first halfword following the frame-offset table is the
 * CLUT width (16 or 256 entries). */
static uint16 spr_clut_entry_count(uint32 resource_id)
{
    const RESOURCE_DATA *resource = resource_table_data(resource_id);
    uint32 offset;
    uint16 count;
    if (resource == 0 || resource->size == 0)
        return 0;
    /* Original CD54 truncates the incremented frame-count byte */
    offset = (uint8)(resource->data[0] + 1u) * 4;
    if (offset > resource->size || resource->size - offset < 4)
        return 0;
    count = xport_load_le16(resource->data + offset);
    if (count * 2u > resource->size - offset - 4)
        return 0;
    return count;
}

/* Original: FUN_8008C814. */
void spr_vram_defer_release(VRAM_SPRITE *resource)
{
    if (deferred_resource_count == deferred_resource_capacity)
    {
        VRAM_SPRITE **resources;
        uint32 capacity;
        XASSERT(!(deferred_resource_count > 0xffffffffu - 16u || deferred_resource_count + 16u > SIZE_MAX / sizeof(*deferred_resources)));
        capacity = deferred_resource_count + 16u;
        resources = realloc(deferred_resources, capacity * sizeof(*resources));
        XASSERT(!(resources == 0));
        deferred_resources = resources;
        deferred_resource_capacity = capacity;
    }
    deferred_resources[deferred_resource_count++] = resource;
}

/* End descriptor links when their owning actor heap is reset or released */
void spr_vram_reset_deferred(void)
{
    sint32 idx;
    free(deferred_resources);
    deferred_resources = 0;
    deferred_resource_count = 0;
    deferred_resource_capacity = 0;
    for (idx = 0; idx < vram_region_count; ++idx)
    {
        vram_regions[idx].head = 0;
        vram_regions[idx].tail = 0;
    }
}

/* Deferred native descriptors unlink from their owning region before heap sweep */
/* Original: FUN_8008C838. */
void spr_vram_release_deferred(void)
{
    uint32 idx;
    for (idx = 0; idx < deferred_resource_count; idx++)
    {
        VRAM_SPRITE *desc = deferred_resources[idx];
        VRAM_REGION *region;
        if (desc == 0)
            break;
        region = desc->region;
        if (region != 0)
        {
            if (desc->prev != 0)
                desc->prev->next = desc->next;
            else
                region->head = desc->next;
            if (desc->next != 0)
                desc->next->prev = desc->prev;
            else
                region->tail = desc->prev;
            desc->prev = 0;
            desc->next = 0;
            desc->region = 0;
        }
        deferred_resources[idx] = 0;
    }
    deferred_resource_count = 0;
}

/* Original: FUN_8008CC88. */
static sint32 spr_vram_rect_overlaps(sint32 x, sint32 y, uint16 w, uint16 h, VRAM_SPRITE *d)
{
    if (d->u1 < x)
        return 0;
    if (x + w < d->u0)
        return 0;
    if (d->v1 < y)
        return 0;
    return !(y + h < d->v0);
}

/* Original: FUN_8008C8B8. */
GDB_CALL void spr_vram_reset(const VRAM_LAYOUT *layout)
{
    uint32 i;
    XASSERT(!(layout == 0 || layout->count > sizeof(vram_regions) / sizeof(vram_regions[0])));
    memset(vram_regions, 0, sizeof(vram_regions));
    vram_region_count = 0;
    spr_vram_reset_deferred();
    spr_resource_reset();
    g_spr_pal.cursor_x = 0x140;
    g_spr_pal.cursor_y = 0;
    for (i = 0; i < layout->count; ++i)
    {
        VRAM_REGION *r = &vram_regions[i];
        const VRAM_REGION_CFG *cfg = &layout->regions[i];
        r->x = cfg->x;
        r->y = cfg->y;
        r->width = cfg->width;
        r->height = cfg->height;
        r->mode = cfg->mode;
        r->tpage = LoadTPage(0, r->mode, 0, r->x, r->y, 0, 0);
        DrawSync(0);
    }
    vram_region_count = layout->count;
}

/* Original: FUN_8008CA48. */
GDB_CALL sint32 spr_vram_allocate(VRAM_SPRITE *d, uint16 w, uint16 h, sint32 mode)
{
    sint32 ri, candidate_seen = 0;
    sint32 max_x = 0x100 - w;
    sint32 max_y = 0x100 - h;
    for (ri = 0; ri < vram_region_count; ri++)
    {
        VRAM_REGION *r = &vram_regions[ri];
        VRAM_SPRITE *o;
        sint32 x = 0, y = 0x100 - r->height;
        if (r->mode != mode || (!candidate_seen && y > max_y))
            continue;
        candidate_seen = 1;
        o = r->head;
        while (o != 0)
        {
            VRAM_SPRITE *next;
            if (spr_vram_rect_overlaps(x, y, w, h, o))
            {
                x = o->u1 + 1;
                next = r->head;
            }
            else
            {
                next = o->next;
            }
            if (x > max_x)
            {
                y = y + 0x10;
                x = 0;
            }
            if (y > max_y)
                break;
            o = next;
        }
        if (o != 0)
            continue;
        d->prev = r->tail;
        d->next = 0;
        if (r->tail != 0)
            r->tail->next = d;
        else
            r->head = d;
        r->tail = d;
        d->region = r;
        d->u0 = x;
        d->u1 = x + w - 1;
        d->v0 = y;
        d->v1 = y + h - 1;
        d->tpage = r->tpage;
        d->page_x = r->x;
        d->page_y = r->y;
        d->pixel_mode = mode;
        return 1;
    }
    if (g_vram_error_reporting_enabled != 0)
    {
        fatal_error_with_val(candidate_seen ? "OUT OF VRAM2 " : "OUT OF VRAM1 ", mode);
        abort();
    }
    return 0;
}
