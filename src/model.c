#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "global.h"
#include "map.h"
#include "model.h"
#include "object.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "render.h"
#include "sprite.h"

/* Types. */
/* Direct HQ translation of FUN_80092D50 and the fixed-bucket model renderer
 * FUN_800914D8.  FUN_800924EC is a separate hierarchical-object path. */

typedef struct PackedVertex
{
    sint16 x, y, z;
} PackedVertex;

typedef struct PackedPolygon
{
    sint16 word[14];
} PackedPolygon;

typedef struct Model
{
    const uint8 *record;
    uint16 vertex_count;
    const PackedVertex *vertices;
    uint16 polygon_count;
    PackedPolygon *polygons;
    sint32 descriptor_half_x, descriptor_half_y, descriptor_half_z;
} Model;

static sint32 mac_shift12(sint64 value)
{
    if (value >= 0)
        return (sint32)(value >> 12);
    return -(sint32)(((-value) + 0xfff) >> 12);
}

MATRIX *MulMatrix0(MATRIX *m0, MATRIX *m1, MATRIX *m2)
{
    MATRIX out;
    sint32 row, column;
    if (!m0 || !m1 || !m2)
        return m2;
    memset(&out, 0, sizeof(out));
    for (row = 0; row < 3; row++)
        for (column = 0; column < 3; column++)
            out.m[row][column] = (sint16)mac_shift12((sint64)m0->m[row][0] * m1->m[0][column] + (sint64)m0->m[row][1] * m1->m[1][column] + (sint64)m0->m[row][2] * m1->m[2][column]);
    *m2 = out;
    return m2;
}

typedef struct ModelPacketTemplate
{
    uint8 bytes[40];
} ModelPacketTemplate;

typedef struct CellRenderItem
{
    sint32 special, drawn, z, y;
    FrameObjectPartial *object;
    const MAP_MODEL_INSTANCE *instance;
} CellRenderItem;

typedef struct DeferredCellModel
{
    const MAP_MODEL_INSTANCE *instance;
    sint32 item_count;
    sint32 item[SPATIAL_BUCKET_CAPACITY + 64];
} DeferredCellModel;

/* -------------------------------------------------------------------------
 * Hierarchical model path: FUN_800920A0/FUN_80092194/FUN_800923A4/
 * FUN_8009242C/FUN_800924EC.  This path is deliberately separate from
 * FUN_800914D8 above: the original either inserts into the supplied main OT,
 * or sorts by maximum transformed Z in a private 0x200-entry OT before
 * splicing that chain into the caller's single main-OT bucket. */

typedef struct HierarchyPolygon
{
    const PackedPolygon *polygon;
    sint32 sx[4], sy[4];
    sint32 vertex[4];
    sint32 depth;
    sint32 sequence;
    sint32 triangle;
} HierarchyPolygon;

/* Variables. */
static Model *models;

static sint32 models_count, models_capacity;

static ModelPacketTemplate model_packet_templates[0x226];

static const uint8 *model_parse_begin, *model_parse_end;

static sint32 model_cell_light_offset, model_cell_light_boost, current_model_shade;

static const MAP_FLOOR_CELL *model_cell;

static HierarchyPolygon hierarchy_polygon[1024];

static sint32 hierarchy_polygon_count;

static sint32 hierarchy_offset_x = 160, hierarchy_offset_y = 64;

/* Functions. */
static sint32 parse_model(Model *model, const uint8 *data, sint32 size, sint32 *at)
{
    sint32 bytes, index, min_x = 0, max_x = 0, min_y = 0, max_y = 0, min_z = 0, max_z = 0;
    if (*at + 2 > size)
        return 0;
    model->record = data + *at;
    model->vertex_count = read_u16_le(data + *at);
    *at += 2;
    bytes = (sint32)model->vertex_count * sizeof(PackedVertex);
    if (*at + bytes + 2 > size)
        return 0;
    model->vertices = (const PackedVertex *)(data + *at);
    for (index = 0; index < model->vertex_count; index++)
    {
        sint32 x = model->vertices[index].x, y = model->vertices[index].y, z = model->vertices[index].z;
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
    model->descriptor_half_x = (max_x - min_x) / 2;
    model->descriptor_half_y = (min_y - max_y) / 2;
    model->descriptor_half_z = (max_z - min_z) / 2;
    *at += bytes;
    model->polygon_count = read_u16_le(data + *at);
    *at += 2;
    bytes = (sint32)model->polygon_count * sizeof(PackedPolygon);
    /* 80093390 returns s0+2 after the last polygon, but does not read those
     * two bytes.  MINGYRO.CC ends MODELS.BIN exactly at the final polygon,
     * so the readable bound is bytes, while the returned next-model cursor
     * deliberately advances by the additional halfword. */
    if (*at + bytes > size)
        return 0;
    model->polygons = (PackedPolygon *)(data + *at);
    *at += bytes + 2;
    return 1;
}

sint32 model_begin(const void *source, sint32 size, sint32 count)
{
    Model *loaded;
    OriginalModelDescriptor *descriptors;
    sint32 total = count + 1;
    if (!source || size <= 0 || count <= 0)
        return 0;
    loaded = (Model *)calloc(total, sizeof(*loaded));
    descriptors = g_model_descriptors;
    if (descriptors == 0)
        descriptors = (OriginalModelDescriptor *)calloc(total, sizeof(*descriptors));
    if (!loaded || !descriptors)
    {
        free(loaded);
        return 0;
    }
    memset(descriptors, 0, (uint32)total * sizeof(*descriptors));
    memset(model_packet_templates, 0, sizeof(model_packet_templates));
    free(models);
    models = loaded;
    g_model_descriptors = descriptors;
    models_count = 0;
    models_capacity = total;
    model_parse_begin = (const uint8 *)source;
    model_parse_end = model_parse_begin + size;
    return 1;
}

sint32 model_reserve_base(sint32 count)
{
    if (count < 0 || count >= models_capacity)
        return 0;
    models_count = count;
    return 1;
}

void model_select_source(const void *source, sint32 size)
{
    model_parse_begin = (const uint8 *)source;
    model_parse_end = model_parse_begin + size;
}

static void model_packet_u16(uint8 *packet, sint32 offset, uint16 value)
{
    packet[offset] = (uint8)value;
    packet[offset + 1] = (uint8)(value >> 8);
}

/* Direct translation of 0x80092D50..0x800933E4.  PAL mutates every packed
 * polygon in place: material is resolved first, word 0 becomes the global
 * packet-template index, bit 0x4000 is cleared from word 1, and UVs receive
 * the material's TEXINFO base.  DAT_800DD7D8 is represented by the fixed
 * 0x226-entry packet-template array below, retaining its exact 40-byte
 * stride for both FT3 and FT4 records. */
/* Original: FUN_80092D50. */
void *model_parse_and_build_packets(sint32 material1, sint32 material2, sint32 material3, sint32 material4, const sint32 *material_table)
{
    sint32 index = g_current_model_id, at = 0, polygon_index;
    const uint8 *data;
    Model *model;
    PackedPolygon *polygon;
    sint32 available;
    if (index < 0 || index >= models_capacity || g_model_descriptors == 0)
        return 0;
    g_model_count = (sint16)(g_model_count + 1);
    data = (const uint8 *)g_model_descriptors[index].model_data;
    if (data == 0)
        return 0;
    model = &models[index];
    if (data >= model_parse_begin && data < model_parse_end)
        available = (sint32)(model_parse_end - data);
    else
        available = 0x100;
    if (!parse_model(model, data, available, &at))
        return 0;
    polygon = model->polygons;
    for (polygon_index = 0; polygon_index < model->polygon_count; polygon_index++)
    {
        PackedPolygon *source = &polygon[polygon_index];
        PSXMapTextureInfo texture;
        ModelPacketTemplate *packet;
        sint32 material = (sint32)source->word[0];
        sint32 packet_index = (sint32)(uint16)g_model_packet_count;
        sint32 triangle;
        uint8 *bytes;
        if (material_table != 0)
        {
            material = (sint32)(sint16)((uint32)material_table[material] >> 10);
            source->word[0] = (sint16)material;
        }
        if (material1 != 0 && material == 1)
            source->word[0] = (sint16)div_1024_trunc(material1);
        if (material2 != 0 && material == 2)
            source->word[0] = (sint16)div_1024_trunc(material2);
        if (material3 != 0 && material == 3)
            source->word[0] = (sint16)div_1024_trunc(material3);
        if (material4 != 0 && material == 4)
            source->word[0] = (sint16)div_1024_trunc(material4);
        material = (sint32)source->word[0];
        if (packet_index < 0 || packet_index >= 0x226)
            return 0;
        source->word[0] = (sint16)packet_index;
        source->word[1] = (sint16)((uint16)source->word[1] & 0xbfff);
        triangle = source->word[2] == -1;
        if (!psx_get_map_texture_info(material, &texture))
            return 0;

        source->word[6] = (sint16)((uint16)source->word[6] + texture.u);
        source->word[7] = (sint16)((uint16)source->word[7] + texture.v);
        source->word[8] = (sint16)((uint16)source->word[8] + texture.u);
        source->word[9] = (sint16)((uint16)source->word[9] + texture.v);
        if (!triangle)
        {
            source->word[10] = (sint16)((uint16)source->word[10] + texture.u);
            source->word[11] = (sint16)((uint16)source->word[11] + texture.v);
        }
        source->word[12] = (sint16)((uint16)source->word[12] + texture.u);
        source->word[13] = (sint16)((uint16)source->word[13] + texture.v);

        packet = &model_packet_templates[packet_index];
        bytes = packet->bytes;
        memset(bytes, 0, 40);
        bytes[3] = (uint8)(triangle ? 7 : 9);
        bytes[4] = bytes[5] = bytes[6] = 0x80;
        bytes[7] = (uint8)(triangle ? 0x24 : 0x2c);
        bytes[12] = (uint8)source->word[6];
        bytes[13] = (uint8)source->word[7];
        model_packet_u16(bytes, 14, texture.clut);
        bytes[20] = (uint8)source->word[8];
        bytes[21] = (uint8)source->word[9];
        model_packet_u16(bytes, 22, texture.tpage);
        if (triangle)
        {
            bytes[28] = (uint8)source->word[12];
            bytes[29] = (uint8)source->word[13];
        }
        else
        {
            bytes[28] = (uint8)source->word[10];
            bytes[29] = (uint8)source->word[11];
            bytes[36] = (uint8)source->word[12];
            bytes[37] = (uint8)source->word[13];
        }
        g_model_packet_count = (sint16)(g_model_packet_count + 1);
    }
    g_model_descriptors[index].half_x = model->descriptor_half_x;
    g_model_descriptors[index].half_y = model->descriptor_half_y;
    g_model_descriptors[index].half_z = model->descriptor_half_z;
    g_model_descriptors[index].collision = 1;
    if (models_count <= index)
        models_count = index + 1;
    g_current_model_world_x -= g_current_model_translation_x;
    g_current_model_world_z -= g_current_model_translation_z;
    return (void *)(data + at);
}

sint32 model_load(const void *source, sint32 size, sint32 count)
{
    const uint8 *cursor = (const uint8 *)source;
    sint32 index;
    if (!model_begin(source, size, count))
        return 0;
    for (index = 0; index < count; index++)
    {
        g_current_model_id = index;
        g_model_descriptors[index].model_data = cursor;
        cursor = (const uint8 *)model_parse_and_build_packets(0, 0, 0, 0, 0);
        if (cursor == 0)
            return 0;
    }
    if (getenv("OA_MODEL_TRACE"))
    {
        FILE *trace = fopen("model_descriptors_port.csv", "w");
        if (trace)
        {
            fprintf(trace, "model,half_x,half_y,half_z\n");
            for (index = 0; index < count; index++)
                fprintf(trace, "%d,%d,%d,%d\n", index, models[index].descriptor_half_x, models[index].descriptor_half_y, models[index].descriptor_half_z);
            fclose(trace);
        }
    }
    return 1;
}

/* Native form of FUN_80095300 for an already-created descriptor table. */
sint32 model_append(const void *source, sint32 size, sint32 count, sint32 material1, sint32 material2, sint32 material3)
{
    Model *grown;
    const uint8 *cursor = (const uint8 *)source;
    sint32 base = models_count, index;
    if (!source || size <= 0 || count <= 0 || g_model_descriptors == 0)
        return -1;
    if (base + count > models_capacity)
    {
        grown = (Model *)realloc(models, (uint32)(base + count + 1) * sizeof(*models));
        if (!grown)
            return -1;
        memset(grown + models_capacity, 0, (uint32)(base + count + 1 - models_capacity) * sizeof(*models));
        models = grown;
        models_capacity = base + count + 1;
    }
    model_parse_begin = (const uint8 *)source;
    model_parse_end = model_parse_begin + size;
    for (index = 0; index < count; index++)
    {
        g_current_model_id = base + index;
        g_model_descriptors[base + index].model_data = cursor;
        cursor = (const uint8 *)model_parse_and_build_packets(material1, material2, material3, 0, 0);
        /* model_parse_and_build_packets already bounds every byte it reads.
         * Its MIPS return at 80093390 may legally be end+2 for the final
         * model, as in COMMON0/MINGYRO.CC. */
        if (cursor == 0)
            return -1;
    }
    return base;
}

sint32 model_dump_parser_audit(const char *path)
{
    FILE *file;
    sint32 source_size, packet_count;
    static const uint8 magic[4] = {'M', 'W', 'A', '1'};
    if (path == 0 || model_parse_begin == 0 || model_parse_end < model_parse_begin)
        return 0;
    source_size = (sint32)(model_parse_end - model_parse_begin);
    packet_count = (sint32)(uint16)g_model_packet_count;
    if (packet_count > 0x226)
        packet_count = 0x226;
    file = fopen(path, "wb");
    if (file == 0)
        return 0;
    fwrite(magic, 1, 4, file);
    fwrite(&source_size, 4, 1, file);
    fwrite(&models_count, 4, 1, file);
    fwrite(&packet_count, 4, 1, file);
    fwrite(model_parse_begin, 1, (uint32)source_size, file);
    fwrite(model_packet_templates, 40, (uint32)packet_count, file);
    fclose(file);
    return 1;
}

static sint32 vi(sint16 byte_offset, sint32 count)
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
    sint32 shade = instance->light - model_cell_light_offset;
    if (shade < 0)
        shade = 0;
    if (shade > 255)
        shade = 255;
    return shade;
}

/* GTE NCLIP result used by FUN_800914D8. The original submits both FT3 and
 * FT4 packets only when the signed area of their first three vertices is
 * non-negative. Keep the arithmetic in 64 bits on the host, then compare the
 * sign; PSX screen coordinates are 16-bit so the MIPS 32-bit result cannot
 * overflow for this input range. */
static sint32 model_front_facing(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2)
{
    sint64 area = (sint64)x0 * y1 + (sint64)x1 * y2 + (sint64)x2 * y0 - (sint64)x0 * y2 - (sint64)x1 * y0 - (sint64)x2 * y1;
    return area >= 0;
}

static void project_model_vertex(sint32 base_x, sint32 base_y, sint32 base_z, sint32 x, sint32 y, sint32 z, sint32 rot_y, sint32 rot_x, sint32 rot_z, sint32 *sx, sint32 *sy, sint32 *raw_z)
{
    sint32 rx, ry, rz, depth;
    MATRIX m;
    SVECTOR in;
    VECTOR out;
    SVECTOR rotation;
    rotation.vx = (sint16)rot_x;
    rotation.vy = (sint16)rot_y;
    rotation.vz = (sint16)rot_z;
    rotation.pad = 0;
    in.vx = (sint16)x;
    in.vy = (sint16)y;
    in.vz = (sint16)z;
    in.pad = 0;
    RotMatrixYXZ(&rotation, &m);
    ApplyMatrix(&m, &in, &out);
    rx = (sint32)out.vx;
    ry = (sint32)out.vy;
    rz = (sint32)out.vz;
    *raw_z = base_z + rz;
    depth = *raw_z;
    /* Literal FUN_800914D8/FUN_800924EC behavior: only the divisor is
       clamped. The unclamped transformed Z remains in the OT-depth array. */
    if (depth < 1)
        depth = 8;
    *sx = (base_x + rx) * 0x140 / depth;
    *sy = (base_y + ry) * 0x163 / depth;
}

static void draw_model(sint32 id, sint32 wx, sint32 wy, sint32 wz, sint32 depth_reference_z, sint32 rot_y, sint32 rot_x, sint32 rot_z, sint32 row_bucket, sint32 shade)
{
    Model *m;
    sint32 *sx, *sy, *vz;
    sint32 i, base_x, base_y, base_z, visibility_depth, screen_bottom;
    if (id < 0 || id >= models_count)
        return;
    m = &models[id];
    psx_set_model_packet_context(id, wx, wy, wz, rot_y, rot_x, rot_z);
    /* 800914F0..80091578: reject a model whose lower descriptor edge is below
    the map draw area. The denominator deliberately uses cameraZ-0x4000. */
    visibility_depth = depth_reference_z - (g_camera_world_z - 0x4000);
    if (g_model_vertical_cull_override == 0 && visibility_depth != 0)
    {
        screen_bottom = ((wy + m->descriptor_half_y * 0x100 - g_camera_world_y) * 0x163) / visibility_depth;
        if (screen_bottom > 0x40 + g_screen_height / 2)
            return;
    }
    if (g_model_descriptors && g_model_descriptors[id].special != 0)
        return;
    sx = (sint32 *)malloc((sint32)m->vertex_count * sizeof(sint32));
    sy = (sint32 *)malloc((sint32)m->vertex_count * sizeof(sint32));
    vz = (sint32 *)malloc((sint32)m->vertex_count * sizeof(sint32));
    if (!sx || !sy || !vz)
    {
        free(sx);
        free(sy);
        free(vz);
        return;
    }
    /* MIPS adds 0xff before >>8 for negative translations: truncation toward
    zero from the game's 24.8 world coordinates. */
    base_x = (wx - g_camera_world_x) / 0x100;
    base_y = (wy - g_camera_world_y) / 0x100;
    base_z = (wz - g_camera_world_z) / 0x100;
    for (i = 0; i < m->vertex_count; i++)
        project_model_vertex(base_x, base_y, base_z, m->vertices[i].x, m->vertices[i].y, m->vertices[i].z, rot_y, rot_x, rot_z, &sx[i], &sy[i], &vz[i]);
    /* FUN_800914D8 80091F74..8009202C and 80091F7C..8009204C read
    gp+0x3ec directly for every FT3/FT4 packet.  Per-polygon maximum GTE Z
    belongs to the separate FUN_800924EC path and must not be applied to the
    CELLSDAT/runtime models rendered here. */
    for (i = 0; i < m->polygon_count; i++)
    {
        const PackedPolygon *p = &m->polygons[i];
        sint32 packet_index = (sint32)(uint16)p->word[0];
        const uint8 *packet = packet_index < 0x226 ? model_packet_templates[packet_index].bytes : 0;
        sint32 v0, v1, v2, v3, polygon_shade = shade - 0x80 + p->word[1];
        if (polygon_shade < 0)
            polygon_shade = 0;
        if (polygon_shade > 255)
            polygon_shade = 255;
        if (p->word[2] == -1)
        {
            v0 = vi(p->word[3], m->vertex_count);
            v1 = vi(p->word[4], m->vertex_count);
            v2 = vi(p->word[5], m->vertex_count);
            if (v0 >= 0 && v1 >= 0 && v2 >= 0 && model_front_facing(sx[v0], sy[v0], sx[v1], sy[v1], sx[v2], sy[v2]))
            {
                psx_set_prim_ot_bucket(row_bucket);
                psx_submit_prepared_model_ft3(sx[v0], sy[v0], sx[v1], sy[v1], sx[v2], sy[v2], packet, polygon_shade);
            }
        }
        else
        {
            v0 = vi(p->word[2], m->vertex_count);
            v1 = vi(p->word[3], m->vertex_count);
            v2 = vi(p->word[4], m->vertex_count);
            v3 = vi(p->word[5], m->vertex_count);
            if (v0 >= 0 && v1 >= 0 && v2 >= 0 && v3 >= 0 && model_front_facing(sx[v0], sy[v0], sx[v1], sy[v1], sx[v2], sy[v2]))
            {
                psx_set_prim_ot_bucket(row_bucket);
                psx_submit_prepared_model_ft4(sx[v0], sy[v0], sx[v1], sy[v1], sx[v2], sy[v2], sx[v3], sy[v3], packet, polygon_shade);
            }
        }
    }
    free(sx);
    free(sy);
    free(vz);
}

/* 0x800914D8 consumes the global model context prepared by 0x80094548.
 * DAT_800D4104/DAT_800D4118 are translation offsets; DAT_800D4204 is the
 * model's Y rotation.  The row base in DAT_800D3F08 remains the visibility
 * depth reference exactly as in the original GTE path. */
/* Original: FUN_800914D8. */
void model_render(void)
{
    draw_model(g_current_model_id, g_current_model_world_x + g_current_model_translation_x, g_current_model_world_y, g_current_model_world_z + g_current_model_translation_z, g_current_model_world_z, g_current_model_rotation_y, g_current_model_rotation_x, g_current_model_rotation_z, g_render_depth_bucket, g_current_model_shade);
}

/* Exact 0x80094518 light increment and saturation. */
/* Original: FUN_80094518. */
void model_cell_light_boost_apply(const MAP_FLOOR_CELL *cell)
{
    g_current_model_shade += (sint32)(cell->packed_orientation >> 3) * 0x10;
    if (g_current_model_shade > 0xff)
        g_current_model_shade = 0xff;
}

void model_draw_runtime_model(sint32 id, sint32 x, sint32 y, sint32 z, sint32 rot_y, sint32 rot_x, sint32 rot_z, sint32 row_bucket)
{
    sint32 half = 0, save_id = g_current_model_id, save_x = g_current_model_world_x, save_y = g_current_model_world_y, save_z = g_current_model_world_z, save_tx = g_current_model_translation_x, save_tz = g_current_model_translation_z, save_ry = g_current_model_rotation_y, save_rx = g_current_model_rotation_x, save_rz = g_current_model_rotation_z, save_shade = g_current_model_shade;
    if (id >= 0 && id < models_count)
        half = models[id].descriptor_half_y;
    psx_ot_trace_model(row_bucket, id, x, y + half * 0x100, z);
    g_current_model_id = id;
    g_current_model_world_x = x;
    g_current_model_world_y = y + half * 0x100;
    g_current_model_world_z = z;
    g_current_model_translation_x = 0;
    g_current_model_translation_z = 0;
    g_current_model_rotation_y = rot_y;
    g_current_model_rotation_x = rot_x;
    g_current_model_rotation_z = rot_z;
    g_current_model_shade = psx_get_object_shade();
    model_render();
    g_current_model_id = save_id;
    g_current_model_world_x = save_x;
    g_current_model_world_y = save_y;
    g_current_model_world_z = save_z;
    g_current_model_translation_x = save_tx;
    g_current_model_translation_z = save_tz;
    g_current_model_rotation_y = save_ry;
    g_current_model_rotation_x = save_rx;
    g_current_model_rotation_z = save_rz;
    g_current_model_shade = save_shade;
}

static void submit_cell_item(CellRenderItem *item, sint32 cell_x, sint32 cell_z, sint32 row_bucket, sint32 ignore_drawn)
{
    if (item->drawn && !ignore_drawn)
        return;
    if (item->special)
    {
        const MAP_MODEL_INSTANCE *p = item->instance;
        sint32 id = p->model_id, wz = g_map_depth_cells * 0xc000 - (cell_z + 1) * 0x4000 + p->local_z;
        FrameObjectPartial logical; /* FUN_80094548 creates this temporary record and FUN_8008DDEC consumes it. */
        memset(&logical, 0, sizeof(logical));
        logical.world_x = cell_x * 0x4000 + p->local_x;
        logical.world_y = item->y;
        logical.world_z = wz;
        logical.ot_bucket = (sint16)row_bucket;
        logical.model_plus_one = id + 1;
        logical.model_rot_y = p->yaw;
        logical.light_delta = (sint16)((uint8)((const sint16 *)p)[-1] - 0x80);
        psx_set_object_shade(current_model_shade);
        frame_model_submit(&logical);
    }
    else
    {
        psx_set_object_shade(current_model_shade);
        frame_model_submit(item->object);
    }
}

static void submit_static_instance(const MAP_MODEL_INSTANCE *p, sint32 cell_x, sint32 cell_z, sint32 row_bucket)
{
    sint32 base_x = cell_x * 0x4000, base_z = g_map_depth_cells * 0xc000 - (cell_z + 1) * 0x4000;
    current_model_shade = model_instance_shade(p);
    g_current_model_id = p->model_id;
    g_current_model_world_x = base_x;
    g_current_model_world_y = p->y * 0x100;
    g_current_model_world_z = base_z;
    g_current_model_translation_x = p->local_x;
    g_current_model_translation_z = p->local_z;
    g_current_model_rotation_y = p->yaw;
    g_current_model_rotation_x = 0;
    g_current_model_rotation_z = 0;
    g_current_model_shade = current_model_shade;
    if (model_cell && model_cell_light_boost != 0)
        model_cell_light_boost_apply(model_cell);
    current_model_shade = g_current_model_shade;
    psx_ot_trace_model(row_bucket, p->model_id, base_x, g_current_model_world_y, base_z);
    model_render();
}

void model_draw_cell(const MAP_MODEL_GROUP *g, sint32 cell_x, sint32 cell_z, sint32 row_bucket)
{
    CellRenderItem items[SPATIAL_BUCKET_CAPACITY + 64];
    DeferredCellModel deferred[64];
    FrameObjectPartial *objects[SPATIAL_BUCKET_CAPACITY];
    sint32 count = 0, special_count = 0, item_count = 0, deferred_count = 0, visible_model_seen = 0, i, j;
    sint32 row_z = g_map_depth_cells * 0xc000 - (cell_z + 1) * 0x4000;
    MAP_FLOOR_CELL *cell = g_map_floor_cells + cell_z * g_map_width_cells + cell_x;
    sint32 map_light = g_map_cell_light_values ? g_map_cell_light_values[cell_z * g_map_width_cells + cell_x] : 0;
    if (!g || !models)
        return;
    model_cell = cell;
    model_cell_light_offset = g_depth_lighting_bias - map_light - g_scene_brightness_bias;
    model_cell_light_boost = (cell->packed_orientation & 0xf8) ? (cell->packed_orientation >> 3) * 0x10 : 0;
    g_model_vertical_cull_override = 0;
    current_model_shade = ((sint8)cell->material == -1 ? 0x80 : cell->field_05) - model_cell_light_offset;
    if (current_model_shade < 0)
        current_model_shade = 0;
    if (current_model_shade > 255)
        current_model_shade = 255;
    count = g->active_count;
    special_count = g->removed_count;
    if (count < 0 || count > 64)
        count = 0;
    if (special_count < 0 || special_count > 64)
        special_count = 0;
    i = psx_get_cell_frame_objects(cell_x, cell_z, objects);
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
        const MAP_MODEL_INSTANCE *p = (const MAP_MODEL_INSTANCE *)(g + 1) + count + i;
        sint32 id = p->model_id, half = 0, y, z, den, screen;
        if (id >= 0 && id < models_count)
            half = models[id].descriptor_half_y;
        y = ((sint32)p->y - half - 5000) * 0x100;
        z = row_z + p->local_z;
        den = row_z - (g_camera_world_z - 0x4000);
        if (den == 0)
            continue;
        screen = ((y - g_camera_world_y) * 0x163) / den; /* 80094890..80094910: special records enter the common list only when their projected Y is below the top clipping threshold. */
        if (screen <= 0x40 - g_screen_height / 2)
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
                    CellRenderItem t = items[i];
                    items[i] = items[i + 1];
                    items[i + 1] = t;
                    swapped = 1;
                }
        } while (swapped);
    }
    for (i = 0; i < count; i++)
    {
        const MAP_MODEL_INSTANCE *p = (const MAP_MODEL_INSTANCE *)(g + 1) + i;
        sint32 id = p->model_id, half = 0, model_y = p->y * 0x100, top_edge, bottom_edge, depth, screen_top, screen_bottom;
        if (model_y == 0x4e2000)
            continue;
        if (id >= 0 && id < models_count)
            half = models[id].descriptor_half_y;
        /* Descriptor +8 is (minY-maxY)/2 and therefore non-positive.  These two
   * expressions are a literal translation of 80094a44 and 80094b88. */
        top_edge = model_y - half * 0x100;
        bottom_edge = model_y + half * 0x100;
        depth = row_z - (g_camera_world_z - 0x4000);
        if (depth == 0)
            continue;
        screen_top = ((top_edge - g_camera_world_y) * 0x163) / depth;
        if (!visible_model_seen && screen_top <= 0x40 - g_screen_height / 2)
            continue;
        visible_model_seen = 1;
        depth = row_z - g_camera_world_z;
        if (depth == 0)
            continue;
        /* 80094B2C..80094B80 loads the current static instance's byte-10
   * brightness into gp+0x2A4 before either ordering branch.  Objects emitted
   * before an on-screen model therefore inherit that model-layer shade; the
   * assignment must not be delayed until submit_static_instance(). */
        current_model_shade = model_instance_shade(p);
        screen_top = ((top_edge - g_camera_world_y) * 0x163) / depth;
        if (screen_top < 0)
        {
            DeferredCellModel *d;
            if (deferred_count >= 64)
                continue;
            d = &deferred[deferred_count++];
            d->instance = p;
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
        screen_bottom = ((bottom_edge - g_camera_world_y) * 0x163) / depth;
        if (screen_bottom < 0)
        {
            /* FUN_80094548 80094D1C..80094E38: when the model's bottom edge is
      above the draw origin, submit the model before the eligible objects. */
            submit_static_instance(p, cell_x, cell_z, row_bucket);
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
            submit_static_instance(p, cell_x, cell_z, row_bucket);
        }
    }
    current_model_shade = ((sint8)cell->material == -1 ? 0x80 : cell->field_05) - model_cell_light_offset;
    if (current_model_shade < 0)
        current_model_shade = 0;
    if (current_model_shade > 255)
        current_model_shade = 255;
    for (i = 0; i < item_count; i++)
        submit_cell_item(&items[i], cell_x, cell_z, row_bucket, 0);
    g_model_vertical_cull_override = 1;
    for (i = deferred_count - 1; i >= 0; i--)
    {
        DeferredCellModel *d = &deferred[i];
        submit_static_instance(d->instance, cell_x, cell_z, row_bucket);
        for (j = d->item_count - 1; j >= 0; j--)
            submit_cell_item(&items[d->item[j]], cell_x, cell_z, row_bucket, 1);
    }
    if ((cell->packed_orientation & 0xf8) != 0)
        cell->packed_orientation = (uint8)(cell->packed_orientation - 8);
    model_cell = 0;
}

static void submit_hierarchy_polygon(const HierarchyPolygon *pending, sint32 bucket)
{
    sint32 packet_index = (sint32)(uint16)pending->polygon->word[0];
    const uint8 *packet = packet_index < 0x226 ? model_packet_templates[packet_index].bytes : 0;
    psx_set_model_clut_override((uint16)g_model_clut_override);
    psx_set_hierarchy_packet_trace(1);
    psx_set_prim_ot_bucket(bucket);
    psx_set_prim_depth(pending->depth);
    psx_set_prim_screen_offset(hierarchy_offset_x, hierarchy_offset_y);
    if (pending->triangle)
        psx_submit_prepared_model_ft3(pending->sx[0], pending->sy[0], pending->sx[1], pending->sy[1], pending->sx[2], pending->sy[2], packet, 0x80);
    else
        psx_submit_prepared_model_ft4(pending->sx[0], pending->sy[0], pending->sx[1], pending->sy[1], pending->sx[2], pending->sy[2], pending->sx[3], pending->sy[3], packet, 0x80);
    psx_set_hierarchy_packet_trace(0);
    psx_set_model_clut_override(0);
}

void model_set_screen_offset(sint32 x, sint32 y)
{
    hierarchy_offset_x = x;
    hierarchy_offset_y = y;
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
    MODEL_NODE *next = (MODEL_NODE *)node->next_sibling;
    MODEL_NODE *previous = (MODEL_NODE *)node->previous_sibling;
    if (next != 0)
        next->previous_sibling = previous;
    if (previous != 0)
        previous->next_sibling = next;
    else
    {
        MODEL_NODE *parent = (MODEL_NODE *)node->parent;
        parent->first_child = next;
        node->parent = 0;
        node->previous_sibling = 0;
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
    child = (MODEL_NODE *)parent->first_child;
    node->next_sibling = child;
    if (child != 0)
        child->previous_sibling = node;
    node->previous_sibling = 0;
    parent->first_child = node;
}

/* Exact FUN_800AC128 translation, 0x800AC128..0x800AC268. The resource starts
 * with a count followed by {pose_offset,parent_index} records. */
void model_initialize_pose(const void *raw_resource, MODEL_NODE **nodes, sint32 pose)
{
    const uint8 *resource = (const uint8 *)raw_resource;
    sint32 count = *(const sint32 *)resource;
    sint32 index;
    sint32 pose_stride = (pose * 3) << 2;
    for (index = 0; index < count; index++)
    {
        const sint32 *record = (const sint32 *)(resource + 8 + index * 8);
        MODEL_NODE *node = nodes[index];
        if (record[1] >= 0)
            model_attach_child(node, nodes[record[1]]);
        if (pose >= 0)
        {
            const sint16 *value = (const sint16 *)(resource + record[0] + pose_stride);
            node->rotation_x = (sint16)(-((sint16)value[0] >> 4) & 0x0fff);
            node->rotation_y = (sint16)((uint16)value[1] >> 4);
            node->rotation_z = (sint16)((uint16)value[2] >> 4);
            node->translation_x = (sint32)value[3] << 4;
            node->translation_y = -((sint32)value[4] << 4);
            node->translation_z = (sint32)value[5] << 4;
        }
    }
}

/* Exact FUN_800AC26C, 0x800AC26C..0x800AC3B4. Unlike FUN_800AC128 this applies
 * pose to an already-linked hierarchy. */
void model_apply_pose(const void *raw_resource, MODEL_NODE **nodes, sint32 pose)
{
    const uint8 *resource = (const uint8 *)raw_resource;
    sint32 count = *(const sint32 *)resource, index;
    sint32 pose_stride = (pose * 3) << 2;
    for (index = 0; index < count; index++)
    {
        sint32 offset = *(const sint32 *)(resource + 8 + index * 8);
        MODEL_NODE *node = nodes[index];
        const sint16 *value = (const sint16 *)(resource + offset + pose_stride);
        if (node->render_flags == 0)
            continue;
        node->rotation_x = (sint16)(-((sint16)value[0] >> 4) & 0x0fff);
        node->rotation_y = (sint16)((uint16)value[1] >> 4);
        node->rotation_z = (sint16)((uint16)value[2] >> 4);
        node->translation_x = (sint32)value[3] << 4;
        node->translation_y = -((sint32)value[4] << 4);
        node->translation_z = (sint32)value[5] << 4;
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
    if (g_stage_index == 4)
        RotMatrix(&rotation, matrix);
    else
        RotMatrixYXZ(&rotation, matrix);
    matrix->t[0] = node->translation_x;
    matrix->t[1] = node->translation_y;
    matrix->t[2] = node->translation_z;
}

/* 0x800924EC..0x80092CEC.  Packet material construction remains in the
 * native GPU wrapper, but every tested branch, cull, vertex transform,
 * NCLIP decision and per-polygon maximum-Z key follows the MIPS routine. */
/* Original: FUN_800924EC. */
static void model_queue_mesh(MATRIX *matrix, void *main_ot, MODEL_NODE *node)
{
    Model *model;
    sint32 *sx, *sy, *depths;
    sint32 center_depth, screen, model_id, i;
    MATRIX projected;
    center_depth = g_current_model_world_z - g_camera_world_z;
    if (center_depth <= 0 || frame_render_work_enabled() == 0)
        return;
    model_id = g_current_model_id;
    if (model_id < 0 || model_id >= models_count)
        return;
    model = &models[model_id];
    screen = (node->world_y + model->descriptor_half_y * 0x100 - g_camera_world_y) * 0x163 / center_depth;
    if (screen >= 0xbf)
        return;
    screen = (node->world_y - model->descriptor_half_y * 0x100 - g_camera_world_y) * 0x163 / center_depth;
    if (screen <= -0xbf)
        return;
    screen = (node->world_x - g_camera_world_x) * 0x140 / center_depth;
    if ((uint32)(screen + 0x140) >= 0x281u)
        return;
    if (g_current_model_world_z < g_camera_world_z + 0x8000)
        return;

    projected = *matrix;
    projected.t[0] = div_256_trunc(projected.t[0]);
    projected.t[1] = div_256_trunc(projected.t[1]);
    projected.t[2] = div_256_trunc(projected.t[2]);
    sx = (sint32 *)malloc((sint32)model->vertex_count * sizeof(*sx));
    sy = (sint32 *)malloc((sint32)model->vertex_count * sizeof(*sy));
    depths = (sint32 *)malloc((sint32)model->vertex_count * sizeof(*depths));
    if (!sx || !sy || !depths)
    {
        free(sx);
        free(sy);
        free(depths);
        return;
    }
    for (i = 0; i < model->vertex_count; i++)
    {
        VECTOR out;
        sint32 z;
        SVECTOR in;
        in.vx = model->vertices[i].x;
        in.vy = model->vertices[i].y;
        in.vz = model->vertices[i].z;
        in.pad = 0;
        ApplyMatrix(&projected, &in, &out);
        out.vx += projected.t[0];
        out.vy += projected.t[1];
        out.vz += projected.t[2];
        z = out.vz;
        depths[i] = z;
        if (z < 1)
            z = 8;
        sx[i] = out.vx * 0x140 / z;
        sy[i] = out.vy * 0x163 / z;
    }
    for (i = 0; i < model->polygon_count && (main_ot != 0 || hierarchy_polygon_count < 1024); i++)
    {
        const PackedPolygon *polygon = &model->polygons[i];
        HierarchyPolygon direct;
        HierarchyPolygon *pending;
        sint32 index[4], count, j, max_z;
        if (polygon->word[2] == -1)
        {
            index[0] = vi(polygon->word[3], model->vertex_count);
            index[1] = vi(polygon->word[4], model->vertex_count);
            index[2] = vi(polygon->word[5], model->vertex_count);
            count = 3;
        }
        else
        {
            index[0] = vi(polygon->word[2], model->vertex_count);
            index[1] = vi(polygon->word[3], model->vertex_count);
            index[2] = vi(polygon->word[4], model->vertex_count);
            index[3] = vi(polygon->word[5], model->vertex_count);
            count = 4;
        }
        for (j = 0; j < count; j++)
            if (index[j] < 0)
                break;
        if (j != count || !model_front_facing(sx[index[0]], sy[index[0]], sx[index[1]], sy[index[1]], sx[index[2]], sy[index[2]]))
            continue;
        max_z = depths[index[0]];
        for (j = 1; j < count; j++)
            if (depths[index[j]] > max_z)
                max_z = depths[index[j]];
        pending = main_ot != 0 ? &direct : &hierarchy_polygon[hierarchy_polygon_count];
        memset(pending, 0, sizeof(*pending));
        pending->polygon = polygon;
        pending->depth = max_z;
        pending->sequence = hierarchy_polygon_count;
        pending->triangle = count == 3;
        for (j = 0; j < count; j++)
        {
            pending->vertex[j] = index[j];
            pending->sx[j] = sx[index[j]];
            pending->sy[j] = sy[index[j]];
        }
        /* 80092728..80092750 selects the caller OT when a1 != 0.
         * 80092C58..80092C9C links each accepted polygon immediately at
         * base - (worldZ/256 - DAT_800D3F3C) + maximum transformed Z.
         * V2 uses this path and never calls the private-OT splice 9242C. */
        if (main_ot != 0)
            submit_hierarchy_polygon(pending, g_render_depth_bucket - (div_256_trunc(g_current_model_world_z) - g_camera_render_z) + max_z);
        else
            hierarchy_polygon_count++;
    }
    free(sx);
    free(sy);
    free(depths);
}

/* 0x80092194..0x800923A0. */
/* Original: FUN_80092194. */
void model_render_node(MODEL_NODE *node, MATRIX *parent)
{
    MATRIX world;
    VECTOR local_translation, scaled;
    MODEL_NODE *child, *next;
    sint32 model_id;
    if (!node || !parent || g_render_depth_bucket >= 0x3e9)
        return;
    model_build_local_matrix(node);
    MulMatrix0(parent, &node->local_matrix, &world);
    local_translation.vx = node->local_matrix.t[0];
    local_translation.vy = node->local_matrix.t[1];
    local_translation.vz = node->local_matrix.t[2];
    local_translation.pad = 0;
    ApplyMatrixLV(parent, &local_translation, &scaled);
    world.t[0] = scaled.vx + parent->t[0];
    world.t[1] = scaled.vy + parent->t[1];
    world.t[2] = scaled.vz + parent->t[2];
    if (node->scale != 0)
    {
        scaled.vx = scaled.vy = scaled.vz = node->scale;
        ScaleMatrix(&world, &scaled);
    }
    node->world_x = world.t[0] + g_camera_world_x;
    node->world_y = world.t[1] + g_camera_world_y;
    node->world_z = world.t[2] + g_camera_world_z;
    model_id = node->model_id;
    g_current_model_id = model_id;
    if (model_id != 0)
        model_queue_mesh(&world, g_model_render_frame, node);
    if (node->parent == 0)
    {
        node->world_rotation_x = node->rotation_x;
        node->world_rotation_y = node->rotation_y;
        node->world_rotation_z = node->rotation_z;
    }
    else
    {
        MODEL_NODE *owner = (MODEL_NODE *)node->parent;
        node->world_rotation_x = (sint16)((node->rotation_x + owner->world_rotation_x) & 0xfff);
        node->world_rotation_y = (sint16)((node->rotation_y + owner->world_rotation_y) & 0xfff);
        node->world_rotation_z = (sint16)((node->rotation_z + owner->world_rotation_z) & 0xfff);
    }
    child = (MODEL_NODE *)node->first_child;
    while (child)
    {
        next = (MODEL_NODE *)child->next_sibling;
        model_render_node(child, &world);
        child = next;
    }
}

static sint32 hierarchy_less(const HierarchyPolygon *a, const HierarchyPolygon *b)
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
    sint32 i, j;
    for (i = 1; i < hierarchy_polygon_count; i++)
    {
        HierarchyPolygon value = hierarchy_polygon[i];
        j = i;
        while (j > 0 && hierarchy_less(&value, &hierarchy_polygon[j - 1]))
        {
            hierarchy_polygon[j] = hierarchy_polygon[j - 1];
            j--;
        }
        hierarchy_polygon[j] = value;
    }
    psx_set_model_clut_override((uint16)g_model_clut_override);
    psx_set_hierarchy_packet_trace(1);
    for (i = 0; i < hierarchy_polygon_count; i++)
    {
        HierarchyPolygon *pending = &hierarchy_polygon[i];
        const PackedPolygon *polygon = pending->polygon;
        sint32 packet_index = (sint32)(uint16)polygon->word[0];
        const uint8 *packet = packet_index < 0x226 ? model_packet_templates[packet_index].bytes : 0;
        psx_set_prim_ot_bucket(g_render_depth_bucket);
        psx_set_prim_depth(pending->depth);
        psx_set_prim_screen_offset(hierarchy_offset_x, hierarchy_offset_y);
        if (pending->triangle)
            psx_submit_prepared_model_ft3(pending->sx[0], pending->sy[0], pending->sx[1], pending->sy[1], pending->sx[2], pending->sy[2], packet, 0x80);
        else
            psx_submit_prepared_model_ft4(pending->sx[0], pending->sy[0], pending->sx[1], pending->sy[1], pending->sx[2], pending->sy[2], pending->sx[3], pending->sy[3], packet, 0x80);
    }
    psx_set_hierarchy_packet_trace(0);
    psx_set_model_clut_override(0);
    hierarchy_polygon_count = 0;
}
