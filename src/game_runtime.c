#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_config.h"
#include "text_renderer.h"
#include "map.h"
#include "psx_gpu.h"
#include "spatial.h"
#include "cc_archive.h"
#include "code_module.h"
#include "game_runtime.h"
#include "game_loop.h"
#include "map_render.h"
#include "camera.h"
#include "input.h"
#include "level_data.h"
#include "model.h"
#include "object.h"
#include "game_file.h"
#include "player.h"
#include "psx.h"
#include "psx_spu.h"
#include "render.h"
#include "resource_table.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "video.h"

/* Zero selects the existing shared native GPU and CD owners */
const uint32 xport_gpu_graph_type_address = 0u;
const uint32 xport_cd_ready_callback_address = 0u;
const uint32 xport_cd_sync_callback_address = 0u;
const uint32 xport_cd_status_address = 0u;
const uint32 xport_cd_setloc_table_address = 0u;

/* Types */
typedef char GAME_RUNTIME_POLY_FT3_SIZE_20[sizeof(POLY_FT3) == 0x20 ? 1 : -1];
typedef char GAME_RUNTIME_POLY_FT4_SIZE_28[sizeof(POLY_FT4) == 0x28 ? 1 : -1];
typedef char GAME_RUNTIME_POLY_FT4_CLUT_0E[(offsetof(POLY_FT4, p) + offsetof(POINT_T, attr)) == 0x0e ? 1 : -1];
typedef char GAME_RUNTIME_POLY_FT4_TPAGE_16[(offsetof(POLY_FT4, p) + sizeof(POINT_T) + offsetof(POINT_T, attr)) == 0x16 ? 1 : -1];

/* Macros. */
#define FW 320

#define FH 256

#define VRAM_W 1024

#define VRAM_H 512

/* Variables. */

static sint32 disp_offset_y;

static sint32 prim_world_depth = -1;

static sint32 prim_ot_bucket = -1;

/* Clear packet depth state at the stage heap boundary */
void game_runtime_heap_reset(void)
{
    prim_world_depth = prim_ot_bucket = -1;
}

/* Original: FUN_8008FF98. */
GDB_CALL sint32 tim_image_upload(const void *raw, uint32 size)
{
    const uint8 *data = (const uint8 *)raw;
    GpuTimImage image;
    PSX_RECT rect;
    if (data == 0 || size < 8 || xport_load_le32(data) != 0x10 || !gpu_decode_tim_body(data + 4, size - 4, &image))
        return 0;
    if (image.clut_data != 0)
    {
        uint32 offset = image.clut_data - data;
        /* Original LoadClut reads256 entries,including any following TIM bytes */
        if (offset > size || size - offset < 256 * sizeof(uint16))
            return 0;
    }
    rect = image.pixels;
    rect.x = g_render_frames[0].drawenv.clip.x;
    rect.y = g_render_frames[0].drawenv.clip.y;
    LoadImagePSX(&rect, (uint32 *)image.pixel_data);
    DrawSync(0);
    rect.x = g_render_frames[1].drawenv.clip.x;
    rect.y = g_render_frames[1].drawenv.clip.y;
    LoadImagePSX(&rect, (uint32 *)image.pixel_data);
    DrawSync(0);
    if (image.clut_data != 0)
    {
        LoadClut((uint32 *)image.clut_data, image.clut.x, image.clut.y);
        DrawSync(0);
    }
    return 1;
}

/* Original: SLES_004.74:FUN_8008B7A8. */
GDB_CALL void str_video_play(const char *str_path, sint32 height, sint32 frames, sint32 mode)
{
    /* Movies run outside the game sound session */
    SpuInit();
    if (video_play_str(str_path, height, frames) < 0)
    {
        fatal_error("CANT PLAY VIDEO");
        return;
    }
    vram_clear();
    if (mode == 2 || mode == 3)
    {
        runtime_heap_initialize();
        linked_list_initialize(&g_object_list);
        disp_buffers_initialize();
        SetDispMask(1);
        spr_vram_reset(&g_game_vram_layout);
        text_font_init();
        loading_caption_show(0);
        overbins_load();
    }
}

void psx_set_prim_depth(sint32 world_z)
{
    prim_world_depth = world_z;
}

void psx_set_prim_ot_bucket(sint32 bucket)
{
    prim_ot_bucket = bucket;
}

static uint32 *prim_ot_take(RENDER_FRAME *target_frame, sint32 fallback_bucket)
{
    sint32 bucket = prim_ot_bucket;
    XASSERT(!(target_frame == 0));
    if (bucket < 0 && prim_world_depth >= 0)
        bucket = (prim_world_depth - g_camera.world_z) >> 8;
    if (bucket < 0 || bucket >= RENDER_OT_LENGTH)
        bucket = fallback_bucket;
    prim_world_depth = -1;
    prim_ot_bucket = -1;
    return target_frame->ot + bucket;
}

void psx_begin_frame(void)
{
    gpu_frame_begin();
    prim_world_depth = -1;
    prim_ot_bucket = -1;
}

void psx_set_disp_offset_y(sint32 offset)
{
    disp_offset_y = offset;
}

void psx_end_frame(void)
{
    gpu_frame_end();
    if (xport_isquit())
        return;
    gpu_display_offset(0, disp_offset_y);
    gpu_present();
}

void psx_add_draw_area_rect(void *ot, sint32 x0, sint32 x1, sint32 y0, sint32 y1)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    DRAWENV environment;
    DR_ENV *packet;
    if (!g_cur_render_frame)
        return;
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(*packet) > RENDER_PRIM_CAPACITY - arena->used));
    environment = g_cur_render_frame->drawenv;
    environment.clip.x = (environment.clip.x + x0);
    environment.clip.y = (environment.clip.y + y0);
    environment.clip.w = (x1 - x0);
    environment.clip.h = (y1 - y0);
    environment.isbg = 0;
    packet = (DR_ENV *)(arena->data.bytes + arena->used);
    memset(packet, 0, sizeof(*packet));
    SetDrawEnv(packet, &environment);
    AddPrim(ot, packet);
    arena->used += sizeof(*packet);
}

/* 0x80090218..0x80090294.  The second clear is not redundant on PSX: it
 * explicitly covers the final VRAM scanline after the full 1024x512 clear. */
/* Original: FUN_80090218. */
GDB_CALL void vram_clear(void)
{
    PSX_RECT rect;

    DrawSync(0);
    rect.x = 0;
    rect.y = 0;
    rect.w = 0x400;
    rect.h = 0x200;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
    rect.y = 0x1ff;
    rect.h = 1;
    ClearImage(&rect, 0, 0, 0);
    DrawSync(0);
}

/* Original: FUN_80090298. */
GDB_CALL void scr_overlay_draw(void)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    POLY_G4 *poly;
    DR_ENV *packet;
    DRAWENV *environment = &g_cur_render_frame->drawenv;
    uint32 required = sizeof(*poly) + 2 * sizeof(*packet);
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || required > RENDER_PRIM_CAPACITY - arena->used));
    poly = (POLY_G4 *)(arena->data.bytes + arena->used);
    memset(poly, 0, sizeof(*poly));
    setPolyG4(poly);
    poly->p[0].rgbc.b = poly->p[1].rgbc.b = 0x78;
    poly->p[0].xy.x = poly->p[2].xy.x = -g_render_state.half_width;
    poly->p[1].xy.x = poly->p[3].xy.x = g_render_state.half_width;
    poly->p[0].xy.y = poly->p[1].xy.y = (0x40 - g_map_render.vertical_extent);
    poly->p[2].xy.y = poly->p[3].xy.y = (0x40 + g_map_render.vertical_extent);
    arena->used += sizeof(*poly);

    packet = (DR_ENV *)(arena->data.bytes + arena->used);
    memset(packet, 0, sizeof(*packet));
    SetDrawEnv(packet, environment);
    AddPrim(g_cur_render_frame->ot + 1, packet);
    arena->used += sizeof(*packet);
    AddPrim(g_cur_render_frame->ot + 1, poly);
    environment->isbg = 1;
    packet = (DR_ENV *)(arena->data.bytes + arena->used);
    memset(packet, 0, sizeof(*packet));
    SetDrawEnv(packet, environment);
    AddPrim(g_cur_render_frame->ot + 1, packet);
    arena->used += sizeof(*packet);
    environment->isbg = 0;
}

void psx_submit_prepared_model_ft3(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, const MODEL_PACKET *src, sint32 shade, uint16 clut_override, RENDER_FRAME *target_frame)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    MODEL_PACKET *copy;
    POLY_FT3 *poly;
    if (src == 0)
        return;
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(MODEL_PACKET) > RENDER_PRIM_CAPACITY - arena->used));
    copy = (MODEL_PACKET *)(arena->data.bytes + arena->used);
    poly = &copy->triangle;
    arena->used += sizeof(MODEL_PACKET);
    memcpy(copy, src, sizeof(*copy));
    poly->rgbc.r = poly->rgbc.g = poly->rgbc.b = shade;
    poly->p[0].xy.x = x0;
    poly->p[0].xy.y = y0;
    poly->p[1].xy.x = x1;
    poly->p[1].xy.y = y1;
    poly->p[2].xy.x = x2;
    poly->p[2].xy.y = y2;
    if (clut_override)
        poly->p[0].attr = clut_override;
    AddPrim(prim_ot_take(target_frame, 2), poly);
}

void psx_submit_prepared_model_ft4(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, const MODEL_PACKET *src, sint32 shade, uint16 clut_override, RENDER_FRAME *target_frame)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    MODEL_PACKET *copy;
    POLY_FT4 *poly;
    if (src == 0)
        return;
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(MODEL_PACKET) > RENDER_PRIM_CAPACITY - arena->used));
    copy = (MODEL_PACKET *)(arena->data.bytes + arena->used);
    poly = &copy->quad;
    arena->used += sizeof(MODEL_PACKET);
    memcpy(copy, src, sizeof(*copy));
    poly->rgbc.r = poly->rgbc.g = poly->rgbc.b = shade;
    poly->p[0].xy.x = x0;
    poly->p[0].xy.y = y0;
    poly->p[1].xy.x = x1;
    poly->p[1].xy.y = y1;
    poly->p[2].xy.x = x2;
    poly->p[2].xy.y = y2;
    poly->p[3].xy.x = x3;
    poly->p[3].xy.y = y3;
    if (clut_override)
        poly->p[0].attr = clut_override;
    AddPrim(prim_ot_take(target_frame, 2), poly);
}

/* Original: SLES_004.74:FUN_8008DDEC. */
void frame_model_submit(FRAME_OBJECT *o)
{
    sint32 row_bucket = g_model_draw.depth_bucket;
    if (!o)
        return;
    o->ot_bucket = ot_depth_inherit_row_bucket(o->ot_bucket, row_bucket);
    if (o->model_plus_one)
    {
        /* FUN_80094548 writes the +0x26/+0x29 light result only to the
         * primitive referenced by FrameObject+0.  FUN_8008DDEC's model path
         * (+0x1C != 0) never feeds that result back to gp+0x2A4, so the
         * model inherits the current cell/model-layer shade unchanged. */
        model_draw_runtime_model(o->model_plus_one - 1, o->world_x, o->world_y, o->world_z, o->model_rot_y, o->model_rot_x, o->model_rot_z, row_bucket);
    }
    else
    {
        sint32 object_shade;
        RENDER_PACKET_ARENA *arena = g_gpu_packets;
        POLY_FT4 *poly;
        uint32 packet_bytes = sizeof(*poly);
        uint32 *ot;
        sint32 clipped = o->clip_width != 0;
        uint16 saved_clut;
        XASSERT(!(o->prim == 0 || g_cur_render_frame == 0 || o->ot_bucket < 0 || o->ot_bucket >= RENDER_OT_LENGTH));
        ot = g_cur_render_frame->ot + o->ot_bucket;
        saved_clut = o->prim->p[0].attr;
        if (clipped)
            packet_bytes += 2 * sizeof(DR_ENV);
        if (o->light_delta == 0 && !o->force_light)
            object_shade = g_map_render.object_shade;
        else
            object_shade = o->light_delta + (o->force_light ? 0x80 : g_map_render.object_shade);
        if (object_shade < 0)
            object_shade = 0;
        if (object_shade > 255)
            object_shade = 255;
        o->prim->rgbc.r = o->prim->rgbc.g = o->prim->rgbc.b = object_shade;
        /* FUN_80094548 writes the current cell light into the source packet
           and FUN_8008DDEC copies that packet into the primitive arena. */
        XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || packet_bytes > RENDER_PRIM_CAPACITY - arena->used));
        psx_set_prim_ot_bucket(o->ot_bucket);
        if (clipped)
        {
            DR_ENV *restore = (DR_ENV *)(arena->data.bytes + arena->used);
            /* The original stores the Q8 span back through signed16 */
            o->clip_width *= 256;
            memset(restore, 0, sizeof(*restore));
            SetDrawEnv(restore, &g_cur_render_frame->drawenv);
            AddPrim(ot, restore);
            arena->used += sizeof(*restore);
        }
        poly = (POLY_FT4 *)(arena->data.bytes + arena->used);
        memcpy(poly, o->prim, sizeof(*poly));
        arena->used += sizeof(*poly);
        AddPrim(ot, poly);
        if (clipped)
        {
            DRAWENV *environment = &g_cur_render_frame->drawenv;
            DR_ENV *clip = (DR_ENV *)(arena->data.bytes + arena->used);
            sint32 depth = (uint32)o->world_z - g_camera.world_z;
            uint32 wx = o->sort_x + ((uint32)o->cell_x_offset << 8);
            sint32 numerator = (wx - g_camera.world_x) * 320u;
            sint32 end_x;
            if (depth == 0 || (depth == -1 && numerator == (-2147483647 - 1)))
            {
#if defined(_DEBUG)
                fputs("FRAME CLIP PROJECTION BREAK 7/6\n", stderr);
#endif
                abort();
            }
            environment->clip.x = ((uint32)(numerator / depth) + g_render_state.half_width);
            o->prim->p[0].attr = saved_clut;
            numerator = (o->clip_width * 320);
            if (depth == -1 && numerator == (-2147483647 - 1))
            {
#if defined(_DEBUG)
                fputs("FRAME CLIP PROJECTION BREAK 6\n", stderr);
#endif
                abort();
            }
            environment->clip.w = (numerator / depth);
            numerator = ((wx + o->clip_width - g_camera.world_x) * 320u);
            if (depth == -1 && numerator == (-2147483647 - 1))
            {
#if defined(_DEBUG)
                fputs("FRAME CLIP PROJECTION BREAK 6\n", stderr);
#endif
                abort();
            }
            end_x = numerator / depth;
            environment->clip.w = (end_x - ((uint32)environment->clip.x - g_render_state.half_width));
            if ((environment->clip.x + environment->clip.w) > g_render_state.width)
                environment->clip.w = ((uint32)g_render_state.width - environment->clip.x);
            memset(clip, 0, sizeof(*clip));
            SetDrawEnv(clip, environment);
            AddPrim(ot, clip);
            arena->used += sizeof(*clip);
            environment->clip.x = 0;
            environment->clip.w = g_render_state.width;
        }
        else
            o->prim->p[0].attr = saved_clut;
    }
    o->force_light = 0;
    o->light_delta = 0;
}
