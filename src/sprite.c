#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "cc_archive.h"
#include "global.h"
#include "object.h"
#include "original_tables.h"
#include "platform/win/game_platform.h"
#include "player.h"
#include "psx.h"
#include "resource_table.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "stubs.h"
#include "text_renderer.h"
#include "vram_alloc.h"

/* Variables. */
/* Semantic native-port module; original address comments are preserved. */
/* Exact C translation of FUN_8008CE24/FUN_8008CFB0/FUN_8008D0C0/
 * FUN_8008D0EC.  These functions bind frame resources to the 0x30-byte VRAM
 * descriptors consumed by FUN_8008D108 and FUN_8008F54C. */

static VRAM_SPRITE descriptor_pool[190];

static VRAM_SPRITE *deferred_resources[257];
static sint32 deferred_resource_count;

static VRAM_SPRITE *descriptor_next = descriptor_pool;

static VRAM_SPRITE *descriptor_bases[256];

/* Functions. */
/* FUN_8008D108/FUN_8008F54C build this upload rectangle on their stacks.
 * It describes the current packed frame, not the full descriptor allocation. */
static void upload_dynamic_frame(VRAM_SPRITE *d, uint8 *frame, sint32 width_minus_one, sint32 height)
{
    PSX_RECT rect;
    sint32 rounded;
    if (d->pixel_mode == 0)
    {
        rounded = ((uint8)width_minus_one + 8) & 0xf8;
        rect.w = (sint16)(rounded / 4);
        rect.x = (sint16)(d->page_x + d->u0 / 4);
    }
    else if (d->pixel_mode == 1)
    {
        rounded = ((uint8)width_minus_one + 4) & 0xfc;
        rect.w = (sint16)(rounded / 2);
        rect.x = (sint16)(d->page_x + d->u0 / 2);
    }
    else
        return;
    rect.y = (sint16)(d->page_y + d->v0);
    rect.h = (sint16)height;
    if (getenv("OA_PLAYER_UPLOAD_TRACE"))
    {
        static sint32 count;
        if (count < 128)
        {
            FILE *f = fopen("player_upload_port.log", count ? "a" : "w");
            if (f)
            {
                fprintf(f, "UPLOAD %d resource=%#x rect=%d,%d,%d,%d mode=%u frame=%ux%u\n", count, g_current_sprite_upload_resource, rect.x, rect.y, rect.w, rect.h, (uint32)d->pixel_mode, (uint32)frame[0], (uint32)frame[1]);
                fclose(f);
            }
            count++;
        }
    }
    LoadImage(&rect, (uint32 *)(frame + 4));
}

/* Original: SLES_004.74:FUN_8008D0C0 (0x8008D0C0..0x8008D0EC).
 * Full ten-bit descriptor index for explicit lookup (including screen sprites).
 * Not equivalent to the six-bit lookup in the world-sprite renderers. */
void *sprite_vram_descriptor_for_resource(uint32 resource_id)
{
    VRAM_SPRITE *base = descriptor_bases[resource_id >> 10];
    return base ? base + (resource_id & 0x3ff) : 0;
}

/* Inlined original lookup: SLES_004.74 8008D1FC..8008D220 and
 * 8008F5B4..8008F5D8. ANDI at 8008D204/8008F5BC is 0x3F, not 0x3FF.
 * Animation frame lookup still consumes the unmodified resource ID. */
static VRAM_SPRITE *sprite_render_descriptor(uint32 resource_id)
{
    VRAM_SPRITE *base = descriptor_bases[resource_id >> 10];
    return base ? base + (resource_id & 0x3f) : 0;
}

/* FUN_8009616C.  The omitted SsSetMVol calls are sound-only side effects;
 * the return value controlling frame upload is instruction-for-instruction
 * equivalent to the original branch. */
sint32 frame_render_work_enabled(void)
{
    const uint32 *timing;
    uint32 boundary;
    if (g_input_recording_mode != 2)
        return 1;
    timing = (const uint32 *)player_assets_executable_address(0x800c8e4cu);
    if (!timing)
        return 1;
    boundary = timing[(uint16)g_attract_demo_index * 2u];
    if (g_frame_counter < boundary)
        return 0;
    return 1;
}

/* Original: FUN_8008D108. */
GDB_CALL void render_world_sprite(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *prim, sint16 ot_bucket, sint32 split_mode, VRAM_SPRITE *descriptor, sint32 rotation)
{
    FrameObjectPartial *record = g_next_frame_object, *next = record;
    VRAM_SPRITE *d = descriptor;
    uint8 *frame;
    POLY_FT4 *p = prim;
    sint32 px, py, w, h, left, top, width_world, height_world, sort_left, split_width;
    sint32 depth, sx0, sx1, sy0, sy1;
    if (getenv("OA_SPRITE_CALL_TRACE"))
    {
        static sint32 count;
        if (count < 256)
        {
            FILE *f = fopen("sprite_calls_port.log", count ? "a" : "w");
            if (f)
            {
                fprintf(f, "CALL %d tick=%u resource=%08x xyz=%08x,%08x,%08x flip=%08x scale=%08x,%08x prim=%08x bucket=%d split=%d desc=%08x rot=%08x\n", count, g_frame_counter, resource, (uint32)x, (uint32)y, (uint32)z, (uint32)flip_or_velocity, (uint32)scale_x, (uint32)scale_y, (uint32)(uintptr_t)p, (sint32)ot_bucket, split_mode, (uint32)(uintptr_t)d, (uint32)rotation);
                fclose(f);
            }
            count++;
        }
    }
    g_current_sprite_upload_resource = resource;
    if (record < g_frame_objects || record >= g_frame_objects + 80)
        return;
    if (!p)
    {
        p = (POLY_FT4 *)(intptr)g_transient_prim_cursor;
        g_transient_prim_cursor += sizeof(*p);
        SetPolyFT4(p);
    }
    if (div_trunc_256(z) <= g_map_depth_cells * 0x80)
        return;
    if (!d)
        d = sprite_render_descriptor(resource);
    if (!d)
        return;
    frame = animation_frame_resource(resource);
    if (!frame)
        return;
    p->tpage = d->tpage;
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
        d->pivot_x = (sint8)frame[2];
        d->pivot_y = (sint8)frame[3];
        d->width_minus_one = (uint8)w;
        d->height = (uint8)h;
        if (flip_or_velocity > 0)
        {
            px = -(w + px);
            w = -w;
        }
        /* s4 retains the positive frame width while s5 is negated for flip. */
        if (frame_render_work_enabled() != 0)
            upload_dynamic_frame(d, frame, (uint8)(frame[0] - 1), h);
        d->cached_resource = resource;
    }
    left = div_trunc_4096(px * 0x100 * scale_x);
    if (scale_x == 0x1000)
        width_world = w << 8;
    else
        width_world = div_trunc_16(w * scale_x);
    top = div_trunc_4096(py * 0x100 * scale_y);
    if (scale_y == 0x1000)
        height_world = h << 8;
    else
        height_world = div_trunc_16(h * scale_y) - 0x100;
    x += left;
    y += top;
    if (width_world < 0)
        width_world = -width_world;
    sort_left = x;
    split_width = width_world;
    /* Do not clear the record: callers intentionally preset +0x24/+0x26/+0x29
     * immediately before D108 (lighting and force-light flags).  The original
     * writes only the fields below and clears +0x1c plus byte +0x28. */
    record->prim = p;
    record->world_x = x;
    record->world_y = y - top;
    record->world_z = z;
    record->sort_x = x;
    record->ot_bucket = ot_bucket;
    record->x_subcell_offset = 0;
    record->draw_env_height = 0;
    record->model_plus_one = 0;
    record->drawn = 0;
    p->clut = d->clut;
    if (flip_or_velocity < 1)
    {
        p->u0 = (uint8)d->u0;
        p->u1 = (uint8)(d->u0 + w);
    }
    else
    {
        p->u0 = (uint8)(d->u0 - w);
        p->u1 = (uint8)d->u0;
    }
    p->v0 = (uint8)d->v0;
    p->u2 = p->u0;
    p->v1 = (uint8)d->v0;
    p->v2 = (uint8)(d->v0 + h - 1);
    p->u3 = p->u1;
    p->v3 = p->v2;
    depth = z - g_camera_world_z;
    if (depth == 0)
        return;
    if (rotation == 0)
    {
        sx0 = (x - g_camera_world_x) * 0x140 / depth;
        sx1 = sx0 + width_world * 0x140 / depth;
        if (sx0 > g_screen_half_width || sx1 < -g_screen_half_width)
            return;
        sy0 = (y - g_camera_world_y) * 0x163 / depth;
        sy1 = (y + height_world - g_camera_world_y) * 0x163 / depth;
        if (sy0 > g_screen_height / 2 + 0x40 || sy1 < -0x40 - g_screen_height / 2)
            return;
        /* D108 coordinates are relative to DrawEnv.ofs=(160,64). */
        p->x0 = p->x2 = (sint16)(sx0 + 160);
        p->x1 = p->x3 = (sint16)(sx1 + 160);
        p->y0 = p->y1 = (sint16)(sy0 + 64);
        p->y2 = p->y3 = (sint16)(sy1 + 64);
    }
    else
    {
        const sint32 *trig = original_tables_mips_sine_table();
        sint32 cs, sn, cx, cy, hx = width_world / 2, hy = height_world / 2, i;
        sint32 lx[4] = {-1, 1, -1, 1}, ly[4] = {-1, -1, 1, 1};
        sint16 *xp[4] = {&p->x0, &p->x1, &p->x2, &p->x3};
        sint16 *yp[4] = {&p->y0, &p->y1, &p->y2, &p->y3};
        sint32 minx = 0x7fffffff, maxx = -0x7fffffff, sumx = 0, sumy = 0;
        if (!trig)
            return;
        cs = div_trunc_256(trig[(rotation + 0x100) & 0x3ff]);
        sn = div_trunc_256(trig[rotation & 0x3ff]);
        cx = x + hx;
        cy = y + hy;
        for (i = 0; i < 4; i++)
        {
            sint32 ox = lx[i] * hx, oy = ly[i] * hy;
            sint32 wx = cx + div_trunc_256(ox * cs - oy * sn);
            sint32 wy = cy + div_trunc_256(oy * cs + ox * sn);
            sint32 qx = (wx - g_camera_world_x) * 0x140 / depth;
            sint32 qy = (wy - g_camera_world_y) * 0x163 / depth;
            *xp[i] = (sint16)(qx + 160);
            *yp[i] = (sint16)(qy + 64);
            sumx += qx;
            sumy += qy;
            if (wx < minx)
                minx = wx;
            if (wx > maxx)
                maxx = wx;
        }
        if ((uint32)((sumx < 0 ? sumx + 3 : sumx) / 4 + 0x100) > 0x200u || (uint32)((sumy < 0 ? sumy + 3 : sumy) / 4 + 0x100) > 0x200u)
            return;
        record->sort_x = minx;
        sort_left = minx;
        split_width = maxx - minx;
    }
    next = record + 1;
    {
        sint32 start = 0x40 - (div_trunc_256(sort_left) & 0x3f);
        sint32 remaining = div_trunc_256(split_width), offset = 0;
        if (start < remaining && split_mode != 1 && div_trunc_256(width_world) > 0x17 && (split_mode != 0 || g_sprite_subdivision_cooldown == 0))
        {
            remaining -= start;
            record->draw_env_height = (sint16)start;
            offset = start;
            while (remaining > 0 && next < g_frame_objects + 80)
            {
                sint32 span = remaining > 0x40 ? 0x40 : remaining;
                *next = *record;
                next->x_subcell_offset = (sint16)offset;
                next->draw_env_height = (sint16)span;
                offset += span;
                remaining -= span;
                next++;
            }
        }
    }
    g_next_frame_object = next;
    if (getenv("OA_SPRITE_CALL_TRACE"))
    {
        FILE *f = fopen("sprite_calls_port.log", "a");
        if (f)
        {
            fprintf(f, "EMIT tick=%u resource=%08x records=%d\n", g_frame_counter, resource, (sint32)(next - record));
            fclose(f);
        }
    }
}

/* 8008E5D4..8008E7D8: screen-space SPRT plus its TPage DR_MODE packet. */
/* Original: FUN_8008E5D4. */
GDB_CALL SPRT *render_screen_sprite(sint16 x, sint16 y, uint32 resource, sint16 ot_bucket)
{
    VRAM_SPRITE *d = (VRAM_SPRITE *)sprite_vram_descriptor_for_resource(resource);
    SPRT *sprite;
    DR_MODE *mode;
    sint32 width, height;
    sprite = (SPRT *)g_gpu_packet_cursor;
    SetSprt(sprite);
    sprite->r0 = sprite->g0 = sprite->b0 = 0x80;
    width = d->width_minus_one;
    width = width ? width : 0x100;
    height = d->height;
    height = height ? height : 0x100;
    sprite->w = (sint16)width;
    sprite->h = (sint16)height;
    /* Preserve the original packet coordinates.  DRAWENV.ofs=(160,64) is a
     * GPU state, represented by native renderer metadata at submission. */
    sprite->x0 = (sint16)(x + d->pivot_x);
    sprite->y0 = (sint16)(y + 0x40 + d->pivot_y);
    sprite->u0 = (uint8)d->u0;
    sprite->v0 = (uint8)d->v0;
    sprite->clut = (uint16)(d->clut + (ot_bucket == 0 ? g_foreground_sprite_clut_offset : 0));
    if (getenv("OA_SCREEN_SPRITE_TRACE"))
    {
        static sint32 trace_count;
        if (trace_count < 512)
        {
            FILE *trace = fopen("screen_sprites_port.log", trace_count ? "a" : "w");
            if (trace)
            {
                fprintf(trace, "%d,resource=%#x,x=%d,y=%d,u=%u,v=%u,clut=%u,w=%u,h=%u,tpage=%u,ot=%d\n", trace_count, resource, (sint32)sprite->x0, (sint32)sprite->y0, (uint32)sprite->u0, (uint32)sprite->v0, (uint32)sprite->clut, (uint32)sprite->w, (uint32)sprite->h, (uint32)d->tpage, (sint32)ot_bucket);
                fclose(trace);
            }
            ++trace_count;
        }
    }
    psx_set_prim_screen_offset(160, 64);
    AddPrim(g_current_render_frame->ot + ot_bucket, sprite);
    g_gpu_packet_cursor += sizeof(SPRT);
    mode = (DR_MODE *)g_gpu_packet_cursor;
    memset(mode, 0, sizeof(*mode));
    mode->tag = 0x02000000;
    mode->code[0] = 0xe1000000u | (d->tpage & 0x9ffu);
    AddPrim(g_current_render_frame->ot + ot_bucket, mode);
    g_gpu_packet_cursor += sizeof(DR_MODE);
    return sprite;
}

/* Original: FUN_8008F54C. */
void render_world_sprite_immediate(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *prim, sint32 ot_bucket, sint32 unused_10, sint32 raw_descriptor, sint32 rotation)
{
    POLY_FT4 local, *q = prim;
    VRAM_SPRITE *d = sprite_render_descriptor(resource);
    uint8 *frame;
    sint32 w, h, signed_w, left, top, width_world, height_world, depth, sx0, sx1, sy0, sy1;
    (void)unused_10;
    if (!q)
    {
        memset(&local, 0, sizeof(local));
        SetPolyFT4(&local);
        local.r0 = local.g0 = local.b0 = 0x80;
        q = &local;
    }
    if (!d)
        d = (VRAM_SPRITE *)raw_descriptor;
    if (!d)
        return;
    frame = animation_frame_resource(resource);
    if (!frame)
        return;
    q->tpage = d->tpage;
    if (d->cached_resource == resource)
    { /* 8008F6F4 has the same cached-width -1 as D108. */
        w = (uint8)(d->width_minus_one - 1);
        h = d->height;
    }
    else
    {
        d->pivot_x = (sint8)frame[2];
        d->pivot_y = (sint8)frame[3];
        w = (uint8)(frame[0] - 1);
        h = frame[1];
        d->width_minus_one = (uint8)w;
        d->height = (uint8)h;
        upload_dynamic_frame(d, frame, w, h);
        d->cached_resource = resource;
    }
    signed_w = flip_or_velocity > 0 ? -w : w;
    left = div_trunc_4096((-w * 0x100 / 2) * scale_x);
    width_world = scale_x == 0x1000 ? signed_w << 8 : div_trunc_16(signed_w * scale_x);
    top = div_trunc_4096((-h * 0x100) * scale_y);
    height_world = scale_y == 0x1000 ? h << 8 : div_trunc_16(h * scale_y) - 0x100;
    y += top;
    if (width_world < 0)
        width_world = -width_world;
    q->clut = d->clut;
    q->u0 = (uint8)d->u0;
    q->v0 = (uint8)d->v0;
    q->u1 = (uint8)(d->u0 + signed_w);
    q->v1 = (uint8)d->v0;
    q->u2 = q->u0;
    q->v2 = (uint8)(d->v0 + h - 1);
    q->u3 = q->u1;
    q->v3 = q->v2;
    depth = z - g_camera_world_z;
    if (depth == 0)
        return;
    if (rotation == 0)
    {
        sx0 = ((x + left) - g_camera_world_x) * 0x140 / depth;
        sx1 = sx0 + width_world * 0x140 / depth;
        if (sx0 > g_screen_half_width || sx1 < -g_screen_half_width)
            return;
        sy0 = (y - g_camera_world_y) * 0x163 / depth;
        sy1 = (y + height_world - g_camera_world_y) * 0x163 / depth;
        if (sy0 > g_screen_height / 2 || sy1 < -(g_screen_height / 2))
            return;
        q->x0 = q->x2 = (sint16)(sx0 + 160);
        q->x1 = q->x3 = (sint16)(sx1 + 160);
        q->y0 = q->y1 = (sint16)(sy0 + 64);
        q->y2 = q->y3 = (sint16)(sy1 + 64);
    }
    else
    {
        const sint32 *trig = original_tables_mips_sine_table();
        sint32 cs, sn, cx256, cy256, ww = width_world / 0x100, hh = height_world / 0x100;
        sint32 hx = ww / 2, hy = hh / 2, lx[4] = {-1, 1, -1, 1}, ly[4] = {-1, -1, 1, 1};
        sint16 *xp[4], *yp[4];
        sint32 i, sumx = 0, sumy = 0;
        xp[0] = &q->x0;
        xp[1] = &q->x1;
        xp[2] = &q->x2;
        xp[3] = &q->x3;
        yp[0] = &q->y0;
        yp[1] = &q->y1;
        yp[2] = &q->y2;
        yp[3] = &q->y3;
        if (!trig)
            return;
        cs = div_trunc_256(trig[(rotation + 0x100) & 0x3ff]);
        sn = div_trunc_256(trig[rotation & 0x3ff]);
        cx256 = div_trunc_256(x + left + width_world / 2);
        cy256 = div_trunc_256(y + height_world / 2);
        for (i = 0; i < 4; i++)
        {
            sint32 ox = lx[i] * hx, oy = ly[i] * hy;
            sint32 wx = (cx256 + div_trunc_256(ox * cs - oy * sn)) * 0x100;
            sint32 wy = (cy256 + div_trunc_256(oy * cs + ox * sn)) * 0x100;
            sint32 qx = (wx - g_camera_world_x) * 0x140 / depth, qy = (wy - g_camera_world_y) * 0x163 / depth;
            *xp[i] = (sint16)(qx + 160);
            *yp[i] = (sint16)(qy + 64);
            sumx += qx;
            sumy += qy;
        }
        if ((uint32)((sumx < 0 ? sumx + 3 : sumx) / 4 + 0x100) > 0x200u || (uint32)((sumy < 0 ? sumy + 3 : sumy) / 4 + 0x100) > 0x200u)
            return;
    }
    if (!g_gpu_packet_cursor || !g_current_render_frame)
        return;
    memcpy(g_gpu_packet_cursor, q, sizeof(POLY_FT4));
    AddPrim(g_current_render_frame->ot + ot_bucket, g_gpu_packet_cursor);
    g_gpu_packet_cursor += sizeof(POLY_FT4);
}

void sprite_resource_reset(void)
{
    memset(descriptor_pool, 0, sizeof(descriptor_pool));
    memset(descriptor_bases, 0, sizeof(descriptor_bases));
    descriptor_next = descriptor_pool;
    /* Host relocation: PSX resource slot 0xBF initially points to 0x800709FC. */
    resource_table_register(0x2fc00, (void *)player_assets_executable_address(0x800709fcu));
}

/* Original: FUN_8008D0EC. */
void sprite_descriptor_table_register(uint32 resource_id, sint32 unused, void *base)
{
    (void)unused;
    descriptor_bases[resource_id >> 10] = (VRAM_SPRITE *)base;
}

sint32 sprite_resource_dump_audit(const char *path)
{
    FILE *file;
    VRAM_SPRITE *descriptor;
    sint32 index, count = (sint32)(descriptor_next - descriptor_pool);
    if (path == 0)
        return 0;
    file = fopen(path, "w");
    if (file == 0)
        return 0;
    fprintf(file, "index,resource,u0,u1,v0,v1,tpage,page_x,page_y,mode,clut,pivot_x,pivot_y,width_minus_one,height\n");
    for (index = 0; index < count; index++)
    {
        descriptor = &descriptor_pool[index];
        fprintf(file, "%d,%08X,%u,%u,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%u\n", index, descriptor->cached_resource, (uint32)descriptor->u0, (uint32)descriptor->u1, (uint32)descriptor->v0, (uint32)descriptor->v1, (uint32)descriptor->tpage, (uint32)descriptor->page_x, (uint32)descriptor->page_y, (uint32)descriptor->pixel_mode, (uint32)descriptor->clut, (sint32)descriptor->pivot_x, (sint32)descriptor->pivot_y, (uint32)descriptor->width_minus_one, (uint32)descriptor->height);
    }
    fclose(file);
    return 1;
}

/* Original: FUN_8008CFB0. */
void sprite_upload_pixels(void *raw_descriptor, uint8 *pixels, sint32 mode)
{
    VRAM_SPRITE *d = (VRAM_SPRITE *)raw_descriptor;
    PSX_RECT rect;
    sint32 width;
    if (!d || !pixels)
        return;
    d->pivot_x = (sint8)pixels[-2];
    d->pivot_y = (sint8)pixels[-1];
    d->width_minus_one = pixels[-4];
    d->height = pixels[-3];
    rect.x = (sint16)(d->page_x + (mode == 0 ? d->u0 / 4 : d->u0 / 2));
    rect.y = (sint16)(d->v0 + d->page_y);
    width = (sint32)d->u1 - (sint32)d->u0 + 1;
    rect.w = (sint16)(mode == 0 ? (width < 0 ? (width + 3) / 4 : width / 4) : width / 2);
    rect.h = (sint16)((sint32)d->v1 - (sint32)d->v0 + 1);
    if (getenv("OA_PLAYER_UPLOAD_TRACE") && (g_current_sprite_upload_resource >> 10) == 0x30)
    {
        static sint32 trace_count;
        FILE *trace = fopen("player_upload_port.log", trace_count ? "a" : "w");
        if (trace)
        {
            fprintf(trace, "UPLOAD %d resource=%#x rect=%d,%d,%d,%d mode=%d tpage=%#x clut=%#x uv=%u,%u,%u,%u\n", trace_count, g_current_sprite_upload_resource, rect.x, rect.y, rect.w, rect.h, mode, d->tpage, d->clut, d->u0, d->u1, d->v0, d->v1);
            fclose(trace);
        }
        trace_count++;
    }
    LoadImage(&rect, (uint32 *)pixels);
    DrawSync(0);
}

/* Original: FUN_8008CE24. */
GDB_CALL void *sprite_create_vram_descriptors(uint32 resource_id, sint32 frame_count, uint16 clut, sint16 abr)
{
    VRAM_SPRITE *first = descriptor_next;
    uint16 clut_width = sprite_clut_entry_count(resource_id);
    sint32 mode = (clut_width == 0x100);
    sint32 alignment_mask = (clut_width == 0x10) ? 7 : (mode ? 3 : 0);
    sint32 i;
    sprite_descriptor_table_register(resource_id, 1, first);
    for (i = 0; i < frame_count; i++)
    {
        VRAM_SPRITE *d = descriptor_next;
        uint8 *frame = animation_frame_resource(resource_id);
        uint32 width, height;
        if (!frame || d >= descriptor_pool + 190)
            break;
        width = frame[0] ? frame[0] : 0x100;
        height = frame[1] ? frame[1] : 0x100;
        sprite_vram_allocate(d, (sint32)((width + alignment_mask) & ~alignment_mask), (sint32)height, mode);
        /* 8008CF04 passes descriptor +0x22,+0x1e,+0x20.  The local UV
         * coordinates are not VRAM page coordinates. */
        d->tpage = GetTPage(d->pixel_mode, abr, d->page_x, d->page_y);
        sprite_upload_pixels(d, frame + 4, mode);
        d->cached_resource = resource_id;
        d->clut = clut;
        descriptor_next++;
        resource_id++;
    }
    return first;
}

/* Original: FUN_8008CD98. */
void sprite_assign_clut_range(uint32 resource_id, sint32 frame_count, uint16 clut)
{
    while (frame_count > 0)
    {
        VRAM_SPRITE *descriptor = (VRAM_SPRITE *)sprite_vram_descriptor_for_resource(resource_id);
        if (descriptor != 0)
            descriptor->clut = clut;
        g_resource_clut_table[resource_id >> 10] = clut;
        --frame_count;
        ++resource_id;
    }
}

/* Original: FUN_800963E0. */
GDB_CALL uint32 sprite_clut_upload(uint32 resource_id, sint32 unused_2, sint32 unused_3, sint32 semi_transparent)
{
    uint16 converted[256], count = sprite_clut_entry_count(resource_id), x;
    uint8 *frame = animation_frame_resource(resource_id), *source;
    PSX_RECT rect;
    sint32 i;
    (void)unused_2;
    (void)unused_3;
    if (!frame || count == 0 || count > 256)
        return 0;
    if (count * 2 == 0x20)
    {
        if (g_clut_upload_x == 0x240)
        {
            g_clut_upload_x = 0x140;
            g_clut_upload_y++;
        }
    }
    else if (g_clut_upload_x >= 0x141)
    {
        g_clut_upload_x = 0x140;
        g_clut_upload_y++;
    }
    source = frame - count * 2;
    g_shared_scratch_value = 0;
    for (i = 0; i < count; i++)
    {
        uint32 color = (uint32)source[i * 2] | (uint32)source[i * 2 + 1] << 8;
        uint16 out = (uint16)color;
        if (g_attract_mode != 0)
        {
            uint32 grey = (((color >> 10) & 31) + ((color >> 5) & 31) + (color & 31)) / 3;
            out = (uint16)((((grey * 2) / 3) << 10) + (grey << 5) + grey);
            if ((out & 0x7fff) == 0 && g_shared_scratch_value != 0)
                out = 1;
        }
        if (semi_transparent && g_shared_scratch_value != 0)
            out |= 0x8000;
        converted[i] = out;
        g_shared_scratch_value++;
    }
    rect.x = g_clut_upload_x;
    rect.y = g_clut_upload_y;
    rect.w = (sint16)count;
    rect.h = 1;
    if (getenv("OA_CLUT_TRACE"))
    {
        static sint32 clut_trace_count;
        FILE *trace = fopen("clut_upload_port.log", clut_trace_count ? "a" : "w");
        if (trace)
        {
            fprintf(trace, "CLUT %d resource=%#x rect=%d,%d,%d,1 semi=%d\n", clut_trace_count, resource_id, rect.x, rect.y, count, semi_transparent);
            fclose(trace);
        }
        clut_trace_count++;
    }
    LoadImage(&rect, (uint32 *)converted);
    DrawSync(0);
    x = (uint16)g_clut_upload_x;
    g_clut_upload_x = (sint16)(g_clut_upload_x + count);
    return ((uint32)(uint16)g_clut_upload_y * 0x40u + ((uint32)x >> 4)) & 0xffffu;
}

/* Direct 0x8008ECDC..0x8008ED30 translation. */
/* Original: FUN_8008ECDC. */
GDB_CALL void font_render_state_initialize(void)
{
    g_font_glyph_width = 0xc;
    g_font_glyph_height = 0xc;
    g_font_line_height = 0xd;
    g_font = (FONT *)sprite_vram_descriptor_for_resource(0x2fc00);
    g_font_clut = (sint16)g_font_uploaded_clut;
    g_font_glyph_columns = 0x100 / 0xc;
}

/* Direct 0x8008BE98..0x8008BEDC translation. */
/* Original: FUN_8008BE98. */
GDB_CALL void font_sprite_initialize(void)
{
    g_font_uploaded_clut = (uint16)sprite_clut_upload(0x2fc00, 0x140, 0x30, 0);
    sprite_create_vram_descriptors(0x2fc00, 1, g_font_uploaded_clut, 0);
    font_render_state_initialize();
}

/* Original: FUN_8008E7DC. */
void render_background_strips(uint16 x, sint32 y, sint32 rows, sint8 u_offset)
{
    while (rows-- > 0)
    {
        sint32 column;
        y += 0x40;
        for (column = 0; column < 6; column++)
        {
            SPRT *p = render_screen_sprite((sint16)((x & 0x3f) - 0xe0 + column * 0x3f), (sint16)y, 0x30400, 0x480);
            sint32 shade = g_background_brightness_bias + 0x80;
            if (shade < 0)
                shade = 0;
            p->r0 = p->g0 = p->b0 = (uint8)shade;
            p->w = 0x40;
            p->h = 0x40;
            /* 0x8008E880 stores s2 directly after FUN_8008E5D4, replacing
             * that helper's internal +0x40.  DRAWENV.ofs.y=64 is still
             * applied later by the GPU. */
            p->y0 = (sint16)y;
            p->u0 = (uint8)(p->u0 + u_offset);
        }
    }
}

/* Original: FUN_8008E8D0. */
void render_stage_background(void)
{
    const uint8 *resource_address = (const uint8 *)player_assets_executable_address(0x800cd508u + (uint32)g_stage_index * 0x24u);
    uint32 resource = resource_address ? read_u32_le(resource_address) : 0;
    sint32 x, y, top, rows, column;
    if (!resource)
        return;
    x = (-g_camera_world_x / 6) / 0x100;
    if (g_stage_index == 3)
        x += (uint8)g_frame_counter;
    x &= 0xff;
    if (g_previous_camera_world_z != -1)
        g_stage_background_y -= (g_camera_world_z - g_previous_camera_world_z) / 0x18 + (g_camera_world_y - g_previous_camera_world_y) / 7;
    y = g_stage_background_y / 0x100;
    top = (g_vertical_cull_extent + 0x100) - g_screen_height;
    if ((uint32)y < (uint32)-top)
    {
        sint32 distance = top - 0x100 - y;
        if (distance < 0)
            distance = -distance;
        render_background_strips((uint16)x, y + 0x100, (sint16)distance / 0x40 + 1, 0x40);
    }
    if (-(g_screen_height / 2) < y)
    {
        sint32 distance = y + g_vertical_cull_extent;
        if (distance < 0)
            distance = -distance;
        rows = (sint16)distance / 0x40 + 1;
        render_background_strips((uint16)x, y - rows * 0x40, rows, 0);
    }
    for (column = 0; column < 3; column++)
    {
        SPRT *p = render_screen_sprite((sint16)((sint16)x - 0x1a0 + column * 0x100), (sint16)y, resource, 0x480);
        sint32 shade = g_background_brightness_bias + 0x80;
        if (shade < 0)
            shade = 0;
        p->h = 0x100;
        p->r0 = p->g0 = p->b0 = (uint8)shade;
    }
}

/* Direct translation of FUN_800A68E8.  CLUT allocation follows in each
 * original caller and therefore deliberately remains outside this helper. */
void sprite_archive_resource_load(const char *name, uint32 resource, sint32 frames, sint32 mode, void *archive)
{
    uint8 *data = (uint8 *)archive_member_find(name, archive);

    if (data == 0)
        fatal_error(name);
    if (*(sint32 *)data < frames)
        fatal_error("RESOURCE FRAME COUNT");
    resource_table_register(resource, data);
    sprite_create_vram_descriptors(resource, frames, 0, (sint16)mode);
}

/* FUN_8008CD54: the first halfword following the frame-offset table is the
 * CLUT width (16 or 256 entries). */
uint16 sprite_clut_entry_count(uint32 resource_id)
{
    uint8 *base = resource_table_data(resource_id);
    if (base == NULL)
        return 0;
    return *(uint16 *)(base + ((uint32)base[0] + 1u) * 4u);
}

/* Direct translation of FUN_8008C814 using the Windows deferred-resource
 * array in place of the PSX address stored at gp+0x408. */
void sprite_vram_defer_release(void *resource)
{
    if (deferred_resource_count >= 256)
        abort();
    deferred_resources[deferred_resource_count++] = (VRAM_SPRITE *)resource;
    deferred_resources[deferred_resource_count] = 0;
}

/* Native ownership seam: preserves the two writes at heap initialization. */
void sprite_vram_reset_deferred(void)
{
    deferred_resource_count = 0;
    deferred_resources[0] = 0;
}

/* Direct game-side translation of 0x8008C838..0x8008C8B0.  The queue backing
 * replaces gp+0x408/0x800EDBF0 only; each descriptor is still unlinked by the
 * original FUN_8008BF8C call and its +0x10 anchor is then cleared. */
/* Original: FUN_8008C838. */
void sprite_vram_release_deferred(void)
{
    sint32 index;
    for (index = 0; index < deferred_resource_count; index++)
    {
        VRAM_SPRITE *descriptor = deferred_resources[index];
        uint32 anchor;
        if (descriptor == 0)
            continue;
        anchor = descriptor->region_token;
        if (anchor != 0)
            linked_list_unlink((void *)(intptr)anchor, descriptor);
        descriptor->region_token = 0;
    }
    deferred_resource_count = 0;
    deferred_resources[0] = 0;
}

/* Original: FUN_800B30F4. */
static void vram_gpu_draw_sync(sint32 mode)
{
    DrawSync(mode);
}

/* Original: FUN_8008CC88. */
static sint32 sprite_vram_rect_overlaps(sint32 x, sint32 y, sint32 w, sint32 h, VRAM_SPRITE *d)
{
    if ((sint32)d->u1 < x)
        return 0;
    if (x + w < (sint32)d->u0)
        return 0;
    if ((sint32)d->v1 < y)
        return 0;
    return !(y + h < (sint32)d->v0);
}

/* Original: FUN_8008C8B8. */
GDB_CALL void sprite_vram_reset(uint16 *env)
{
    const uint16 *s = env;
    sint32 i = 0;
    vram_alloc_reset_storage();
    /* 8008C8D8..8008C938 clears all 190 descriptors and resets gp+0x1dc. */
    sprite_resource_reset();
    g_clut_upload_x = 0x140;
    g_clut_upload_y = 0;
    for (i = 0; i < 32; i++)
        linked_list_initialize(vram_alloc_region(i));
    i = 0;
    while (s[0] != 0xffff && i < 32)
    {
        VramRegion *r = vram_alloc_region(i++);
        r->x = s[0];
        r->y = s[1];
        r->width = s[2];
        r->height = s[3];
        r->mode = s[4];
        r->tpage = (uint16)LoadTPage(0, r->mode, 0, r->x, r->y, 0, 0);
        vram_gpu_draw_sync(0);
        s += 5;
    }
    vram_alloc_set_region_count(i);
}

/* Original: FUN_8008CA48. */
GDB_CALL VRAM_SPRITE *sprite_vram_allocate(VRAM_SPRITE *d, sint32 w, sint32 h, sint32 mode)
{
    sint32 ri, candidate_seen = 0;
    for (ri = 0; ri < vram_alloc_region_count(); ri++)
    {
        VramRegion *r = vram_alloc_region(ri);
        sint32 x = 0, y = 0x100 - (sint32)r->height;
        if (r->mode != mode || y > 0x100 - h)
            continue;
        candidate_seen = 1;
        for (;;)
        {
            VRAM_SPRITE *o = (VRAM_SPRITE *)r->head;
            while (o)
            {
                if (sprite_vram_rect_overlaps(x, y, w, h, o))
                {
                    x = (sint32)o->u1 + 1;
                    if (x > 0x100 - w)
                    {
                        y += 0x10;
                        x = 0;
                        if (y > 0x100 - h)
                            break;
                    }
                    o = r->head;
                }
                else
                {
                    o = vram_alloc_next_descriptor(o);
                }
            }
            if (!o && y <= 0x100 - h)
            {
                linked_list_append(r, d);
                d->region_token = (uint32)(ri + 1);
                d->u0 = (uint16)x;
                d->u1 = (uint16)(x + w - 1);
                d->v0 = (uint16)y;
                d->v1 = (uint16)(y + h - 1);
                d->tpage = r->tpage;
                d->page_x = r->x;
                d->page_y = r->y;
                d->pixel_mode = (uint16)mode;
                return d;
            }
            if (y > 0x100 - h)
                break;
        }
    }
    if (g_vram_error_reporting_enabled != 0)
        fatal_error_with_value(candidate_seen ? "OUT OF VRAM2 " : "OUT OF VRAM1 ", mode);
    return 0;
}
