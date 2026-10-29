#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_math.h"
#include "native_config.h"
#include "game.h"
#include "map_render.h"
#include "camera.h"
#include "spatial.h"
#include "map.h"
#include "model.h"
#include "game_file.h"
#include "runtime_heap.h"
#include "object.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "render.h"
#include "sprite.h"

/* Originals: DAT_800D3EF0, DAT_800D3F0C, DAT_800D404C, DAT_800D40BC, DAT_800D41F0 */
MODEL_SELECTIONS g_model_selections;

/* Types. */
/* Direct HQ translation of FUN_80092D50 and the fixed-bucket model renderer
 * FUN_800914D8.  FUN_800924EC is a separate hierarchical-object path. */

typedef struct MODEL_VERTEX
{
    sint16 x, y, z;
} MODEL_VERTEX;

/* Mutable serialized polygons retain original resource aliasing and byte order */
typedef struct
{
    uint8 u[2], v[2];
} MODEL_UV_RECORD;

typedef struct MODEL_POLYGON_RECORD
{
    uint8 material_or_packet[2];
    uint8 shade_flags[2];
    uint8 vertex_offsets[4][2];
    MODEL_UV_RECORD uv[4];
} MODEL_POLYGON_RECORD;

typedef char MODEL_POLYGON_RECORD_SIZE[sizeof(MODEL_POLYGON_RECORD) == 28 ? 1 : -1];
typedef char MODEL_POLYGON_RECORD_VERTICES[offsetof(MODEL_POLYGON_RECORD, vertex_offsets) == 4 ? 1 : -1];
typedef char MODEL_POLYGON_RECORD_UV[offsetof(MODEL_POLYGON_RECORD, uv) == 12 ? 1 : -1];

struct MODEL
{
    sint16 vertex_count;
    MODEL_VERTEX *vertices;
    sint16 polygon_count;
    MODEL_POLYGON_RECORD *polygons;
    sint32 desc_half_x, desc_half_y, desc_half_z;
};

typedef struct CELL_RENDER_ITEM
{
    sint32 special, drawn, z, y;
    FRAME_OBJECT *object;
    const MAP_MODEL_INSTANCE *instance;
} CELL_RENDER_ITEM;

typedef struct DEFERRED_CELL_MODEL
{
    const MAP_MODEL_INSTANCE *instance;
    sint32 item_count;
    sint32 shade;
    sint32 *item;
    sint32 item_capacity;
} DEFERRED_CELL_MODEL;

typedef struct
{
    CELL_RENDER_ITEM *items;
    DEFERRED_CELL_MODEL *deferred;
    sint32 item_capacity, deferred_capacity;
} CELL_RENDER_WORK;

/* Cell sorting borrows map/object records only during the current draw */
static CELL_RENDER_WORK cell_render_work;

/* -------------------------------------------------------------------------
 * Hierarchical model path: FUN_800920A0/FUN_80092194/FUN_800923A4/
 * FUN_8009242C/FUN_800924EC.  This path is deliberately separate from
 * FUN_800914D8 above: the original either inserts into the supplied main OT,
 * or sorts by maximum transformed Z in a private 0x200-entry OT before
 * splicing that chain into the caller's single main-OT bucket. */

typedef struct HIERARCHY_POLYGON
{
    const MODEL_POLYGON_RECORD *polygon;
    XY scr[4];
    sint32 depth;
    uint32 sequence;
    sint32 triangle;
} HIERARCHY_POLYGON;

/* Variables. */
/* Original descriptor base: DAT_800D4010 */
MODEL_CATALOG g_model_catalog;
MODEL_DRAW_STATE g_model_draw;
/* Original: DAT_800D4078. */
sint16 g_model_count;
/* Original: DAT_800D3FDC. */
sint16 g_model_packet_count;

static MODEL_PACKET model_packet_templates[0x226];

static sint32 model_cell_light_offset;

static const MAP_FLOOR_CELL *model_cell;

static HIERARCHY_POLYGON *hierarchy_polygon;
static uint32 hierarchy_polygon_capacity;

static uint32 hierarchy_polygon_count;

/* Native vertices are owned; polygon codecs still borrow current stage bytes */
void model_heap_reset(void)
{
    sint32 i;
    for (i = 0; i < cell_render_work.deferred_capacity; ++i)
        free(cell_render_work.deferred[i].item);
    free(cell_render_work.items);
    free(cell_render_work.deferred);
    memset(&cell_render_work, 0, sizeof(cell_render_work));
    for (i = 0; i < g_model_catalog.capacity; ++i)
        free(g_model_catalog.recs[i].vertices);
    free(g_model_catalog.recs);
    free(g_model_catalog.descs);
    g_model_catalog.recs = 0;
    g_model_catalog.descs = 0;
    g_model_catalog.count = g_model_catalog.capacity = 0;
    g_model_catalog.src = 0;
    g_model_catalog.src_size = 0;
    model_cell = 0;
    hierarchy_polygon_count = 0;
    free(hierarchy_polygon);
    hierarchy_polygon = 0;
    hierarchy_polygon_capacity = 0;
    memset(model_packet_templates, 0, sizeof(model_packet_templates));
    g_model_packet_count = 0;
}

/* Original data: SLES_004.74:0x800C8DEC..0x800C8E0B */
const MATRIX g_model_identity = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};

/* Functions. */
static sint32 parse_model(MODEL *model, const uint8 *data, sint32 size, sint32 *at)
{
    MODEL decoded = {0};
    sint32 bytes, idx, vertex_offset;
    sint32 min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
    if (size < 2 || *at < 0 || *at > size - 2)
        return 0;
    decoded.vertex_count = xport_load_le16(data + *at);
    *at += 2;
    bytes = decoded.vertex_count > 0 ? decoded.vertex_count * 6 : 0;
    if (size - *at < 2 || bytes > size - *at - 2)
        return 0;
    vertex_offset = *at;
    *at += bytes;
    decoded.polygon_count = xport_load_le16(data + *at);
    *at += 2;
    bytes = decoded.polygon_count > 0 ? decoded.polygon_count * 28 : 0;
    /* The original returns two bytes beyond the final readable polygon */
    if (bytes > size - *at)
        return 0;
    decoded.polygons = (MODEL_POLYGON_RECORD *)(data + *at);
    *at += bytes + 2;
    if (decoded.vertex_count > 0)
    {
        decoded.vertices = malloc(decoded.vertex_count * sizeof(*decoded.vertices));
        if (decoded.vertices == 0)
            return 0;
    }
    for (idx = 0; idx < decoded.vertex_count; ++idx)
    {
        const uint8 *entry = data + vertex_offset + idx * 6;
        MODEL_VERTEX *vertex = decoded.vertices + idx;
        sint32 x, y, z;
        vertex->x = xport_load_le16(entry);
        vertex->y = xport_load_le16(entry + 2);
        vertex->z = xport_load_le16(entry + 4);
        x = vertex->x;
        y = vertex->y;
        z = vertex->z;
        if (x < min_x)
            min_x = x;
        if (x > max_x)
            max_x = x;
        if (y < min_y)
            min_y = y;
        if (y > max_y)
            max_y = y;
        if (z < min_z)
            min_z = z;
        if (z > max_z)
            max_z = z;
    }
    decoded.desc_half_x = (max_x - min_x) / 2;
    decoded.desc_half_y = (min_y - max_y) / 2;
    decoded.desc_half_z = (max_z - min_z) / 2;
    free(model->vertices);
    *model = decoded;
    return 1;
}

sint32 model_begin(const void *src, sint32 size, sint32 count)
{
    MODEL *loaded;
    MODEL_DESC *descs;
    sint32 total;
    sint32 idx;
    if (!src || size <= 0 || count <= 0 || count > 0x7fff)
        return 0;
    total = count + 1;
    loaded = (MODEL *)calloc(total, sizeof(*loaded));
    descs = (MODEL_DESC *)calloc(total, sizeof(*descs));
    if (!loaded || !descs)
    {
        free(loaded);
        free(descs);
        return 0;
    }
    memset(descs, 0, total * sizeof(*descs));
    memset(model_packet_templates, 0, sizeof(model_packet_templates));
    for (idx = 0; idx < g_model_catalog.capacity; ++idx)
        free(g_model_catalog.recs[idx].vertices);
    free(g_model_catalog.recs);
    free(g_model_catalog.descs);
    g_model_catalog.recs = loaded;
    g_model_catalog.descs = descs;
    g_model_catalog.count = 0;
    g_model_catalog.capacity = total;
    g_model_catalog.src = (const uint8 *)src;
    g_model_catalog.src_size = size;
    return 1;
}

sint32 model_reserve_base(sint32 count)
{
    if (count < 0 || count >= g_model_catalog.capacity)
        return 0;
    g_model_catalog.count = count;
    return 1;
}

/* Direct translation of 0x80092D50..0x800933E4.  PAL mutates every packed
 * polygon in place: material is resolved first, word 0 becomes the global
 * packet-template index, bit 0x4000 is cleared from word 1, and UVs receive
 * the material's TEXINFO base.  DAT_800DD7D8 is represented by the fixed
 * 0x226-entry packet-template array below, retaining its exact 40-byte
 * stride for both FT3 and FT4 records. */
/* Original: FUN_80092D50. */
sint32 model_parse_and_build_packets(sint32 material1, sint32 material2, sint32 material3, sint32 material4, const sint32 *material_table, sint32 material_count)
{
    sint32 idx = g_model_draw.model_idx;
    sint32 at = 0, polygon_idx;
    const uint8 *data;
    MODEL *model;
    MODEL_DESC *desc;
    MODEL_POLYGON_RECORD *polygon;
    sint32 available;
    if (idx >= g_model_catalog.capacity || g_model_catalog.descs == 0)
        return -1;
    desc = &g_model_catalog.descs[idx];
    g_model_count = (g_model_count + 1);
    data = desc->data;
    if (data == 0)
        return -1;
    model = &g_model_catalog.recs[idx];
    if (desc->data_size > 0x7ffffffdu)
        return -1;
    available = desc->data_size;
    if (!parse_model(model, data, available, &at))
        return -1;
    polygon = model->polygons;
    for (polygon_idx = 0; polygon_idx < model->polygon_count; polygon_idx++)
    {
        MODEL_POLYGON_RECORD *src = &polygon[polygon_idx];
        const VRAM_SPRITE *tex;
        MODEL_PACKET *packet;
        sint16 material = xport_load_le16(src->material_or_packet);
        uint16 packet_idx = g_model_packet_count;
        sint32 triangle;
        POINT_T *vertices;
        RGBC *color;
        if (material_table != 0)
        {
            if (material >= material_count)
                return -1;
            uint32 resource = material_table[material];
            material = resource >> 10;
            xport_store_le16(src->material_or_packet, material);
        }
        if (material1 != 0 && material == 1)
            xport_store_le16(src->material_or_packet, (sint16)div_1024_trunc(material1));
        if (material2 != 0 && material == 2)
            xport_store_le16(src->material_or_packet, (sint16)div_1024_trunc(material2));
        if (material3 != 0 && material == 3)
            xport_store_le16(src->material_or_packet, (sint16)div_1024_trunc(material3));
        if (material4 != 0 && material == 4)
            xport_store_le16(src->material_or_packet, (sint16)div_1024_trunc(material4));
        material = xport_load_le16(src->material_or_packet);
        if (packet_idx >= 0x226)
            return -1;
        xport_store_le16(src->material_or_packet, packet_idx);
        xport_store_le16(src->shade_flags, xport_load_le16(src->shade_flags) & 0xbfff);
        triangle = xport_load_le_s16(src->vertex_offsets[0]) == -1;
        tex = spr_vram_desc_for_resource((uint32)material << 10);
        if (tex == 0)
            return -1;

        xport_store_le16(src->uv[0].u, (uint16)(xport_load_le16(src->uv[0].u) + tex->u0));
        xport_store_le16(src->uv[0].v, (uint16)(xport_load_le16(src->uv[0].v) + tex->v0));
        xport_store_le16(src->uv[1].u, (uint16)(xport_load_le16(src->uv[1].u) + tex->u0));
        xport_store_le16(src->uv[1].v, (uint16)(xport_load_le16(src->uv[1].v) + tex->v0));
        if (!triangle)
        {
            xport_store_le16(src->uv[2].u, (uint16)(xport_load_le16(src->uv[2].u) + tex->u0));
            xport_store_le16(src->uv[2].v, (uint16)(xport_load_le16(src->uv[2].v) + tex->v0));
        }
        xport_store_le16(src->uv[3].u, (uint16)(xport_load_le16(src->uv[3].u) + tex->u0));
        xport_store_le16(src->uv[3].v, (uint16)(xport_load_le16(src->uv[3].v) + tex->v0));

        packet = &model_packet_templates[packet_idx];
        memset(packet, 0, sizeof(*packet));
        if (triangle)
        {
            POLY_FT3 *poly = &packet->triangle;
            setPolyFT3(poly);
            vertices = poly->p;
            color = &poly->rgbc;
        }
        else
        {
            POLY_FT4 *poly = &packet->quad;
            setPolyFT4(poly);
            vertices = poly->p;
            color = &poly->rgbc;
            vertices[2].uv.u = xport_load_le16(src->uv[2].u);
            vertices[2].uv.v = xport_load_le16(src->uv[2].v);
        }
        color->r = color->g = color->b = 0x80;
        vertices[0].uv.u = xport_load_le16(src->uv[0].u);
        vertices[0].uv.v = xport_load_le16(src->uv[0].v);
        vertices[0].attr = tex->clut;
        vertices[1].uv.u = xport_load_le16(src->uv[1].u);
        vertices[1].uv.v = xport_load_le16(src->uv[1].v);
        vertices[1].attr = tex->tpage;
        vertices[triangle ? 2 : 3].uv.u = xport_load_le16(src->uv[3].u);
        vertices[triangle ? 2 : 3].uv.v = xport_load_le16(src->uv[3].v);
        g_model_packet_count = (g_model_packet_count + 1);
    }
    desc->half_x = model->desc_half_x;
    desc->half_y = model->desc_half_y;
    desc->half_z = model->desc_half_z;
    desc->col = 1;
    if (g_model_catalog.count <= idx)
        g_model_catalog.count = idx + 1;
    g_model_draw.world_x = ((uint32)g_model_draw.world_x - g_model_draw.translation_x);
    g_model_draw.world_z = ((uint32)g_model_draw.world_z - g_model_draw.translation_z);
    return at;
}

/* Append to the existing native descriptor owner */
static sint32 model_append(const void *src, sint32 size, sint32 count, sint32 material1, sint32 material2, sint32 material3)
{
    MODEL *grown;
    MODEL_DESC *grown_descs;
    sint32 cursor = 0;
    sint32 base = g_model_catalog.count, idx;
    if (!src || size <= 0 || count == 0 || count > 0x7fff || base > 0x7fff - count || g_model_catalog.descs == 0)
        return -1;
    if (base + count >= g_model_catalog.capacity)
    {
        sint32 capacity = base + count + 1;
        grown = (MODEL *)calloc(capacity, sizeof(*grown));
        grown_descs = (MODEL_DESC *)calloc(capacity, sizeof(*grown_descs));
        if (grown == 0 || grown_descs == 0)
        {
            free(grown);
            free(grown_descs);
            return -1;
        }
        memcpy(grown, g_model_catalog.recs, g_model_catalog.capacity * sizeof(*grown));
        memcpy(grown_descs, g_model_catalog.descs, g_model_catalog.capacity * sizeof(*grown_descs));
        free(g_model_catalog.recs);
        free(g_model_catalog.descs);
        g_model_catalog.recs = grown;
        g_model_catalog.descs = grown_descs;
        g_model_catalog.capacity = capacity;
    }
    g_model_catalog.src = (const uint8 *)src;
    g_model_catalog.src_size = size;
    for (idx = 0; idx < count; idx++)
    {
        g_model_draw.model_idx = base + idx;
        sint32 consumed;
        if (cursor > size)
            return -1;
        g_model_catalog.descs[base + idx].data = (const uint8 *)src + cursor;
        g_model_catalog.descs[base + idx].data_size = size - cursor;
        consumed = model_parse_and_build_packets(material1, material2, material3, 0, 0, 0);
        if (consumed < 0)
            return -1;
        /* Original final cursor may be two bytes beyond the input span */
        cursor += consumed;
    }
    return base;
}

/* Original: FUN_80095300. */
GDB_CALL void model_stage_load(const char *path, sint32 size, sint16 *base, sint16 count, sint32 material1, sint32 material2, sint32 material3, const uint8 *data)
{
    sint32 first;
    if (data == 0)
    {
        XASSERT(!(path == 0));
        size = game_file_size(path);
        data = runtime_heap_allocate_sector_aligned(size);
        game_file_read(path, (void *)data, size);
        size = (size + 0x7ff) & ~0x7ff;
    }
    XASSERT(!(base == 0));
    first = g_model_count;
    *base = first;
    g_model_draw.model_idx = first;
    if (count > 0)
    {
        if (first < 0 || first != g_model_catalog.count || size <= 0 || model_append(data, size, count, material1, material2, material3) != first)
        {
            fatal_error_with_val("STAGE MODEL ERROR", first);
            abort();
        }
        g_model_draw.model_idx = first + count;
    }
    runtime_heap_sweep();
}

static sint32 model_vertex_idx(sint16 byte_offset, sint32 count)
{
    sint32 i;
    if (byte_offset < 0 || (byte_offset & 1))
        return -1;
    i = byte_offset >> 1;
    return i < count ? i : -1;
}

static sint32 model_instance_shade(const MAP_MODEL_INSTANCE *instance)
{
    /* FUN_80094548 0x80094B2C..0x80094B80: byte 10 minus the current
  * cell-light offset, saturated to 0..255. */
    sint32 shade = ((uint32)instance->light - model_cell_light_offset);
    if (shade < 0)
        shade = 0;
    if (shade > 255)
        shade = 255;
    return shade;
}

/* Original: FUN_800914D8. */
void model_render(void)
{
    sint32 id = g_model_draw.model_idx;
    sint32 wx = (uint32)g_model_draw.world_x + g_model_draw.translation_x;
    sint32 wy = g_model_draw.world_y;
    sint32 wz = (uint32)g_model_draw.world_z + g_model_draw.translation_z;
    sint32 rot_y = g_model_draw.rotation_y, rot_x = g_model_draw.rotation_x, rot_z = g_model_draw.rotation_z;
    sint32 shade = g_model_draw.shade;
    sint32 row_bucket = g_model_draw.depth_bucket;
    MODEL *model;
    MATRIX matrix;
    XY *scr;
    sint32 i, base_x, base_y, base_z, numerator, visibility_depth, scr_bottom;
    sint32 general_rotation = rot_x != 0 || rot_z != 0 || (rot_y & 0x3ffu) != 0;
    sint16 first_packet_idx = 0;
    if (id >= g_model_catalog.count)
        return;
    model = &g_model_catalog.recs[id];
    if (g_model_draw.vertical_cull_override == 0)
    {
        visibility_depth = (g_model_draw.world_z - (g_camera.world_z - 0x4000u));
        numerator = ((wy + ((uint32)model->desc_half_y << 8) - g_camera.world_y) * 355u);
        if (visibility_depth == 0)
        {
#if defined(_DEBUG)
            fputs("MODEL CULL BREAK 7\n", stderr);
#endif
            abort();
        }
        if (visibility_depth == -1 && numerator == (-2147483647 - 1))
        {
#if defined(_DEBUG)
            fputs("MODEL CULL BREAK 6\n", stderr);
#endif
            abort();
        }
        scr_bottom = numerator / visibility_depth;
        if (scr_bottom > 0x40 + g_render_state.height / 2)
            return;
    }
    if (g_model_catalog.descs && g_model_catalog.descs[id].special != 0)
        return;
    scr = malloc((model->vertex_count > 0 ? model->vertex_count : 0) * sizeof(*scr));
    if (model->vertex_count > 0 && !scr)
    {
        free(scr);
        fatal_error("MODEL VERTEX MEMORY");
        abort();
    }
    base_x = div_256_trunc(((uint32)wx - g_camera.world_x));
    base_y = div_256_trunc(((uint32)wy - g_camera.world_y));
    base_z = div_256_trunc(((uint32)wz - g_camera.world_z));
    if (general_rotation)
    {
        SVECTOR rotation;
        rotation.vx = rot_x;
        rotation.vy = rot_y;
        rotation.vz = rot_z;
        rotation.pad = 0;
        RotMatrixYXZ(&rotation, &matrix);
        matrix.t[0] = base_x;
        matrix.t[1] = base_y;
        matrix.t[2] = base_z;
        SetRotMatrix(&matrix);
        SetTransMatrix(&matrix);
    }
    else XASSERT(!(model->vertex_count > 0 && rot_y != 0 && rot_y != 0x400 && rot_y != 0x800 && rot_y != 0xc00));
    for (i = 0; i < model->vertex_count; ++i)
    {
        const MODEL_VERTEX *vertex = &model->vertices[i];
        sint32 x, y, depth;
        if (general_rotation)
        {
            xport_gte_write_data(0, (uint16)vertex->x | ((uint32)(uint16)vertex->y << 16));
            xport_gte_write_data(1, (uint16)vertex->z);
            xport_gte_mvmva(0x480012u);
            x = xport_gte_read_data(25);
            y = xport_gte_read_data(26);
            depth = xport_gte_read_data(27);
        }
        else
        {
            y = base_y + vertex->y;
            if (rot_y == 0)
            {
                x = base_x + vertex->x;
                depth = base_z + vertex->z;
            }
            else if (rot_y == 0x400)
            {
                x = base_x + vertex->z;
                depth = base_z - vertex->x;
            }
            else if (rot_y == 0x800)
            {
                x = base_x - vertex->x;
                depth = base_z - vertex->z;
            }
            else
            {
                x = base_x - vertex->z;
                depth = base_z + vertex->x;
            }
            if (depth <= 0)
                depth = 8;
        }
        numerator = (x * 320u);
        if (depth == 0)
        {
#if defined(_DEBUG)
            fputs("MODEL VERTEX BREAK 7\n", stderr);
#endif
            abort();
        }
        if (depth == -1 && numerator == (-2147483647 - 1))
        {
#if defined(_DEBUG)
            fputs("MODEL VERTEX BREAK 6\n", stderr);
#endif
            abort();
        }
        scr[i].x = numerator / depth;
        numerator = (y * 355u);
        if (depth == -1 && numerator == (-2147483647 - 1))
        {
#if defined(_DEBUG)
            fputs("MODEL VERTEX BREAK 6\n", stderr);
#endif
            abort();
        }
        scr[i].y = numerator / depth;
    }
    if (model->polygon_count > 0)
    {
        first_packet_idx = xport_load_le16(model->polygons[0].material_or_packet);
        XASSERT(!(first_packet_idx < 0 || first_packet_idx >= 0x226 || model->polygon_count > 0x226 - first_packet_idx));
    }
    for (i = 0; i < model->polygon_count; ++i)
    {
        const MODEL_POLYGON_RECORD *polygon = &model->polygons[i];
        const MODEL_PACKET *packet = &model_packet_templates[first_packet_idx + i];
        sint32 idx[4], count, j, clip_xy[3];
        sint32 polygon_shade = shade - 0x80u + (sint16)xport_load_le16(polygon->shade_flags);
        if (polygon_shade > 255)
            polygon_shade = 255;
        if (polygon_shade < 0)
            polygon_shade = 0;
        if (xport_load_le_s16(polygon->vertex_offsets[0]) == -1)
        {
            for (j = 0; j < 3; ++j)
                idx[j] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[j + 1]), model->vertex_count);
            count = 3;
        }
        else
        {
            for (j = 0; j < 4; ++j)
                idx[j] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[j]), model->vertex_count);
            count = 4;
        }
        for (j = 0; j < count; ++j)
        {
            XASSERT(idx[j] >= 0);
        }
        for (j = 0; j < 3; ++j)
            clip_xy[j] = (scr[idx[j]].x + ((uint32)scr[idx[j]].y << 16));
        if (NormalClip(clip_xy[0], clip_xy[1], clip_xy[2]) < 0)
            continue;
        psx_set_prim_ot_bucket(row_bucket);
        if (count == 3)
            psx_submit_prepared_model_ft3(scr[idx[0]].x, scr[idx[0]].y, scr[idx[1]].x, scr[idx[1]].y, scr[idx[2]].x, scr[idx[2]].y, packet, polygon_shade, 0, g_cur_render_frame);
        else
            psx_submit_prepared_model_ft4(scr[idx[0]].x, scr[idx[0]].y, scr[idx[1]].x, scr[idx[1]].y, scr[idx[2]].x, scr[idx[2]].y, scr[idx[3]].x, scr[idx[3]].y, packet, polygon_shade, 0, g_cur_render_frame);
    }
    free(scr);
}

/* Original: SLES_004.74:FUN_80094518. */
static void model_cell_light_boost_apply(const MAP_FLOOR_CELL *cell)
{
    g_map_render.object_shade += (cell->packed_orientation >> 3) << 4;
    if (g_map_render.object_shade > 0xff)
        g_map_render.object_shade = 0xff;
}

void model_draw_runtime_model(sint32 id, sint32 x, sint32 y, sint32 z, sint32 rot_y, sint32 rot_x, sint32 rot_z, sint32 row_bucket)
{
    sint32 model_y;
    uint32 half;
    MODEL_DRAW_STATE saved = g_model_draw;
    XASSERT(!(id >= g_model_catalog.count || g_model_catalog.recs == 0));
    half = g_model_catalog.recs[id].desc_half_y;
    model_y = (y + (half << 8));
    g_model_draw.model_idx = id;
    g_model_draw.world_x = x;
    g_model_draw.world_y = model_y;
    g_model_draw.world_z = z;
    g_model_draw.translation_x = 0;
    g_model_draw.translation_z = 0;
    g_model_draw.rotation_y = rot_y;
    g_model_draw.rotation_x = rot_x;
    g_model_draw.rotation_z = rot_z;
    g_model_draw.shade = g_map_render.object_shade;
    g_model_draw.vertical_cull_override = 0;
    model_render();
    g_model_draw.model_idx = saved.model_idx;
    g_model_draw.world_x = saved.world_x;
    g_model_draw.world_y = saved.world_y;
    g_model_draw.world_z = saved.world_z;
    g_model_draw.translation_x = saved.translation_x;
    g_model_draw.translation_z = saved.translation_z;
    g_model_draw.rotation_y = saved.rotation_y;
    g_model_draw.rotation_x = 0;
    g_model_draw.rotation_z = 0;
    g_model_draw.shade = saved.shade;
    g_model_draw.vertical_cull_override = saved.vertical_cull_override;
}

static void submit_cell_item(CELL_RENDER_ITEM *item, sint32 cell_x, sint32 cell_z, sint32 row_bucket, sint32 ignore_drawn)
{
    if (item->drawn && !ignore_drawn)
        return;
    if (item->special)
    {
        const MAP_MODEL_INSTANCE *p = item->instance;
        sint32 id = p->model_id, wz = (g_map.depth * 0xc000u - ((cell_z + 1u) << 14) + p->local_z);
        FRAME_OBJECT logical; /* FUN_80094548 creates this temporary record and FUN_8008DDEC consumes it. */
        memset(&logical, 0, sizeof(logical));
        logical.world_x = (((uint32)cell_x << 14) + p->local_x);
        logical.world_y = item->y;
        logical.world_z = wz;
        logical.ot_bucket = row_bucket;
        logical.model_plus_one = id + 1;
        logical.model_rot_y = p->yaw;
        /* Special object light uses the preceding homogeneous record */
        logical.light_delta = ((p - 1)->light - 0x80);
        frame_model_submit(&logical);
    }
    else
    {
        frame_model_submit(item->object);
    }
}

static void submit_static_instance(const MAP_MODEL_INSTANCE *p, sint32 cell_x, sint32 cell_z, sint32 row_bucket, sint32 shade)
{
    sint32 base_x = ((uint32)cell_x << 14);
    sint32 base_z = (g_map.depth * 0xc000u - ((cell_z + 1u) << 14));
    g_map_render.object_shade = shade;
    g_model_draw.model_idx = p->model_id;
    g_model_draw.world_x = base_x;
    g_model_draw.world_y = p->y * 256;
    g_model_draw.world_z = base_z;
    g_model_draw.translation_x = p->local_x;
    g_model_draw.translation_z = p->local_z;
    g_model_draw.rotation_y = p->yaw;
    g_model_draw.rotation_x = 0;
    g_model_draw.rotation_z = 0;
    if (model_cell && (model_cell->packed_orientation & 0xf8) != 0)
        model_cell_light_boost_apply(model_cell);
    g_model_draw.shade = g_map_render.object_shade;
    model_render();
}

/* Original: SLES_004.74:FUN_80094548. */
void model_draw_cell(MAP_FLOOR_CELL *cell, sint32 cell_x, sint32 cell_z, sint32 row_bucket)
{
    CELL_RENDER_ITEM *items;
    DEFERRED_CELL_MODEL *deferred;
    sint32 grp_idx, item_capacity;
    FRAME_OBJECT *objects[SPATIAL_BUCKET_CAPACITY];
    sint32 count = 0, special_count = 0, item_count = 0, deferred_count = 0, visible_model_seen = 0, i, j;
    uint32 row_z = g_map.depth * 0xc000u - ((cell_z + 1u) << 14);
    sint32 cell_idx = (uint32)cell_z * g_map.width + cell_x;
    uint32 light_idx = cell_idx + g_map.width;
    const MAP_MODEL_GROUP *g;
    sint32 map_light = 0;
    if (cell == 0 || cell_x < 0 || cell_z < 0 || cell_x >= g_map.width || cell_z >= g_map.depth)
        return;
    g = cell->grp;
    XASSERT(!(g_map.floor == 0 || cell_idx >= g_map.floor_count || (g_map.light.vals == 0 || light_idx >= g_map.light.count) || g == 0));
    map_light = g_map.light.vals[light_idx];
    g_model_draw.world_x = ((uint32)cell_x << 14);
    g_model_draw.world_z = row_z;
    model_cell = cell;
    model_cell_light_offset = ((uint32)g_map_render.depth_light_bias - map_light - g_map_render.brightness);
    count = g == g_map.grps ? 0 : g->active_count;
    special_count = g == g_map.grps ? 0 : g->removed_count;
    grp_idx = (g - g_map.grps);
    XASSERT(!(grp_idx >= g_map.rec_capacity || count < 0 || special_count < 0 || count + special_count > g_map.rec_capacity - grp_idx - 1));
    item_capacity = SPATIAL_BUCKET_CAPACITY + special_count;
    if (item_capacity > cell_render_work.item_capacity)
    {
        CELL_RENDER_ITEM *grown = realloc(cell_render_work.items, item_capacity * sizeof(*grown));
        XASSERT(!(grown == 0));
        cell_render_work.items = grown;
        cell_render_work.item_capacity = item_capacity;
    }
    if (count > cell_render_work.deferred_capacity)
    {
        DEFERRED_CELL_MODEL *grown = realloc(cell_render_work.deferred, count * sizeof(*grown));
        XASSERT(!(grown == 0));
        memset(grown + cell_render_work.deferred_capacity, 0, (count - cell_render_work.deferred_capacity) * sizeof(*grown));
        cell_render_work.deferred = grown;
        cell_render_work.deferred_capacity = count;
    }
    items = cell_render_work.items;
    deferred = cell_render_work.deferred;
    i = spatial_cell_objects_get(cell_x, cell_z, objects);
    for (j = 0; j < i; j++)
    {
        items[item_count].special = 0;
        items[item_count].drawn = objects[j]->drawn;
        items[item_count].z = objects[j]->world_z;
        items[item_count].y = objects[j]->world_y;
        items[item_count].object = objects[j];
        items[item_count].instance = 0;
        ++item_count;
    }
    for (i = 0; i < special_count; i++)
    {
        const MAP_MODEL_INSTANCE *p = (g + 1) + count + i;
        sint32 id = p->model_id, half, y, z, den, scr, numerator;
        XASSERT(!(id >= g_model_catalog.count || g_model_catalog.recs == 0));
        half = g_model_catalog.recs[id].desc_half_y;
        y = (p->y - half - 5000) * 256;
        z = (row_z + p->local_z);
        den = (row_z - (g_camera.world_z - 0x4000u));
        numerator = (((uint32)y - g_camera.world_y) * 355u);
        if (den == 0 || (den == -1 && numerator == (-2147483647 - 1)))
        {
#if defined(_DEBUG)
            fputs("CELL MODEL PROJECTION BREAK 7/6\n", stderr);
#endif
            abort();
        }
        scr = numerator / den;
        if (scr <= 0x40 - g_render_state.height / 2)
            continue;
        items[item_count].special = 1;
        items[item_count].drawn = 0;
        items[item_count].z = z;
        items[item_count].y = y;
        items[item_count].object = 0;
        items[item_count].instance = p;
        ++item_count;
    }
    /* 80094970..8009498C: slt(next.z,current.z), then swap.  The producer
  * order is near-to-far; AddPrim reverses it into far-to-near GPU order. */
    {
        sint32 swapped;
        do
        {
            swapped = 0;
            for (i = 0; i + 1 < item_count; i++)
                if (ot_depth_should_swap_cell_objects(items[i].z, items[i + 1].z))
                {
                    CELL_RENDER_ITEM t = items[i];
                    items[i] = items[i + 1];
                    items[i + 1] = t;
                    swapped = 1;
                }
        } while (swapped);
    }
    g_model_draw.vertical_cull_override = 0;
    g_map_render.object_shade = ((cell->material == 0xff ? 0x80u : cell->shade) - model_cell_light_offset);
    if (g_map_render.object_shade < 0)
        g_map_render.object_shade = 0;
    if (g_map_render.object_shade > 255)
        g_map_render.object_shade = 255;
    for (i = 0; i < count; i++)
    {
        const MAP_MODEL_INSTANCE *p = (g + 1) + i;
        sint32 id = p->model_id; sint32 half; uint32 top_edge; sint32 bottom_edge; sint32 depth; sint32 scr_top; sint32 scr_bottom; sint32 numerator;
        sint32 model_y = p->y * 256;
        g_model_draw.model_idx = id;
        g_model_draw.world_y = model_y;
        XASSERT(!(id >= g_model_catalog.count || g_model_catalog.recs == 0));
        half = g_model_catalog.recs[id].desc_half_y;
        if (model_y == 0x4e2000)
            continue;
        /* Descriptor +8 is (minY-maxY)/2 and therefore non-positive.  These two
   * expressions are a literal translation of 80094a44 and 80094b88. */
        top_edge = (model_y - half * 256);
        bottom_edge = (model_y + half * 256);
        if (!visible_model_seen)
        {
            depth = (row_z - (g_camera.world_z - 0x4000u));
            numerator = ((top_edge - g_camera.world_y) * 355u);
            if (depth == 0 || (depth == -1 && numerator == (-2147483647 - 1)))
            {
#if defined(_DEBUG)
                fputs("CELL MODEL PROJECTION BREAK 7/6\n", stderr);
#endif
                abort();
            }
            scr_top = numerator / depth;
            if (scr_top <= 0x40 - g_render_state.height / 2)
            {
                g_map_render.object_shade = model_instance_shade(p);
                continue;
            }
        }
        visible_model_seen = 1;
        g_model_draw.translation_x = p->local_x;
        g_model_draw.translation_z = p->local_z;
        g_model_draw.rotation_y = p->yaw;
        depth = (row_z - g_camera.world_z);
        /* 80094B2C..80094B80 loads the current static instance's byte-10
   * brightness into gp+0x2A4 before either ordering branch.  Objects emitted
   * before an on-screen model therefore inherit that model-layer shade; the
   * assignment must not be delayed until submit_static_instance(). */
        g_map_render.object_shade = model_instance_shade(p);
        numerator = ((top_edge - g_camera.world_y) * 355u);
        if (depth == 0 || (depth == -1 && numerator == (-2147483647 - 1)))
        {
#if defined(_DEBUG)
            fputs("CELL MODEL PROJECTION BREAK 7/6\n", stderr);
#endif
            abort();
        }
        scr_top = numerator / depth;
        if (scr_top < 0)
        {
            DEFERRED_CELL_MODEL *d;
            d = &deferred[deferred_count++];
            if (item_count > d->item_capacity)
            {
                sint32 *grown = realloc(d->item, item_count * sizeof(*grown));
                XASSERT(!(grown == 0));
                d->item = grown;
                d->item_capacity = item_count;
            }
            d->instance = p;
            d->shade = g_map_render.object_shade;
            d->item_count = 0; /* 80094C50 deliberately does not test FrameObject+0x28 here: a deferred model captures every object below its boundary, including one already emitted by an earlier model. */
            for (j = 0; j < item_count; j++)
                if (items[j].y <= bottom_edge + 0x200)
                {
                    items[j].drawn = 1;
                    if (!items[j].special && items[j].object)
                        items[j].object->drawn = 1;
                    d->item[d->item_count++] = j;
                }
            continue;
        }
        numerator = (((uint32)bottom_edge - g_camera.world_y) * 355u);
        if (depth == 0 || (depth == -1 && numerator == (-2147483647 - 1)))
        {
#if defined(_DEBUG)
            fputs("CELL MODEL PROJECTION BREAK 7/6\n", stderr);
#endif
            abort();
        }
        scr_bottom = numerator / depth;
        if (scr_bottom < 0)
        {
            /* FUN_80094548 80094D1C..80094E38: when the model's bottom edge is
      above the draw origin, submit the model before the eligible objects. */
            submit_static_instance(p, cell_x, cell_z, row_bucket, g_map_render.object_shade);
            for (j = 0; j < item_count; j++)
                if (!items[j].drawn && items[j].y <= bottom_edge + 0x200)
                {
                    submit_cell_item(&items[j], cell_x, cell_z, row_bucket, 0);
                    items[j].drawn = 1;
                    if (!items[j].special && items[j].object)
                        items[j].object->drawn = 1;
                }
        }
        else
        {
            /* FUN_80094548 80094D40..80094E5C performs the same object scan first
      when the bottom edge is on-screen, then emits the model.  AddPrim is
      LIFO, so exchanging these calls reverses model/sprite occlusion. */
            for (j = 0; j < item_count; j++)
                if (!items[j].drawn && items[j].y <= bottom_edge + 0x200)
                {
                    submit_cell_item(&items[j], cell_x, cell_z, row_bucket, 0);
                    items[j].drawn = 1;
                    if (!items[j].special && items[j].object)
                        items[j].object->drawn = 1;
                }
            submit_static_instance(p, cell_x, cell_z, row_bucket, g_map_render.object_shade);
        }
    }
    g_map_render.object_shade = ((cell->material == 0xff ? 0x80u : cell->shade) - model_cell_light_offset);
    if (g_map_render.object_shade < 0)
        g_map_render.object_shade = 0;
    if (g_map_render.object_shade > 255)
        g_map_render.object_shade = 255;
    for (i = 0; i < item_count; i++)
        submit_cell_item(&items[i], cell_x, cell_z, row_bucket, 0);
    g_model_draw.vertical_cull_override = 1;
    for (i = deferred_count - 1; i >= 0; i--)
    {
        DEFERRED_CELL_MODEL *d = &deferred[i];
        submit_static_instance(d->instance, cell_x, cell_z, row_bucket, d->shade);
        for (j = d->item_count - 1; j >= 0; j--)
            submit_cell_item(&items[d->item[j]], cell_x, cell_z, row_bucket, 1);
    }
    if ((cell->packed_orientation & 0xf8) != 0)
        cell->packed_orientation = (cell->packed_orientation - 8);
    model_cell = 0;
}

static void submit_hierarchy_polygon(const HIERARCHY_POLYGON *pending, sint32 bucket, RENDER_FRAME *target_frame)
{
    sint32 packet_idx = xport_load_le16(pending->polygon->material_or_packet);
    const MODEL_PACKET *packet = packet_idx < 0x226 ? &model_packet_templates[packet_idx] : 0;
    psx_set_prim_ot_bucket(bucket);
    psx_set_prim_depth(pending->depth);
    if (pending->triangle)
        psx_submit_prepared_model_ft3(pending->scr[0].x, pending->scr[0].y, pending->scr[1].x, pending->scr[1].y, pending->scr[2].x, pending->scr[2].y, packet, 0x80, g_model_draw.clut_override, target_frame);
    else
        psx_submit_prepared_model_ft4(pending->scr[0].x, pending->scr[0].y, pending->scr[1].x, pending->scr[1].y, pending->scr[2].x, pending->scr[2].y, pending->scr[3].x, pending->scr[3].y, packet, 0x80, g_model_draw.clut_override, target_frame);
}

/* 0x800920A0..0x800920C0: host representation of ClearOTag(temporary,0x200). */
/* Original: FUN_800920A0. */
void model_render_begin(void)
{
    hierarchy_polygon_count = 0;
}

/* Exact FUN_800920C8 and FUN_80092138 translations. A node's first four words
 * are next sibling, previous sibling, first child and parent. */
static void model_detach(MODEL_NODE *node)
{
    MODEL_NODE *next = node->next_sibling;
    MODEL_NODE *prev = node->prev_sibling;
    if (next != 0)
        next->prev_sibling = prev;
    if (prev != 0)
        prev->next_sibling = next;
    else
    {
        MODEL_NODE *parent = node->parent;
        parent->first_child = next;
        node->parent = 0;
        node->prev_sibling = 0;
        node->next_sibling = 0;
    }
}

/* Original: FUN_800920C8. */
void model_attach_child(MODEL_NODE *node, MODEL_NODE *parent)
{
    MODEL_NODE *child;
    if (node->parent != 0)
        model_detach(node);
    node->parent = parent;
    child = parent->first_child;
    node->next_sibling = child;
    if (child != 0)
        child->prev_sibling = node;
    node->prev_sibling = 0;
    parent->first_child = node;
}

/* Resident hierarchy construction, also called by GYRO.BIN */
/* Original: FUN_8009541C. */
void model_build_from_config(MODEL_NODE *nodes, const sint16 *cfg, sint16 model_base)
{
    MODEL_NODE *node = nodes;
    while (*cfg != -1)
    {
        sint16 model = *cfg++, parent;
        if (model != -2)
            node->model_id = model_base + model;
        node->translation_x = *cfg++ * 256;
        node->translation_y = *cfg++ * 256;
        node->translation_z = *cfg++ * 256;
        parent = *cfg++;
        if (parent)
            model_attach_child(node, &nodes[parent - 1]);
        ++node;
    }
}

/* Original: SLES_004.74:FUN_800AC128. */
void model_pose_init(const MODEL_POSE_RESOURCE *resource, MODEL_NODE **nodes, sint32 node_count, sint32 pose)
{
    const uint8 *bytes;
    sint32 count, idx, stride = pose * 12u;
    XASSERT(!(resource == 0 || resource->data == 0 || resource->size < 4));
    bytes = (const uint8 *)resource->data;
    count = xport_load_le32(bytes);
    if (count <= 0)
        return;
    sint32 available_nodes = (resource->size - 8) / 8;
    XASSERT(!(resource->size < 8 || count > node_count || count > available_nodes));
    for (idx = 0; idx < count; ++idx)
    {
        const uint8 *entry = bytes + 8 + idx * 8;
        sint32 parent = xport_load_le32(entry + 4);
        XASSERT(!(((parent >= 0 || pose >= 0) && (nodes == 0 || nodes[idx] == 0)) || (parent >= 0 && (parent >= node_count || nodes[parent] == 0))));
        if (pose >= 0)
        {
            uint32 offset = xport_load_le32(entry) + stride;
            XASSERT(!(offset > resource->size || resource->size - offset < 12));
        }
    }
    /* Reject a cyclic final hierarchy before mutating native nodes */
    for (idx = 0; idx < count; ++idx)
    {
        MODEL_NODE *slow, *fast;
        if (xport_load_le_s32(bytes + 12 + idx * 8) < 0)
            continue;
        slow = fast = nodes[idx];
        do
        {
            uint32 pass;
            for (pass = 0; pass < 3; ++pass)
            {
                MODEL_NODE *cursor = pass == 0 ? slow : fast;
                MODEL_NODE *next;
                sint32 owner;
                if (cursor == 0)
                    continue;
                next = cursor->parent;
                for (owner = 0; owner < count; ++owner)
                {
                    sint32 parent;
                    if (nodes[owner] != cursor)
                        continue;
                    parent = xport_load_le32(bytes + 12 + owner * 8);
                    if (parent >= 0)
                        next = nodes[parent];
                }
                if (pass == 0)
                    slow = next;
                else
                    fast = next;
            }
            XASSERT(!(slow != 0 && slow == fast));
        } while (fast != 0);
    }
    for (idx = 0; idx < count; ++idx)
    {
        const uint8 *entry = bytes + 8 + idx * 8;
        sint32 parent = xport_load_le32(entry + 4);
        if (parent >= 0)
            model_attach_child(nodes[idx], nodes[parent]);
        if (pose >= 0)
        {
            const uint8 *val = bytes + (xport_load_le32(entry) + stride);
            MODEL_NODE *node = nodes[idx];
            node->rotation_x = (-((sint16)xport_load_le16(val) >> 4) & 0x0fff);
            node->rotation_y = (xport_load_le16(val + 2) >> 4);
            node->rotation_z = (xport_load_le16(val + 4) >> 4);
            node->translation_x = (sint16)xport_load_le16(val + 6) * 16;
            node->translation_y = -(sint16)xport_load_le16(val + 8) * 16;
            node->translation_z = (sint16)xport_load_le16(val + 10) * 16;
        }
    }
}

/* Original: SLES_004.74:FUN_800AC26C. */
void model_pose_apply(const MODEL_POSE_RESOURCE *resource, MODEL_NODE **nodes, sint32 node_count, sint32 pose)
{
    const uint8 *bytes;
    sint32 count, idx, stride = pose * 12u;
    XASSERT(!(resource == 0 || resource->data == 0 || resource->size < 4));
    bytes = (const uint8 *)resource->data;
    count = xport_load_le32(bytes);
    if (count <= 0)
        return;
    sint32 available_nodes = (resource->size - 8) / 8;
    XASSERT(!(resource->size < 8 || nodes == 0 || count > node_count || count > available_nodes));
    for (idx = 0; idx < count; ++idx)
    {
        uint32 offset;
        XASSERT(!(nodes[idx] == 0));
        if (nodes[idx]->pose_enabled == 0)
            continue;
        offset = xport_load_le32(bytes + 8 + idx * 8) + stride;
        XASSERT(!(offset > resource->size || resource->size - offset < 12));
    }
    for (idx = 0; idx < count; ++idx)
    {
        MODEL_NODE *node = nodes[idx];
        const uint8 *val;
        if (node->pose_enabled == 0)
            continue;
        val = bytes + (xport_load_le32(bytes + 8 + idx * 8) + stride);
        node->rotation_x = (-((sint16)xport_load_le16(val) >> 4) & 0x0fff);
        node->rotation_y = (xport_load_le16(val + 2) >> 4);
        node->rotation_z = (xport_load_le16(val + 4) >> 4);
        node->translation_x = (sint16)xport_load_le16(val + 6) * 16;
        node->translation_y = -(sint16)xport_load_le16(val + 8) * 16;
        node->translation_z = (sint16)xport_load_le16(val + 10) * 16;
    }
}

/* 0x800923A4..0x80092428. */
/* Original: FUN_800923A4. */
static void model_build_local_matrix(MODEL_NODE *node)
{
    SVECTOR rotation;
    MATRIX *matrix = &node->local_matrix;
    rotation.vx = node->rotation_x;
    rotation.vy = node->rotation_y;
    rotation.vz = node->rotation_z;
    rotation.pad = 0;
    if (g_session.stage == 4)
        RotMatrix(&rotation, matrix);
    else
        RotMatrixYXZ(&rotation, matrix);
    matrix->t[0] = node->translation_x;
    matrix->t[1] = node->translation_y;
    matrix->t[2] = node->translation_z;
}

/* Hierarchy projections feed native packets through the shared GPU runtime */
/* Original: FUN_800924EC. */
static void model_queue_mesh(MATRIX *matrix, RENDER_FRAME *target_frame, MODEL_NODE *node)
{
    MODEL *model;
    XY *scr;
    sint32 *depths;
    sint32 center_depth, scr_coord, i;
    sint32 model_id;
    MATRIX proj;
    center_depth = ((uint32)g_model_draw.world_z - g_camera.world_z);
    if (center_depth <= 0 || frame_render_work_enabled() == 0)
        return;
    model_id = g_model_draw.model_idx;
    if (model_id >= g_model_catalog.count)
        return;
    model = &g_model_catalog.recs[model_id];
    scr_coord = mul_lo((node->world_y + ((uint32)model->desc_half_y << 8) - g_camera.world_y), 355u) / center_depth;
    if (scr_coord >= 0xbf)
        return;
    scr_coord = mul_lo((node->world_y - ((uint32)model->desc_half_y << 8) - g_camera.world_y), 355u) / center_depth;
    if (scr_coord <= -0xbf)
        return;
    scr_coord = mul_lo(((uint32)node->world_x - g_camera.world_x), 320u) / center_depth;
    if ((scr_coord + 0x140u) >= 0x281u)
        return;
    if (g_model_draw.world_z < (g_camera.world_z + 0x8000))
        return;

    proj = *matrix;
    proj.t[0] = div_256_trunc(proj.t[0]);
    proj.t[1] = div_256_trunc(proj.t[1]);
    proj.t[2] = div_256_trunc(proj.t[2]);
    scr = malloc((model->vertex_count > 0 ? model->vertex_count : 0) * sizeof(*scr));
    depths = malloc((model->vertex_count > 0 ? model->vertex_count : 0) * sizeof(*depths));
    if (model->vertex_count > 0 && (!scr || !depths))
    {
        free(scr);
        free(depths);
        fatal_error("HIERARCHY VERTEX MEMORY");
        abort();
    }
    SetRotMatrix(&proj);
    SetTransMatrix(&proj);
    for (i = 0; i < model->vertex_count; i++)
    {
        VECTOR out;
        sint32 z;
        SVECTOR in;
        in.vx = model->vertices[i].x;
        in.vy = model->vertices[i].y;
        in.vz = model->vertices[i].z;
        in.pad = 0;
        xport_gte_write_data(0, (uint16)in.vx | ((uint32)(uint16)in.vy << 16));
        xport_gte_write_data(1, (uint16)in.vz);
        xport_gte_mvmva(0x480012u);
        out.vx = xport_gte_read_data(25);
        out.vy = xport_gte_read_data(26);
        out.vz = xport_gte_read_data(27);
        z = out.vz;
        depths[i] = z;
        if (z < 1)
            z = 8;
        scr[i].x = mul_lo(out.vx, 320u) / z;
        scr[i].y = mul_lo(out.vy, 355u) / z;
    }
    for (i = 0; i < model->polygon_count; i++)
    {
        const MODEL_POLYGON_RECORD *polygon = &model->polygons[i];
        HIERARCHY_POLYGON direct;
        HIERARCHY_POLYGON *pending;
        sint32 idx[4], count, j, max_z;
        sint32 clip_xy[3];
        if (xport_load_le_s16(polygon->vertex_offsets[0]) == -1)
        {
            idx[0] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[1]), model->vertex_count);
            idx[1] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[2]), model->vertex_count);
            idx[2] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[3]), model->vertex_count);
            count = 3;
        }
        else
        {
            idx[0] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[0]), model->vertex_count);
            idx[1] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[1]), model->vertex_count);
            idx[2] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[2]), model->vertex_count);
            idx[3] = model_vertex_idx(xport_load_le16(polygon->vertex_offsets[3]), model->vertex_count);
            count = 4;
        }
        for (j = 0; j < count; j++)
            if (idx[j] < 0)
                break;
        if (j != count)
            continue;
        /* Original signed-X addition can borrow into the packed Y halfword */
        for (j = 0; j < 3; ++j)
            clip_xy[j] = (scr[idx[j]].x + ((uint32)scr[idx[j]].y << 16));
        if (NormalClip(clip_xy[0], clip_xy[1], clip_xy[2]) < 0)
            continue;
        max_z = depths[idx[0]];
        for (j = 1; j < count; j++)
            if (depths[idx[j]] > max_z)
                max_z = depths[idx[j]];
        if (target_frame == 0 && hierarchy_polygon_count == hierarchy_polygon_capacity)
        {
            uint32 remaining = model->polygon_count - i;
            uint32 capacity;
            HIERARCHY_POLYGON *grown;
            XASSERT(hierarchy_polygon_count <= 0x7fffffffu - remaining);
            capacity = hierarchy_polygon_count + remaining;
            XASSERT(!(capacity > SIZE_MAX / sizeof(*grown)));
            grown = realloc(hierarchy_polygon, capacity * sizeof(*grown));
            XASSERT(!(grown == 0));
            hierarchy_polygon = grown;
            hierarchy_polygon_capacity = capacity;
        }
        pending = target_frame != 0 ? &direct : &hierarchy_polygon[hierarchy_polygon_count];
        memset(pending, 0, sizeof(*pending));
        pending->polygon = polygon;
        pending->depth = max_z;
        pending->sequence = hierarchy_polygon_count;
        pending->triangle = count == 3;
        for (j = 0; j < count; j++)
        {
            pending->scr[j] = scr[idx[j]];
        }
        /* 80092728..80092750 selects the caller OT when a1 != 0.
         * 80092C58..80092C9C links each accepted polygon immediately at
         * base - (worldZ/256 - DAT_800D3F3C) + maximum transformed Z.
         * V2 uses this path and never calls the private-OT splice 9242C. */
        if (target_frame != 0)
            submit_hierarchy_polygon(pending, (g_model_draw.depth_bucket - ((uint32)div_256_trunc(g_model_draw.world_z) - g_camera.render_z) + max_z), target_frame);
        else
            hierarchy_polygon_count++;
    }
    free(scr);
    free(depths);
}

/* 0x80092194..0x800923A0. */
/* Original: FUN_80092194. */
void model_render_node(MODEL_NODE *node, const MATRIX *parent)
{
    MATRIX world;
    VECTOR local_translation, scaled;
    MODEL_NODE *child;
    sint32 model_id;
    if (!node || !parent || g_model_draw.depth_bucket >= 0x3e9)
        return;
    model_build_local_matrix(node);
    /* MulMatrix0 borrows the parent matrix without modifying it */
    MulMatrix0((MATRIX *)parent, &node->local_matrix, &world);
    local_translation.vx = node->local_matrix.t[0];
    local_translation.vy = node->local_matrix.t[1];
    local_translation.vz = node->local_matrix.t[2];
    local_translation.pad = 0;
    /* The PsyQ signature borrows this matrix without modifying it */
    ApplyMatrixLV((MATRIX *)parent, &local_translation, &scaled);
    world.t[0] = ((uint32)scaled.vx + parent->t[0]);
    world.t[1] = ((uint32)scaled.vy + parent->t[1]);
    world.t[2] = ((uint32)scaled.vz + parent->t[2]);
    if (node->scale != 0)
    {
        scaled.vx = scaled.vy = scaled.vz = node->scale;
        ScaleMatrix(&world, &scaled);
    }
    node->world_x = ((uint32)world.t[0] + g_camera.world_x);
    node->world_y = ((uint32)world.t[1] + g_camera.world_y);
    node->world_z = ((uint32)world.t[2] + g_camera.world_z);
    model_id = node->model_id;
    g_model_draw.model_idx = model_id;
    if (model_id != 0)
        model_queue_mesh(&world, g_model_draw.target_frame, node);
    if (node->parent == 0)
    {
        node->world_rotation_x = node->rotation_x;
        node->world_rotation_y = node->rotation_y;
        node->world_rotation_z = node->rotation_z;
    }
    else
    {
        MODEL_NODE *owner = node->parent;
        node->world_rotation_x = ((node->rotation_x + owner->world_rotation_x) & 0xfff);
        node->world_rotation_y = ((node->rotation_y + owner->world_rotation_y) & 0xfff);
        node->world_rotation_z = ((node->rotation_z + owner->world_rotation_z) & 0xfff);
    }
    child = node->first_child;
    while (child)
    {
        model_render_node(child, &world);
        child = child->next_sibling;
    }
}

static sint32 hierarchy_less(const HIERARCHY_POLYGON *a, const HIERARCHY_POLYGON *b)
{
    if (a->depth != b->depth)
        return a->depth < b->depth;
    return a->sequence > b->sequence;
}

/* 0x8009242C..0x800924E8.  ClearOTag links the private OT toward increasing
 * addresses. AddPrim makes equal-depth packets LIFO there, so its traversal
 * is ascending depth and descending source sequence. The splice prepends
 * every visited packet to the main bucket, reversing both dimensions again. */
/* Original: FUN_8009242C. */
void model_render_end(void)
{
    uint32 i, j;
    for (i = 1; i < hierarchy_polygon_count; i++)
    {
        HIERARCHY_POLYGON val = hierarchy_polygon[i];
        j = i;
        while (j > 0 && hierarchy_less(&val, &hierarchy_polygon[j - 1]))
        {
            hierarchy_polygon[j] = hierarchy_polygon[j - 1];
            j--;
        }
        hierarchy_polygon[j] = val;
    }
    for (i = 0; i < hierarchy_polygon_count; i++)
    {
        HIERARCHY_POLYGON *pending = &hierarchy_polygon[i];
        const MODEL_POLYGON_RECORD *polygon = pending->polygon;
        sint32 packet_idx = xport_load_le16(polygon->material_or_packet);
        const MODEL_PACKET *packet = packet_idx < 0x226 ? &model_packet_templates[packet_idx] : 0;
        psx_set_prim_ot_bucket(g_model_draw.depth_bucket);
        psx_set_prim_depth(pending->depth);
        if (pending->triangle)
            psx_submit_prepared_model_ft3(pending->scr[0].x, pending->scr[0].y, pending->scr[1].x, pending->scr[1].y, pending->scr[2].x, pending->scr[2].y, packet, 0x80, g_model_draw.clut_override, g_cur_render_frame);
        else
            psx_submit_prepared_model_ft4(pending->scr[0].x, pending->scr[0].y, pending->scr[1].x, pending->scr[1].y, pending->scr[2].x, pending->scr[2].y, pending->scr[3].x, pending->scr[3].y, packet, 0x80, g_model_draw.clut_override, g_cur_render_frame);
    }
    hierarchy_polygon_count = 0;
}
