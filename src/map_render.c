#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "map.h"
#include "game.h"
#include "map_render.h"
#include "camera.h"
#include "game_runtime.h"
#include "level_data.h"
#include "model.h"
#include "render.h"
#include "spatial.h"
#include "sprite.h"
#include "sprite_renderer.h"

/* Native object light starts at Q7 unity */
MAP_RENDER_STATE g_map_render = {0x80};

typedef struct FLOOR_VERTEX
{
    sint16 x, y;
    uint8 u, v;
} FLOOR_VERTEX;

/* One frame's last floor selection, not a material descriptor registry */
typedef struct FLOOR_TEXTURE_CACHE
{
    sint32 key;
    uint16 uv[8];
    uint16 tpage, clut;
} FLOOR_TEXTURE_CACHE;

/* Original key: 800D40B0, cyclic UV scratch: 800E2DD0..800E2DDF */
static FLOOR_TEXTURE_CACHE floor_tex;

/* Floor geometry emits a typed packet into the active native arena */
/* Original: SLES_004.74:FUN_80093600. */
static void floor_tile_render(sint32 row_back, sint32 world_x, MAP_FLOOR_CELL *cell)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    POLY_FT4 *poly;
    FLOOR_VERTEX q[4];
    MAP_FLOOR_CELL *next;
    const VRAM_SPRITE *desc;
    sint32 cell_idx, resource;
    uint32 light_idx;
    uint32 row_front; sint32 back_depth; sint32 front_depth; sint32 slope; sint32 orientation;
    sint32 numerator0, numerator1, key, bu, bv, i, rot, shade;
    uint8 tile;
    sint32 material, special, map_light, bucket;

    if (!cell)
        return;
    cell_idx = (cell - g_map.floor);
    XASSERT(!(g_map.width <= 0 || cell_idx >= g_map.floor_count || g_map.floor_count - cell_idx < 2));
    row_front = (row_back - 0x4000u);
    back_depth = ((uint32)row_back - g_camera.world_z);
    front_depth = (row_front - g_camera.world_z);
    orientation = cell->packed_orientation >> 3;
    slope = (orientation < 4 ? -orientation : orientation - 8) * 256;
    numerator0 = (((cell->height * 256u) + slope - g_camera.world_y) * 355u);
    numerator1 = (((cell[1].height * 256u) + slope - g_camera.world_y) * 355u);
    if (!back_depth)
    {
#if defined(_DEBUG)
        fputs("FLOOR PROJECTION BREAK 7\n", stderr);
#endif
        abort();
    }
    if (back_depth == -1 && (numerator0 == (-2147483647 - 1) || numerator1 == (-2147483647 - 1)))
    {
#if defined(_DEBUG)
        fputs("FLOOR PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    q[0].y = numerator0 / back_depth;
    q[1].y = numerator1 / back_depth;
    if (q[0].y >= 225 && q[1].y >= 225)
        return;
    if (cell->material == 255)
        return;
    XASSERT(!(g_stage_env == 0 || g_stage_env->material_lookup == 0));
    /* The row's front edge selects the following light-grid row */
    light_idx = cell_idx + g_map.width;
    XASSERT(!(!g_map.light.vals || light_idx >= g_map.light.count));
    map_light = g_map.light.vals[light_idx];
    shade = ((uint32)cell->shade - g_map_render.depth_light_bias + map_light + g_map_render.brightness + (orientation << 4));
    if (shade < 0)
        shade = 0;
    if (shade > 255)
        shade = 255;
    XASSERT(g_map.floor_count - cell_idx > g_map.width + 1);
    next = cell + g_map.width;
    numerator0 = (((next->height * 256u) + slope - g_camera.world_y) * 355u);
    numerator1 = (((next[1].height * 256u) + slope - g_camera.world_y) * 355u);
    if (!front_depth)
    {
#if defined(_DEBUG)
        fputs("FLOOR PROJECTION BREAK 7\n", stderr);
#endif
        abort();
    }
    if (front_depth == -1 && (numerator0 == (-2147483647 - 1) || numerator1 == (-2147483647 - 1)))
    {
#if defined(_DEBUG)
        fputs("FLOOR PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    q[2].y = numerator0 / front_depth + 1u;
    q[3].y = numerator1 / front_depth + 1u;

    material = g_stage_env->material_lookup[cell->material];
    special = g_stage_env->map_resource;
    tile = cell->tile_flags & 0x3f;
    if (material == special)
    {
        tile += (g_render_state.frame >> 2) & 7;
        if ((g_render_state.frame & 1) == 0)
        {
            sint16 val = cell->height + cell->height_step;
            cell->height = val;
            if (val < -g_map.terrain_variation)
                cell->height_step = 1;
            if (val > 0)
                cell->height_step = -1;
            if ((cell[1].material == 0xff ? *g_void_floor_material : g_stage_env->material_lookup[cell[1].material]) != special)
            {
                val = cell[1].height + cell[1].height_step;
                cell[1].height = val;
                if (val < -g_map.terrain_variation)
                    cell[1].height_step = 1;
                if (val > 0)
                    cell[1].height_step = -1;
            }
        }
    }
    /* The original key deliberately overlaps material and tile bits */
    key = (cell->material << 4) + tile;
    if (key != floor_tex.key)
    {
        resource = material;
        desc = spr_vram_desc_for_resource((resource & ~0x3ffu) | (resource & 0x3fu));
        XASSERT(!(desc == 0));
        bu = desc->u0 + (tile & 3) * 64;
        bv = desc->v0 + (tile >> 2) * 64;
        floor_tex.uv[0] = floor_tex.uv[6] = bu;
        floor_tex.uv[1] = floor_tex.uv[3] = bv;
        floor_tex.uv[2] = floor_tex.uv[4] = bu + 63;
        floor_tex.uv[5] = floor_tex.uv[7] = bv + 63;
        floor_tex.tpage = desc->tpage;
        floor_tex.clut = desc->clut;
        floor_tex.key = key;
    }
    /* Grid initialization publishes even selectors; light decay subtracts eight */
    rot = cell->packed_orientation & 7;
    XASSERT((rot & 1) == 0);
    for (i = 0; i < 4; ++i)
    {
        sint32 n = (rot + i * 2) & 7;
        sint32 vertex = i < 2 ? i : 5 - i;
        q[vertex].u = floor_tex.uv[n];
        q[vertex].v = floor_tex.uv[n + 1];
    }
    if (cell->tile_flags & 0x40)
    {
        sint32 t = q[0].u;
        q[0].u = q[1].u;
        q[1].u = t;
        t = q[2].u;
        q[2].u = q[3].u;
        q[3].u = t;
    }
    if (cell->tile_flags & 0x80)
    {
        sint32 t = q[0].v;
        q[0].v = q[2].v;
        q[2].v = t;
        q[1].v = q[0].v;
        q[3].v = q[2].v;
    }
    numerator0 = (((uint32)world_x - g_camera.world_x) * 320u);
    if ((back_depth == -1 || front_depth == -1) && numerator0 == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("FLOOR PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    numerator1 = numerator0 / back_depth;
    q[0].x = numerator1;
    q[1].x = (uint32)numerator1 + g_map_render.back_scale;
    numerator1 = numerator0 / front_depth;
    q[2].x = numerator1;
    q[3].x = (uint32)numerator1 + g_map_render.front_scale;
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(POLY_FT4) > RENDER_PRIM_CAPACITY - arena->used));
    poly = (POLY_FT4 *)(arena->data.bytes + arena->used);
    arena->used += sizeof(POLY_FT4);
    memset(poly, 0, sizeof(*poly));
    setPolyFT4(poly);
    setRGB0(poly, shade, shade, shade);
    setXY4(poly, q[0].x, q[0].y, q[1].x, q[1].y, q[2].x, q[2].y, q[3].x, q[3].y);
    setUV4(poly, q[0].u, q[0].v, q[1].u, q[1].v, q[2].u, q[2].v, q[3].u, q[3].v);
    poly->p[0].attr = floor_tex.clut;
    poly->p[1].attr = floor_tex.tpage;
    XASSERT(!(g_cur_render_frame == 0));
    bucket = g_model_draw.depth_bucket;
    XASSERT(!(bucket < 0 || bucket >= RENDER_OT_LENGTH));
    AddPrim(g_cur_render_frame->ot + bucket, poly);
    psx_set_prim_depth(-1);
    psx_set_prim_ot_bucket(-1);
}

static void cell_render(sint32 x, sint32 z, sint32 row_back, MAP_FLOOR_CELL *cell)
{
    if (!cell || x < 0 || x >= g_map.width)
        return;
    /* Original 8009425C/80094308: models precede the optional floor packet */
    model_draw_cell(cell, x, z, g_model_draw.depth_bucket);
    if (cell->material != 0xff)
        floor_tile_render(row_back, ((uint32)x << 14), cell);
}

/* Original: FUN_80093DE0. */
void render_map_scene(void)
{
    sint32 z; sint32 x; uint32 row_back; sint32 row_front; sint32 far_z; sint32 left; uint32 right; sint32 depth; sint32 count; sint32 split; sint32 deferred_left; sint32 deferred_count;
    sint32 numerator, back_depth, front_depth;
    sint32 row_idx;
    floor_tex.key = -1;
    g_map_render.foreground_clut = 0;
    g_map_render.background_resource = 0;
    g_model_draw.depth_bucket = ot_depth_row_bucket(0);
    g_map_render.depth_anchor_z = 0;
    far_z = (g_camera.world_z + ((uint32)g_map_render.effect_depth << 8));
    if (g_map_render.far_z < far_z)
        far_z = g_map_render.far_z;
    /* Literal 0x80093E50..0x80094364 row and horizontal-cell traversal. */
    for (z = 0; z < g_map.depth; ++z)
    {
        row_idx = (uint32)z * g_map.width;
        row_back = g_map.depth * 0xc000u - (z << 14);
        row_front = (row_back - 0x4000u);
        if (row_front < 1)
        {
            return;
        }
        g_map_render.spr_subdiv = 0;
        if (row_front > far_z)
            continue;
        if (g_map_render.depth_anchor_z == 0)
        {
            g_map_render.depth_anchor_z = row_front;
            if (frame_render_work_enabled() == 0)
            {
                return;
            }
        }
        if (z == 0)
            g_map_render.background_resource = 1;
        if (row_front < (g_camera.world_z + 0x4000))
        {
            return;
        }
        /* FUN_80093DE0: depth-dependent baseline subtracted from MAPFLOOR light. */
        {
            sint32 light_depth = (sint32)((uint32)row_front - g_camera.world_z) / 0x100;
            sint16 bias = 0, mode = 0;
            if (g_stage_env)
            {
                bias = g_stage_env->disp.fields.light_bias;
                mode = g_stage_env->music.fields.flags;
            }
            light_depth += bias;
            g_map_render.depth_light_bias = mode ? -(light_depth / 5 + 0x80) : light_depth / 7;
        }
        /* Reverse deferred left cells to preserve prepend-based OT order */
        depth = (row_front - (g_camera.world_z - 0x4000u));
        numerator = ((-(uint32)g_render_state.half_width) * depth);
        left = ((uint32)g_camera.world_x + (numerator / 320));
        left = (left & 0xffff4000u);
        if (left < 0)
            left = 0;
        numerator = ((uint32)g_render_state.half_width * depth);
        right = ((uint32)g_camera.world_x + (numerator / 320));
        count = (sint32)(right - left) / 0x4000 + 1;
        split = 1;
        deferred_left = left >> 14;
        deferred_count = 0;
        back_depth = (row_back - g_camera.world_z);
        front_depth = ((uint32)row_front - g_camera.world_z);
        if (!back_depth || !front_depth)
        {
#if defined(_DEBUG)
            fputs("MAP ROW PROJECTION BREAK 7\n", stderr);
#endif
            abort();
        }
        g_map_render.back_scale = 0x500000 / back_depth + 1;
        g_map_render.front_scale = 0x500000 / front_depth + 1;
        for (x = left >> 14; count > 0; x++, count--)
        {
            MAP_FLOOR_CELL *cell;
            if (x >= g_map.width)
                break;
            XASSERT(!(g_map.floor == 0 || row_idx >= g_map.floor_count || x >= g_map.floor_count - row_idx));
            cell = g_map.floor + row_idx + x;
            if (split)
            {
                numerator = ((((uint32)x << 14) - (g_camera.world_x - 0x4000u)) * 320u);
                if (front_depth == -1 && numerator == (-2147483647 - 1))
                {
#if defined(_DEBUG)
                    fputs("MAP ROW PROJECTION BREAK 6\n", stderr);
#endif
                    abort();
                }
                if (numerator / front_depth < 0)
                {
                    ++deferred_count;
                    continue;
                }
            }
            split = 0;
            cell_render(x, z, row_back, cell);
        }
        while (deferred_count > 0)
        {
            x = deferred_left + --deferred_count;
            cell_render(x, z, row_back, g_map.floor + row_idx + x);
        }
        g_model_draw.depth_bucket = (g_model_draw.depth_bucket - 0x40u);
    }
}
