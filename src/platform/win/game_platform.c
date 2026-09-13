#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "audio/psyq_sound.h"
#include "cc_archive.h"
#include "code_module.h"
#include "game_platform.h"
#include "global.h"
#include "level_data.h"
#include "model.h"
#include "object.h"
#include "original_file.h"
#include "platform_file.h"
#include "player.h"
#include "psx.h"
#include "render.h"
#include "resource_table.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "stubs.h"
#include "windows_compat.h"

/* Types. */
typedef struct
{
    uint32 magic;
    uint32 flags;
} TIM_FILE_HEADER;

typedef struct
{
    uint32 size;
    PSX_RECT rectangle;
    uint16 pixels[1];
} TIM_DATA_BLOCK;

#if defined(AP_32BIT)
typedef char PolyFt3_size_20[sizeof(POLY_FT3) == 0x20 ? 1 : -1];
typedef char PolyFt4_size_28[sizeof(POLY_FT4) == 0x28 ? 1 : -1];
typedef char PolyFt4_clut_0e[offsetof(POLY_FT4, clut) == 0x0e ? 1 : -1];
typedef char PolyFt4_tpage_16[offsetof(POLY_FT4, tpage) == 0x16 ? 1 : -1];
typedef char TimFileHeader_size_08[sizeof(TIM_FILE_HEADER) == 0x08 ? 1 : -1];
typedef char TimDataBlock_rect_04[offsetof(TIM_DATA_BLOCK, rectangle) == 0x04 ? 1 : -1];
typedef char TimDataBlock_pixels_0c[offsetof(TIM_DATA_BLOCK, pixels) == 0x0c ? 1 : -1];
#endif

typedef struct MapTextureSlot
{
    uint16 tpage, clut;
    uint8 u, v, valid;
} MapTextureSlot;

typedef struct NativeOtEntry
{
    void *ot;
    void *prim;
    sint16 clip_x0, clip_x1, clip_y0, clip_y1;
    sint16 offset_x, offset_y;
    uint8 kind;
    FrameObjectPartial *owner;
    sint32 next;
} NativeOtEntry;

typedef struct RV
{
    sint32 x, y, u, v;
} RV;

typedef struct RasterScan
{
    sint32 minx, maxx, miny, maxy;
    sint64 e0, e1, e2;
    sint64 e0dx, e1dx, e2dx, e0dy, e1dy, e2dy;
    sint64 u, v, r, g, b;
    sint64 udx, vdx, rdx, gdx, bdx;
    sint64 udy, vdy, rdy, gdy, bdy;
} RasterScan;

typedef struct
{
    const uint8 *texinfo;
    uint32 texinfo_size;
    sint32 loaded_materials;
} RESOURCE_SCRIPT_CONTEXT;

typedef struct
{
    char name[16];   /* +0x00 */
    uint32 resource; /* +0x10 */
    sint16 frames;   /* +0x14 */
    uint16 field_16; /* +0x16 */
} TEXTURE_RESOURCE_RECORD;

typedef char TextureResourceRecord_size_18[sizeof(TEXTURE_RESOURCE_RECORD) == 0x18 ? 1 : -1];

typedef struct PendingStageModels
{
    uint8 *data;
    sint32 size, count, material1, material2, material3;
    sint16 *base;
} PendingStageModels;

/* Macros. */
#define FW 320

#define FH 256

#define VRAM_W 1024

#define VRAM_H 512

#define MODEL_TEXTURE_SLOT_COUNT 0x11C

/* Variables. */
static uint32 fb[FW * FH], present_fb[FW * FH], menu_background[FW * FH];

static sint32 frame_number;

static sint32 menu_background_active;

static sint32 native_headless, smoke_frame_limit;

static sint32 smoke_wait_for_mission, smoke_mission_started;

static sint32 display_offset_y;

static sint32 native_music_id = -1, native_music_volume;

static uint16 vram[VRAM_W * VRAM_H];

static uint16 active_tpage;

static uint32 texture_window;

static uint8 map_prims[8192][40];

static sint32 map_prim_count;

static MapTextureSlot map_textures[64];

static MapTextureSlot model_textures[MODEL_TEXTURE_SLOT_COUNT];

static uint8 *hqtex_archive;

static NativeOtEntry native_ot[16384];

static sint32 native_ot_count;

static sint32 native_ot_heads[0x481];

static FrameObjectPartial *native_ot_owner;

static sint32 prim_world_depth = -1;

static sint32 prim_ot_bucket = -1;

static sint32 prim_screen_offset_x, prim_screen_offset_y;

static sint32 trace_cell_x = -1, trace_cell_z = -1;

static const MAP_FLOOR_CELL *trace_cell;

static sint32 packet_model_id = -1, packet_model_x, packet_model_y, packet_model_z, packet_model_ry, packet_model_rx, packet_model_rz;

static uint16 model_clut_override;

static sint32 hierarchy_packet_trace_active;

static sint32 raster_clip_x0 = 0, raster_clip_x1 = FW, raster_clip_y0 = 0, raster_clip_y1 = FH;

static FILE *logical_ot_trace;

static sint32 logical_ot_done, logical_ot_seq;

static sint32 cell_sprite_light = 0x80;

static sint32 raster_offset_x, raster_offset_y;

/* Full control flow of FUN_800AB478.  Script words, archive paths and texture
 * descriptors are read from the original SLES tables. */
static RESOURCE_SCRIPT_CONTEXT *resource_script_context;

static PendingStageModels pending_stage_models[8];

static sint32 pending_stage_models_count;

static const uint8 *resource_script_direct;

static sint32 resource_script_direct_size;

/* Functions. */
static void game_gpu_add_prim(void *user, void *ot, void *prim);
static void game_gpu_add_prims(void *user, void *ot, void *first, void *last);
static sint32 game_gpu_clear_image(void *user, PSX_RECT *rectangle, uint8 red, uint8 green, uint8 blue);
static uint32 *game_gpu_clear_ot(void *user, uint32 *ot, sint32 count, sint32 reverse);
static void game_gpu_draw_ot(void *user, uint32 *ot);
static sint32 game_gpu_move_image(void *user, PSX_RECT *rectangle, sint32 x, sint32 y);
static sint32 game_gpu_store_image(void *user, PSX_RECT *rectangle, uint32 *pixels);

__declspec(dllexport) volatile uint32 g_psx_display_mode_boundary_calls;

/* Native side of the low-level libcd/device routines called by the recovered
 * wrappers at 0x800B82A4 and 0x800B8644.  Zero is the original driver's
 * success result.  The counters and result words expose this platform edge
 * to the debugger without changing the recovered wrapper control flow. */
__declspec(dllexport) sint32 g_psx_cd_read_track_table_result;

__declspec(dllexport) sint32 g_psx_cd_start_driver_result;

__declspec(dllexport) sint32 g_psx_cd_configure_driver_result;

__declspec(dllexport) uint32 g_psx_cd_read_track_table_calls;

__declspec(dllexport) uint32 g_psx_cd_start_driver_calls;

__declspec(dllexport) uint32 g_psx_cd_configure_driver_calls;

__declspec(dllexport) uint32 g_psx_cd_stop_driver_calls;

__declspec(dllexport) sint32 g_psx_cd_last_mode;

__declspec(dllexport) void *g_psx_cd_last_workspace;

__declspec(dllexport) uint32 g_psx_resource_opcode_counts[10];

static sint32 game_gpu_load_image(void *user, PSX_RECT *r, uint32 *p)
{
    sint32 x, y;
    uint16 *s = (uint16 *)p;
    (void)user;
    if (!r || !p)
        return -1;
    for (y = 0; y < r->h; y++)
        for (x = 0; x < r->w; x++)
            if ((uint32)(r->x + x) < VRAM_W && (uint32)(r->y + y) < VRAM_H)
                vram[(r->y + y) * VRAM_W + r->x + x] = *s++;
    return 0;
}

/* Original: FUN_8008FF98. */
GDB_CALL sint32 tim_image_upload(void *raw)
{
    const TIM_FILE_HEADER *tim = (const TIM_FILE_HEADER *)raw;
    const TIM_DATA_BLOCK *clut = 0, *image;
    const uint8 *data;
    PSX_RECT r;
    if (!tim || tim->magic != 0x10)
        return 0;
    data = (const uint8 *)(tim + 1);
    if (tim->flags & 8)
    {
        clut = (const TIM_DATA_BLOCK *)data;
        if (clut->size < 0x0c)
            return 0;
        data += clut->size;
    }
    image = (const TIM_DATA_BLOCK *)data;
    if (image->size < 0x0c)
        return 0;
    r = image->rectangle;
    r.x = g_render_frame_buffer_0.drawenv.clip.x;
    r.y = g_render_frame_buffer_0.drawenv.clip.y;
    LoadImage(&r, (uint32 *)image->pixels);
    DrawSync(0);
    r.x = g_render_frame_buffer_1.drawenv.clip.x;
    r.y = g_render_frame_buffer_1.drawenv.clip.y;
    LoadImage(&r, (uint32 *)image->pixels);
    DrawSync(0);
    if (clut)
    {
        r = clut->rectangle;
        LoadImage(&r, (uint32 *)clut->pixels);
        DrawSync(0);
    }
    return 1;
}

/* Original: FUN_800B3058. */
GDB_CALL void display_mask_set(sint32 mode)
{
    (void)mode;
    ++g_psx_display_mode_boundary_calls;
}

/* Original: FUN_8008B7A8.  The PAL body loads COMMON0/FMV.BIN and invokes its
 * STR player.  Native STR playback is an intentionally excluded platform
 * boundary, but retain an addressable no-op implementation for the recovered
 * ABI and debugger/call-database identity. */
GDB_CALL void str_video_play(char *str_path, sint32 width, sint32 frames, sint32 mode)
{
    (void)str_path;
    (void)width;
    (void)frames;
    (void)mode;
}

sint32 psx_cd_read_track_table(sint32 mode, void *workspace)
{
    ++g_psx_cd_read_track_table_calls;
    g_psx_cd_last_mode = mode;
    g_psx_cd_last_workspace = workspace;
    return g_psx_cd_read_track_table_result;
}

sint32 psx_cd_start_driver(void)
{
    ++g_psx_cd_start_driver_calls;
    return g_psx_cd_start_driver_result;
}

sint32 psx_cd_configure_driver(void)
{
    ++g_psx_cd_configure_driver_calls;
    return g_psx_cd_configure_driver_result;
}

void psx_cd_stop_driver(void)
{
    ++g_psx_cd_stop_driver_calls;
}

/* Host boundary for the PsyQ/libcd sync, track-range and volume command
 * sequence in FUN_800A99F0/FUN_800A9C24.  Native currently has no CD-DA
 * output device, but preserves the exact selected catalog entry and volume. */
void psx_select_music_track(sint32 id, sint32 volume)
{
    native_music_id = id;
    native_music_volume = volume;
    psyq_sound_music_play(id, volume);
}

static void dump_ot_trace_once(void)
{
    static sint32 done;
    const char *enabled = getenv("OA_OT_TRACE");
    FILE *f;
    sint32 i;
    if (done || !enabled || !*enabled || frame_number < 30 || !g_current_render_frame)
        return;
    done = 1;
    f = fopen("ot_trace.csv", "w");
    if (!f)
        return;
    fprintf(f, "submission,bucket,code,kind,world_x,world_y,world_z,x,y,u,v\n");
    for (i = 0; i < native_ot_count; i++)
    {
        ptrdiff_t delta = (uint8 *)native_ot[i].ot - (uint8 *)g_current_render_frame;
        sint32 bucket = (delta >= 0 && (delta & 3) == 0) ? (sint32)(delta >> 2) : -1;
        uint8 *p = (uint8 *)native_ot[i].prim;
        FrameObjectPartial *owner = native_ot[i].owner;
        const char *kind = native_ot[i].kind ? "env" : owner ? "frame" : "map";
        sint32 code = p ? p[7] : 0;
        fprintf(f, "%d,%d,%u,%s,%d,%d,%d,%d,%d,%u,%u\n", i, bucket, code, kind, owner ? owner->world_x : 0, owner ? owner->world_y : 0, owner ? owner->world_z : 0, p && ((code & 0xfc) == 0x64) ? *(sint16 *)(p + 8) : 0, p && ((code & 0xfc) == 0x64) ? *(sint16 *)(p + 10) : 0, p && ((code & 0xfc) == 0x64) ? p[12] : 0, p && ((code & 0xfc) == 0x64) ? p[13] : 0);
    }
    fclose(f);
}

void psx_set_model_packet_context(sint32 id, sint32 x, sint32 y, sint32 z, sint32 ry, sint32 rx, sint32 rz)
{
    packet_model_id = id;
    packet_model_x = x;
    packet_model_y = y;
    packet_model_z = z;
    packet_model_ry = ry;
    packet_model_rx = rx;
    packet_model_rz = rz;
}

void psx_set_model_clut_override(uint16 clut)
{
    model_clut_override = clut;
}

void psx_set_hierarchy_packet_trace(sint32 active)
{
    hierarchy_packet_trace_active = active;
}

static void trace_sprite_packet(const uint8 *p, const FrameObjectPartial *o)
{
    static sint32 count;
    FILE *f;
    sint32 i;
    sint32 ready = !getenv("OA_LOGICAL_OT_REFERENCE_CAMERA") || (g_camera_world_x == 0xc8000 && g_camera_world_y == (sint32)0xffff9100 && g_camera_world_z == 0x68000);
    if (!getenv("OA_SPRITE_PACKET_TRACE") || !ready || count >= 512 || !o)
        return;
    f = fopen("sprite_packets_port.log", count ? "a" : "w");
    if (!f)
        return;
    fprintf(f, "PACKET %d tick=%u bucket=%d cell=%d,%d rawlight=%u maplight=%u baseline=%d fade=%d xyz=%08x,%08x,%08x split=%d,%d light=%u,%u,%u code=%02x\n", count, g_frame_counter, (sint32)o->ot_bucket, trace_cell_x, trace_cell_z, trace_cell ? (uint32)trace_cell->field_05 : 0, trace_cell && g_map_cell_light_values ? (uint32)g_map_cell_light_values[trace_cell_z * g_map_width_cells + trace_cell_x] : 0, g_depth_lighting_bias, g_scene_brightness_bias, (uint32)o->world_x, (uint32)o->world_y, (uint32)o->world_z, (sint32)o->x_subcell_offset, (sint32)o->draw_env_height, (uint32)p[4], (uint32)p[5], (uint32)p[6], (uint32)p[7]);
    for (i = 0; i < 10; i++)
        fprintf(f, "%08x%c", ((const uint32 *)p)[i], i == 9 ? '\n' : ' ');
    fclose(f);
    count++;
}

void psx_ot_trace_begin(void)
{
    const char *e = getenv("OA_LOGICAL_OT_TRACE");
    if (getenv("OA_LOGICAL_OT_REFERENCE_CAMERA"))
    {
        g_camera_world_x = 0xc8000;
        g_camera_world_y = (sint32)0xffff9100;
        g_camera_world_z = 0x68000;
    }
    if (logical_ot_done || frame_number < 30 || !e || !*e)
        return;
    logical_ot_trace = fopen("ot_order_port.log", "w");
    logical_ot_seq = 0;
    if (logical_ot_trace)
        fprintf(logical_ot_trace, "FRAME camera=%#x,%#x,%#x\n", g_camera_world_x, g_camera_world_y, g_camera_world_z);
}

void psx_ot_trace_model(sint32 bucket, sint32 id, sint32 x, sint32 y, sint32 z)
{
    if (logical_ot_trace)
        fprintf(logical_ot_trace, "%d,MODEL,bucket=%d,id=%d,x=%#x,y=%#x,z=%#x\n", logical_ot_seq++, bucket, id, x, y, z);
}

void psx_ot_trace_object(const FrameObjectPartial *o, sint32 bucket)
{
    if (logical_ot_trace && o)
        fprintf(logical_ot_trace, "%d,OBJECT,bucket=%d,stored=%d,x=%#x,y=%#x,z=%#x,model=%d\n", logical_ot_seq++, bucket, (sint32)o->ot_bucket, o->world_x, o->world_y, o->world_z, o->model_plus_one);
}

void psx_ot_trace_floor(sint32 bucket, sint32 row_back, sint32 cell_x)
{
    if (logical_ot_trace)
        fprintf(logical_ot_trace, "%d,FLOOR,bucket=%d,row_back=%#x,x=%#x\n", logical_ot_seq++, bucket, row_back, cell_x);
}

void psx_ot_trace_end(void)
{
    if (logical_ot_trace)
    {
        fclose(logical_ot_trace);
        logical_ot_trace = 0;
        logical_ot_done = 1;
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

void psx_set_prim_screen_offset(sint32 x, sint32 y)
{
    prim_screen_offset_x = x;
    prim_screen_offset_y = y;
}

void psx_set_cell_sprite_light(const MAP_FLOOR_CELL *cell, sint32 cell_x, sint32 cell_z)
{
    sint32 light = 0x80, map_light = 0;
    if (g_map_cell_light_values && cell_x >= 0 && cell_z >= 0 && cell_x < g_map_width_cells && cell_z < g_map_depth_cells)
        map_light = g_map_cell_light_values[cell_z * g_map_width_cells + cell_x];
    if (cell && (sint8)cell->material != -1)
        light = cell->field_05;
    light = light - g_depth_lighting_bias + map_light + g_scene_brightness_bias;
    if (light < 0)
        light = 0;
    if (light > 255)
        light = 255;
    cell_sprite_light = light;
}

void psx_set_object_shade(sint32 shade)
{
    if (shade < 0)
        shade = 0;
    if (shade > 255)
        shade = 255;
    cell_sprite_light = shade;
}

sint32 psx_get_object_shade(void)
{
    return cell_sprite_light;
}

void psx_game_platform_configure(void)
{
    PSX_CONFIG config;
    const char *smoke;
    {
        char executable[MAX_PATH];
        char *slash;
        if (GetModuleFileName(0, executable, sizeof(executable)))
        {
            slash = strrchr(executable, '\\');
            if (slash)
            {
                *slash = '\0';
                SetCurrentDirectory(executable);
            }
        }
    }
    smoke = getenv("OA_STAGE_SMOKE");
    if (smoke && *smoke)
    {
        native_headless = 1;
        smoke_frame_limit = (sint32)strtol(smoke, 0, 0);
        smoke_wait_for_mission = getenv("OA_STAGE_SMOKE_WAIT_RUNTIME") != 0;
        if (smoke_frame_limit < 1)
            smoke_frame_limit = 1;
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    }
    memset(&config, 0, sizeof(config));
    config.window_title = "Agent Armstrong";
    config.window_width = 960;
    config.window_height = 768;
    config.refresh_rate = 50;
    config.headless = native_headless;
    config.host.load_image = game_gpu_load_image;
    config.host.move_image = game_gpu_move_image;
    config.host.store_image = game_gpu_store_image;
    config.host.clear_image = game_gpu_clear_image;
    config.host.clear_ot = game_gpu_clear_ot;
    config.host.add_prim = game_gpu_add_prim;
    config.host.add_prims = game_gpu_add_prims;
    config.host.draw_ot = game_gpu_draw_ot;
    config.host.draw_sync = game_psx_draw_sync;
    config.host.flush_cache = game_psx_flush_cache;
    config.host.sound_initialize = game_psx_sound_initialize;
    config.host.sound_shutdown = game_psx_sound_shutdown;
    config.host.sound_set_tick_mode = game_psx_sound_set_tick_mode;
    config.host.sound_start = game_psx_sound_start;
    config.host.sound_set_master_volume = game_psx_sound_set_master_volume;
    config.host.sound_set_serial_attributes = game_psx_sound_set_serial_attributes;
    config.host.sound_set_serial_volume = game_psx_sound_set_serial_volume;
    psx_configure(&config);
}

static void pxc(sint32 x, sint32 y, uint32 color)
{
    if (x >= raster_clip_x0 && x < raster_clip_x1 && y >= raster_clip_y0 && y < raster_clip_y1 && (uint32)x < FW && (uint32)y < FH)
        fb[y * FW + x] = color;
}

static void line_color(sint32 x0, sint32 y0, sint32 x1, sint32 y1, uint32 color)
{
    sint32 dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, e = dx + dy;
    for (;;)
    {
        sint32 q;
        pxc(x0, y0, color);
        if (x0 == x1 && y0 == y1)
            break;
        q = e * 2;
        if (q >= dy)
        {
            e += dy;
            x0 += sx;
        }
        if (q <= dx)
        {
            e += dx;
            y0 += sy;
        }
    }
}

void psx_draw_line(sint32 x0, sint32 y0, sint32 x1, sint32 y1)
{
    line_color(x0, y0, x1, y1, 0xffffff);
}

void psx_draw_quad(sint32 a, sint32 b, sint32 c, sint32 d, sint32 e, sint32 f, sint32 g, sint32 h)
{
    psx_draw_line(a, b, c, d);
    psx_draw_line(c, d, e, f);
    psx_draw_line(e, f, g, h);
    psx_draw_line(g, h, a, b);
}

void psx_begin_frame(void)
{
    if (menu_background_active)
        memcpy(fb, menu_background, sizeof(fb));
    else
        memset(fb, 0, sizeof(fb));
    map_prim_count = 0;
    prim_ot_bucket = -1;
    prim_screen_offset_x = prim_screen_offset_y = 0;
    psx_draw_quad(1, 1, 318, 1, 318, 254, 1, 254);
}

void psx_menu_background_end(void)
{
    menu_background_active = 0;
}

void psx_draw_triangle(sint32 a, sint32 b, sint32 c, sint32 d, sint32 e, sint32 f)
{
    psx_draw_line(a, b, c, d);
    psx_draw_line(c, d, e, f);
    psx_draw_line(e, f, a, b);
}

void psx_draw_quad_color(sint32 a, sint32 b, sint32 c, sint32 d, sint32 e, sint32 f, sint32 g, sint32 h, uint32 color)
{
    line_color(a, b, c, d, color);
    line_color(c, d, e, f, color);
    line_color(e, f, g, h, color);
    line_color(g, h, a, b, color);
}

void psx_set_display_offset_y(sint32 offset)
{
    display_offset_y = offset;
}

sint32 psx_dump_vram(const char *path)
{
    return app_file_write(path, vram, sizeof(vram));
}

void psx_end_frame(void)
{
    const uint32 *shown = fb;
    sint32 dy, rows;
    char title[80];
    if (native_headless)
    {
        if (smoke_wait_for_mission && !smoke_mission_started)
            return;
        ++frame_number;
        if (frame_number >= smoke_frame_limit)
        {
            const char *stage = getenv("OA_START_STAGE");
            printf("STAGE_SMOKE_PASS stage=%s frames=%d\n", stage ? stage : "HQ", frame_number);
            fflush(stdout);
            ExitProcess(0);
        }
        return;
    }
    if (psx_quit_requested())
        return;
    if (display_offset_y != 0)
    {
        dy = display_offset_y;
        if (dy >= FH)
            dy = FH;
        if (dy <= -FH)
            dy = -FH;
        memset(present_fb, 0, sizeof(present_fb));
        rows = FH - (dy < 0 ? -dy : dy);
        if (rows > 0)
        {
            if (dy > 0)
                memcpy(present_fb + dy * FW, fb, rows * FW * sizeof(*fb));
            else
                memcpy(present_fb, fb + (-dy) * FW, rows * FW * sizeof(*fb));
        }
        shown = present_fb;
    }
    ++frame_number;
    if (frame_number == 30 && getenv("OA_VRAM_FRAME_DUMP"))
        psx_dump_vram("VRAM_frame.raw");
    if ((frame_number % 30) == 0)
        sprintf(title, "OpenArmstrong - HQ wireframe - frame %d", frame_number);
    psx_window_present(shown, FW, FH, (frame_number % 30) == 0 ? title : NULL);
}

void psx_smoke_mission_start(void)
{
    if (native_headless && smoke_wait_for_mission)
    {
        frame_number = 0;
        smoke_mission_started = 1;
    }
}

static uint32 *game_gpu_clear_ot(void *user, uint32 *ot, sint32 count, sint32 reverse)
{
    sint32 i;
    (void)user;
    (void)reverse;
    for (i = 0; i < count; i++)
        ot[i] = 0;
    native_ot_count = 0;
    prim_screen_offset_x = prim_screen_offset_y = 0;
    for (i = 0; i < 0x481; i++)
        native_ot_heads[i] = -1;
    return ot;
}

static void game_gpu_add_prim(void *user, void *ot, void *prim)
{
    sint32 linked_bucket = -1;
    (void)user;
    if (g_current_render_frame)
    {
        sint32 bucket = prim_ot_bucket;
        if (bucket < 0 && prim_world_depth >= 0)
            bucket = (prim_world_depth - g_camera_world_z) >> 8;
        if (bucket >= 0)
            ot = g_current_render_frame->ot + bucket;
        if ((uint32 *)ot >= g_current_render_frame->ot && (uint32 *)ot <= g_current_render_frame->ot + 0x480)
            linked_bucket = (sint32)((uint32 *)ot - g_current_render_frame->ot);
    }
    prim_ot_bucket = -1;
    prim_world_depth = -1;
    if (native_ot_count < 16384)
    {
        sint32 entry = native_ot_count;
        native_ot[entry].ot = ot;
        native_ot[entry].prim = prim;
        native_ot[entry].clip_x0 = 0;
        native_ot[entry].clip_x1 = FW;
        native_ot[entry].clip_y0 = 0;
        native_ot[entry].clip_y1 = FH;
        native_ot[entry].offset_x = (sint16)prim_screen_offset_x;
        native_ot[entry].offset_y = (sint16)prim_screen_offset_y;
        native_ot[entry].kind = 0;
        native_ot[entry].owner = native_ot_owner;
        native_ot[entry].next = linked_bucket >= 0 ? native_ot_heads[linked_bucket] : -1;
        if (linked_bucket >= 0)
            native_ot_heads[linked_bucket] = entry;
        ++native_ot_count;
    }
    prim_screen_offset_x = prim_screen_offset_y = 0;
}

void psx_add_draw_area_rect(void *ot, sint32 x0, sint32 x1, sint32 y0, sint32 y1)
{
    sint32 bucket;
    if (!g_current_render_frame || native_ot_count >= 16384)
        return;
    bucket = (sint32)(((uint8 *)ot - (uint8 *)g_current_render_frame) >> 2);
    if (bucket < 0 || bucket > 0x480)
        return;
    native_ot[native_ot_count].ot = ot;
    native_ot[native_ot_count].prim = 0;
    native_ot[native_ot_count].clip_x0 = (sint16)x0;
    native_ot[native_ot_count].clip_x1 = (sint16)x1;
    native_ot[native_ot_count].clip_y0 = (sint16)y0;
    native_ot[native_ot_count].clip_y1 = (sint16)y1;
    native_ot[native_ot_count].offset_x = 0;
    native_ot[native_ot_count].offset_y = 0;
    native_ot[native_ot_count].kind = 1;
    native_ot[native_ot_count].owner = native_ot_owner;
    native_ot[native_ot_count].next = native_ot_heads[bucket];
    native_ot_heads[bucket] = native_ot_count++;
}

static void add_draw_area(void *ot, sint32 x0, sint32 x1)
{
    psx_add_draw_area_rect(ot, x0, x1, 0, FH);
}

static void game_gpu_add_prims(void *user, void *ot, void *p0, void *p1)
{
    sint32 bucket = prim_ot_bucket, depth = prim_world_depth;
    psx_set_prim_ot_bucket(bucket);
    psx_set_prim_depth(depth);
    game_gpu_add_prim(user, ot, p1);
    psx_set_prim_ot_bucket(bucket);
    psx_set_prim_depth(depth);
    game_gpu_add_prim(user, ot, p0);
}

static uint32 rgb555(uint16 c)
{
    uint32 r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
    return (r | (r >> 5)) << 16 | (g | (g >> 5)) << 8 | (b | (b >> 5));
}

static uint16 texel_indexed(sint32 u, sint32 v, uint16 tpage, uint16 clut, sint32 *transparent)
{
    sint32 tp = (tpage >> 7) & 3, tx = (tpage & 15) * 64, ty = ((tpage >> 4) & 1) * 256, cx = (clut & 63) * 16, cy = (clut >> 6) & 0x1ff, index;
    uint16 w, color;
    u = ((u & ~(((sint32)texture_window & 31) << 3)) | ((((sint32)texture_window >> 10) & 31) & ((sint32)texture_window & 31)) << 3) & 255;
    v = ((v & ~((((sint32)texture_window >> 5) & 31) << 3)) | ((((sint32)texture_window >> 15) & 31) & (((sint32)texture_window >> 5) & 31)) << 3) & 255;
    *transparent = 1;
    if ((uint32)(ty + v) >= VRAM_H || (uint32)cy >= VRAM_H)
        return 0;
    if (tp == 0)
    {
        w = vram[(ty + v) * VRAM_W + tx + (u >> 2)];
        index = (w >> ((u & 3) * 4)) & 15;
    }
    else if (tp == 1)
    {
        w = vram[(ty + v) * VRAM_W + tx + (u >> 1)];
        index = (w >> ((u & 1) * 8)) & 255;
    }
    else
    {
        if ((uint32)(tx + u) >= VRAM_W)
            return 0;
        color = vram[(ty + v) * VRAM_W + tx + u];
        *transparent = color == 0;
        return color;
    }
    if ((uint32)(cx + index) >= VRAM_W)
        return 0;
    color = vram[cy * VRAM_W + cx + index];
    *transparent = color == 0;
    return color;
}

static uint16 texel(sint32 u, sint32 v, uint16 tpage, uint16 clut)
{
    sint32 transparent;
    return texel_indexed(u, v, tpage, clut, &transparent);
}

static uint32 modulate(uint16 c, sint32 r, sint32 g, sint32 b, sint32 raw)
{
    uint32 q = rgb555(c), tr = (q >> 16) & 255, tg = (q >> 8) & 255, tb = q & 255;
    if (raw)
        return q;
    tr = tr * r / 128;
    tg = tg * g / 128;
    tb = tb * b / 128;
    if (tr > 255)
        tr = 255;
    if (tg > 255)
        tg = 255;
    if (tb > 255)
        tb = 255;
    return tr << 16 | tg << 8 | tb;
}

static uint32 semi_blend(uint32 back, uint32 front, sint32 abr)
{
    sint32 br = (back >> 19) & 31, bg = (back >> 11) & 31, bb = (back >> 3) & 31, fr = (front >> 19) & 31, fg = (front >> 11) & 31, fbv = (front >> 3) & 31, r, g, b;
    if (abr == 0)
    {
        r = (br + fr) >> 1;
        g = (bg + fg) >> 1;
        b = (bb + fbv) >> 1;
    }
    else if (abr == 1)
    {
        r = br + fr;
        g = bg + fg;
        b = bb + fbv;
    }
    else if (abr == 2)
    {
        r = br - fr;
        g = bg - fg;
        b = bb - fbv;
    }
    else
    {
        r = br + (fr >> 2);
        g = bg + (fg >> 2);
        b = bb + (fbv >> 2);
    }
    if (r < 0)
        r = 0;
    if (g < 0)
        g = 0;
    if (b < 0)
        b = 0;
    if (r > 31)
        r = 31;
    if (g > 31)
        g = 31;
    if (b > 31)
        b = 31;
    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return (uint32)(r << 16 | g << 8 | b);
}

static uint32 packet_rgb(uint8 *p, sint32 o)
{
    return (uint32)p[o] << 16 | (uint32)p[o + 1] << 8 | p[o + 2];
}

static void raster_bounds(RV a, RV b, RV c, sint32 *minx, sint32 *maxx, sint32 *miny, sint32 *maxy)
{
    *minx = a.x;
    *maxx = a.x;
    *miny = a.y;
    *maxy = a.y;
    if (b.x < *minx)
        *minx = b.x;
    if (c.x < *minx)
        *minx = c.x;
    if (b.x > *maxx)
        *maxx = b.x;
    if (c.x > *maxx)
        *maxx = c.x;
    if (b.y < *miny)
        *miny = b.y;
    if (c.y < *miny)
        *miny = c.y;
    if (b.y > *maxy)
        *maxy = b.y;
    if (c.y > *maxy)
        *maxy = c.y;
    if (*minx < raster_clip_x0)
        *minx = raster_clip_x0;
    if (*maxx >= raster_clip_x1)
        *maxx = raster_clip_x1 - 1;
    if (*miny < raster_clip_y0)
        *miny = raster_clip_y0;
    if (*maxy >= raster_clip_y1)
        *maxy = raster_clip_y1 - 1;
    if (*minx < 0)
        *minx = 0;
    if (*miny < 0)
        *miny = 0;
    if (*maxx >= FW)
        *maxx = FW - 1;
    if (*maxy >= FH)
        *maxy = FH - 1;
}

static sint64 edge2(const RV *a, const RV *b, sint32 sx, sint32 sy)
{
    return (sint64)(sx - a->x * 2) * (b->y - a->y) - (sint64)(sy - a->y * 2) * (b->x - a->x);
}

static sint64 fixed_value(sint32 aa, sint32 bb, sint32 cc, sint64 e0, sint64 e1, sint64 e2, sint64 denominator)
{
    return ((sint64)aa * e0 + (sint64)bb * e1 + (sint64)cc * e2) * 65536 / denominator;
}

static sint32 setup_scan(RV a, RV b, RV c, uint32 ca, uint32 cb, uint32 cc, RasterScan *s)
{
    sint64 area, denominator, sign;
    sint32 sx, sy;
    raster_bounds(a, b, c, &s->minx, &s->maxx, &s->miny, &s->maxy);
    if (s->minx > s->maxx || s->miny > s->maxy)
        return 0;
    area = (sint64)(c.x - a.x) * (b.y - a.y) - (sint64)(c.y - a.y) * (b.x - a.x);
    if (area == 0)
        return 0;
    sign = area < 0 ? -1 : 1;
    denominator = area * 2 * sign;
    sx = s->minx * 2 + 1;
    sy = s->miny * 2 + 1;
    s->e0 = edge2(&b, &c, sx, sy) * sign;
    s->e1 = edge2(&c, &a, sx, sy) * sign;
    s->e2 = edge2(&a, &b, sx, sy) * sign;
    s->e0dx = (sint64)2 * (c.y - b.y) * sign;
    s->e1dx = (sint64)2 * (a.y - c.y) * sign;
    s->e2dx = (sint64)2 * (b.y - a.y) * sign;
    s->e0dy = (sint64)-2 * (c.x - b.x) * sign;
    s->e1dy = (sint64)-2 * (a.x - c.x) * sign;
    s->e2dy = (sint64)-2 * (b.x - a.x) * sign;
    s->u = fixed_value(a.u, b.u, c.u, s->e0, s->e1, s->e2, denominator);
    s->v = fixed_value(a.v, b.v, c.v, s->e0, s->e1, s->e2, denominator);
    s->r = fixed_value((ca >> 16) & 255, (cb >> 16) & 255, (cc >> 16) & 255, s->e0, s->e1, s->e2, denominator);
    s->g = fixed_value((ca >> 8) & 255, (cb >> 8) & 255, (cc >> 8) & 255, s->e0, s->e1, s->e2, denominator);
    s->b = fixed_value(ca & 255, cb & 255, cc & 255, s->e0, s->e1, s->e2, denominator);
    s->udx = fixed_value(a.u, b.u, c.u, s->e0dx, s->e1dx, s->e2dx, denominator);
    s->vdx = fixed_value(a.v, b.v, c.v, s->e0dx, s->e1dx, s->e2dx, denominator);
    s->rdx = fixed_value((ca >> 16) & 255, (cb >> 16) & 255, (cc >> 16) & 255, s->e0dx, s->e1dx, s->e2dx, denominator);
    s->gdx = fixed_value((ca >> 8) & 255, (cb >> 8) & 255, (cc >> 8) & 255, s->e0dx, s->e1dx, s->e2dx, denominator);
    s->bdx = fixed_value(ca & 255, cb & 255, cc & 255, s->e0dx, s->e1dx, s->e2dx, denominator);
    s->udy = fixed_value(a.u, b.u, c.u, s->e0dy, s->e1dy, s->e2dy, denominator);
    s->vdy = fixed_value(a.v, b.v, c.v, s->e0dy, s->e1dy, s->e2dy, denominator);
    s->rdy = fixed_value((ca >> 16) & 255, (cb >> 16) & 255, (cc >> 16) & 255, s->e0dy, s->e1dy, s->e2dy, denominator);
    s->gdy = fixed_value((ca >> 8) & 255, (cb >> 8) & 255, (cc >> 8) & 255, s->e0dy, s->e1dy, s->e2dy, denominator);
    s->bdy = fixed_value(ca & 255, cb & 255, cc & 255, s->e0dy, s->e1dy, s->e2dy, denominator);
    return 1;
}

static void textured_triangle(RV a, RV b, RV c, uint16 tp, uint16 cl, uint32 ca, uint32 cb, uint32 cc, sint32 raw, sint32 semi)
{
    RasterScan s;
    sint32 x, y;
    if (!setup_scan(a, b, c, ca, cb, cc, &s))
        return;
    for (y = s.miny; y <= s.maxy; y++)
    {
        sint64 e0 = s.e0, e1 = s.e1, e2 = s.e2, u = s.u, v = s.v, rr = s.r, gg = s.g, bb = s.b;
        sint32 entered = 0;
        for (x = s.minx; x <= s.maxx; x++)
        {
            if (e0 >= 0 && e1 >= 0 && e2 >= 0)
            {
                sint32 transparent, r, g, bl;
                uint16 t;
                uint32 color;
                entered = 1;
                t = texel_indexed((sint32)(u >> 16), (sint32)(v >> 16), tp, cl, &transparent);
                if (!transparent)
                {
                    r = raw ? 128 : (sint32)(rr >> 16);
                    g = raw ? 128 : (sint32)(gg >> 16);
                    bl = raw ? 128 : (sint32)(bb >> 16);
                    color = modulate(t, r, g, bl, 0);
                    if (semi && (t & 0x8000))
                        color = semi_blend(fb[y * FW + x], color, (tp >> 5) & 3);
                    fb[y * FW + x] = color;
                }
            }
            else if (entered)
                break;
            e0 += s.e0dx;
            e1 += s.e1dx;
            e2 += s.e2dx;
            u += s.udx;
            v += s.vdx;
            rr += s.rdx;
            gg += s.gdx;
            bb += s.bdx;
        }
        s.e0 += s.e0dy;
        s.e1 += s.e1dy;
        s.e2 += s.e2dy;
        s.u += s.udy;
        s.v += s.vdy;
        s.r += s.rdy;
        s.g += s.gdy;
        s.b += s.bdy;
    }
}

static void colored_triangle(RV a, RV b, RV c, uint32 ca, uint32 cb, uint32 cc, sint32 semi)
{
    RasterScan s;
    sint32 x, y;
    if (!setup_scan(a, b, c, ca, cb, cc, &s))
        return;
    for (y = s.miny; y <= s.maxy; y++)
    {
        sint64 e0 = s.e0, e1 = s.e1, e2 = s.e2, rr = s.r, gg = s.g, bb = s.b;
        sint32 entered = 0;
        for (x = s.minx; x <= s.maxx; x++)
        {
            if (e0 >= 0 && e1 >= 0 && e2 >= 0)
            {
                uint32 color;
                sint32 r = (sint32)(rr >> 16), g = (sint32)(gg >> 16), bl = (sint32)(bb >> 16);
                entered = 1;
                color = (uint32)r << 16 | (uint32)g << 8 | (uint32)bl;
                if (semi)
                    color = semi_blend(fb[y * FW + x], color, (active_tpage >> 5) & 3);
                fb[y * FW + x] = color;
            }
            else if (entered)
                break;
            e0 += s.e0dx;
            e1 += s.e1dx;
            e2 += s.e2dx;
            rr += s.rdx;
            gg += s.gdx;
            bb += s.bdx;
        }
        s.e0 += s.e0dy;
        s.e1 += s.e1dy;
        s.e2 += s.e2dy;
        s.r += s.rdy;
        s.g += s.gdy;
        s.b += s.bdy;
    }
}

static RV rv(uint8 *p, sint32 xy, sint32 uv)
{
    RV v;
    v.x = read_s16_le_at(p, xy) + raster_offset_x;
    v.y = read_s16_le_at(p, xy + 2) + raster_offset_y;
    v.u = p[uv];
    v.v = p[uv + 1];
    return v;
}

static void magenta_bbox(uint8 *p, sint32 n, const sint32 *ofs)
{
    sint32 i, minx = 32767, miny = 32767, maxx = -32768, maxy = -32768, x, y;
    for (i = 0; i < n; i++)
    {
        x = read_s16_le_at(p, ofs[i]);
        y = read_s16_le_at(p, ofs[i] + 2);
        if (x < minx)
            minx = x;
        if (x > maxx)
            maxx = x;
        if (y < miny)
            miny = y;
        if (y > maxy)
            maxy = y;
    }
    if (minx < 0)
        minx = 0;
    if (miny < 0)
        miny = 0;
    if (maxx >= FW)
        maxx = FW - 1;
    if (maxy >= FH)
        maxy = FH - 1;
    for (y = miny; y <= maxy; y++)
        for (x = minx; x <= maxx; x++)
            pxc(x, y, 0xff00ff);
}

static void draw_prim(void *raw)
{
    uint8 *p = (uint8 *)raw;
    sint32 code = p[7] & 0xfc;
    uint32 *w = (uint32 *)p;
    uint32 ca, cb, cc, cd;
    if ((w[1] & 0xff000000) == 0xe1000000)
    {
        active_tpage = (uint16)(w[1] & 0x9ff);
        if ((w[2] & 0xff000000) == 0xe2000000)
            texture_window = w[2] & 0xfffff;
        return;
    }
    if ((w[1] & 0xff000000) == 0xe2000000)
    {
        texture_window = w[1] & 0xfffff;
        return;
    }
    ca = packet_rgb(p, 4);
    if (code == 0x20)
    {
        RV a = rv(p, 8, 0), b = rv(p, 12, 0), c = rv(p, 16, 0);
        colored_triangle(a, b, c, ca, ca, ca, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x28)
    {
        RV a = rv(p, 8, 0), b = rv(p, 12, 0), c = rv(p, 16, 0), d = rv(p, 20, 0);
        colored_triangle(a, b, c, ca, ca, ca, (p[7] & 2) != 0);
        colored_triangle(b, c, d, ca, ca, ca, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x30)
    {
        RV a = rv(p, 8, 0), b = rv(p, 16, 0), c = rv(p, 24, 0);
        cb = packet_rgb(p, 12);
        cc = packet_rgb(p, 20);
        colored_triangle(a, b, c, ca, cb, cc, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x38)
    {
        RV a = rv(p, 8, 0), b = rv(p, 16, 0), c = rv(p, 24, 0), d = rv(p, 32, 0);
        cb = packet_rgb(p, 12);
        cc = packet_rgb(p, 20);
        cd = packet_rgb(p, 28);
        colored_triangle(a, b, c, ca, cb, cc, (p[7] & 2) != 0);
        colored_triangle(b, c, d, cb, cc, cd, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x24)
    {
        RV a = rv(p, 8, 12), b = rv(p, 16, 20), c = rv(p, 24, 28);
        textured_triangle(a, b, c, *(uint16 *)(p + 22), *(uint16 *)(p + 14), ca, ca, ca, p[7] & 1, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x2c)
    {
        RV a = rv(p, 8, 12), b = rv(p, 16, 20), c = rv(p, 24, 28), d = rv(p, 32, 36);
        uint16 tp = *(uint16 *)(p + 22), cl = *(uint16 *)(p + 14);
        textured_triangle(a, b, c, tp, cl, ca, ca, ca, p[7] & 1, (p[7] & 2) != 0);
        textured_triangle(b, c, d, tp, cl, ca, ca, ca, p[7] & 1, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x34)
    {
        RV a = rv(p, 8, 12), b = rv(p, 20, 24), c = rv(p, 32, 36);
        uint16 tp = *(uint16 *)(p + 26), cl = *(uint16 *)(p + 14);
        cb = packet_rgb(p, 16);
        cc = packet_rgb(p, 28);
        textured_triangle(a, b, c, tp, cl, ca, cb, cc, p[7] & 1, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x3c)
    {
        RV a = rv(p, 8, 12), b = rv(p, 20, 24), c = rv(p, 32, 36), d = rv(p, 44, 48);
        uint16 tp = *(uint16 *)(p + 26), cl = *(uint16 *)(p + 14);
        cb = packet_rgb(p, 16);
        cc = packet_rgb(p, 28);
        cd = packet_rgb(p, 40);
        textured_triangle(a, b, c, tp, cl, ca, cb, cc, p[7] & 1, (p[7] & 2) != 0);
        textured_triangle(b, c, d, tp, cl, cb, cc, cd, p[7] & 1, (p[7] & 2) != 0);
        return;
    }
    if (code == 0x64 || code == 0x74 || code == 0x7c)
    {
        sint32 x, y, x0 = read_s16_le_at(p, 8) + raster_offset_x, y0 = read_s16_le_at(p, 10) + raster_offset_y, ww = code == 0x64 ? read_s16_le_at(p, 16) : (code == 0x74 ? 8 : 16), hh = code == 0x64 ? read_s16_le_at(p, 18) : (code == 0x74 ? 8 : 16);
        for (y = 0; y < hh; y++)
            for (x = 0; x < ww; x++)
            {
                sint32 transparent;
                uint16 t = texel_indexed(p[12] + x, p[13] + y, active_tpage, *(uint16 *)(p + 14), &transparent);
                if (!transparent && x0 + x >= raster_clip_x0 && x0 + x < raster_clip_x1 && y0 + y >= raster_clip_y0 && y0 + y < raster_clip_y1 && (uint32)(x0 + x) < FW && (uint32)(y0 + y) < FH)
                {
                    uint32 color = modulate(t, p[4], p[5], p[6], p[7] & 1);
                    if ((p[7] & 2) && (t & 0x8000))
                        color = semi_blend(fb[(y0 + y) * FW + x0 + x], color, (active_tpage >> 5) & 3);
                    fb[(y0 + y) * FW + x0 + x] = color;
                }
            }
        return;
    }
    if (code == 0x60 || code == 0x68 || code == 0x70 || code == 0x78)
    {
        sint32 x, y, x0 = read_s16_le_at(p, 8) + raster_offset_x, y0 = read_s16_le_at(p, 10) + raster_offset_y, ww = code == 0x60 ? read_s16_le_at(p, 12) : (code == 0x68 ? 1 : code == 0x70 ? 8 : 16), hh = code == 0x60 ? read_s16_le_at(p, 14) : (code == 0x68 ? 1 : code == 0x70 ? 8 : 16);
        for (y = 0; y < hh; y++)
            for (x = 0; x < ww; x++)
            {
                uint32 color = ca;
                if (p[7] & 2)
                    color = semi_blend(fb[(y0 + y) * FW + x0 + x], color, (active_tpage >> 5) & 3);
                pxc(x0 + x, y0 + y, color);
            }
        return;
    }
    pxc(read_s16_le_at(p, 8) + raster_offset_x, read_s16_le_at(p, 10) + raster_offset_y, 0xff00ff);
}

/* B36F4 forwards the supplied OT head unchanged (800B3730..800B3750).
 * FDC8C submits frame+4, not frame base: process bucket 1 and its tail.
 * The full-frame native caller uses the base as its explicit all-buckets API. */
static void game_gpu_draw_ot(void *user, uint32 *ot)
{
    sint32 bucket, first;
    ptrdiff_t delta;
    (void)user;
    if (!g_current_render_frame || !ot)
        return;
    delta = (uint8 *)ot - (uint8 *)g_current_render_frame->ot;
    if (delta < 0 || delta > 0x480 * 4 || (delta & 3) != 0)
        return;
    first = ot == g_current_render_frame->ot ? 0x480 : (sint32)(delta / 4);
    active_tpage = 0;
    texture_window = 0;
    raster_clip_x0 = 0;
    raster_clip_x1 = FW;
    raster_clip_y0 = 0;
    raster_clip_y1 = FH;
    raster_offset_x = raster_offset_y = 0;
    dump_ot_trace_once();
    for (bucket = first; bucket >= 0; --bucket)
    {
        sint32 entry = native_ot_heads[bucket];
        while (entry >= 0)
        {
            if (native_ot[entry].kind == 1)
            {
                raster_clip_x0 = native_ot[entry].clip_x0;
                raster_clip_x1 = native_ot[entry].clip_x1;
                raster_clip_y0 = native_ot[entry].clip_y0;
                raster_clip_y1 = native_ot[entry].clip_y1;
            }
            else
            {
                raster_offset_x = native_ot[entry].offset_x;
                raster_offset_y = native_ot[entry].offset_y;
                draw_prim(native_ot[entry].prim);
            }
            entry = native_ot[entry].next;
        }
    }
    raster_clip_x0 = 0;
    raster_clip_x1 = FW;
    raster_clip_y0 = 0;
    raster_clip_y1 = FH;
    raster_offset_x = raster_offset_y = 0;
}

/* Original: FUN_80088A3C. */
GDB_CALL void end_frame_submit(sint32 mode)
{
    RENDER_FRAME *frame = g_current_render_frame;
    /* 80088A4C/80088A54: release deferred VRAM descriptors before sweeping
     * their heap blocks.  Door entry/exit sprites allocate one descriptor
     * each, so omitting these calls exhausts/overlaps their dynamic VRAM
     * regions after the first transition. */
    sprite_vram_release_deferred();
    runtime_heap_sweep();
    DrawSync(0);
    if (g_sprite_subdivision_cooldown != 0)
        --g_sprite_subdivision_cooldown;
    else if (g_vsync_count_this_frame != 0)
        g_sprite_subdivision_cooldown = 0x1e;
    else
        VSync(0);
    if (g_gpu_packet_cursor >= (uint8 *)(frame + 1))
        fatal_error("PRIM OVERFLOW");
    if (mode == 0)
        frame->drawenv.isbg = 1;
    g_render_frame_buffer_0.display_env.screen.h = (sint16)g_screen_height;
    g_render_frame_buffer_1.display_env.screen.h = (sint16)g_screen_height;
    frame->drawenv.isbg = 0;
    frame->drawenv.dtd = 0;
    DrawOTag(frame->ot);
    psx_end_frame();
    g_sound_handles_invalidated = 0;
    g_previous_held_buttons = g_held_buttons;
}

static sint32 game_gpu_move_image(void *user, PSX_RECT *r, sint32 x, sint32 y)
{
    sint32 row;
    (void)user;
    if (!r)
        return -1;
    if (x < 0 || y < 0 || x + r->w > VRAM_W || y + r->h > VRAM_H)
        return -1;
    if (r->x == 0 && r->y == 0 && r->w == FW && r->h == FH && x == 0 && y == FH)
    {
        memcpy(menu_background, fb, sizeof(fb));
        menu_background_active = 1;
    }
    if (y > r->y)
        for (row = r->h - 1; row >= 0; row--)
            memmove(&vram[(y + row) * VRAM_W + x], &vram[(r->y + row) * VRAM_W + r->x], r->w * 2);
    else
        for (row = 0; row < r->h; row++)
            memmove(&vram[(y + row) * VRAM_W + x], &vram[(r->y + row) * VRAM_W + r->x], r->w * 2);
    return 0;
}

static sint32 game_gpu_store_image(void *user, PSX_RECT *r, uint32 *p)
{
    sint32 row;
    (void)user;
    if (!r || !p)
        return -1;
    for (row = 0; row < r->h; row++)
        memcpy((uint16 *)p + row * r->w, &vram[(r->y + row) * VRAM_W + r->x], r->w * 2);
    return 0;
}

static sint32 game_gpu_clear_image(void *user, PSX_RECT *r, uint8 rr, uint8 gg, uint8 bb)
{
    sint32 x, y;
    uint16 c = (uint16)((rr >> 3) | ((gg >> 3) << 5) | ((bb >> 3) << 10));
    (void)user;
    if (!r)
        return -1;
    for (y = 0; y < r->h; y++)
        for (x = 0; x < r->w; x++)
            if ((uint32)(r->x + x) < VRAM_W && (uint32)(r->y + y) < VRAM_H)
                vram[(r->y + y) * VRAM_W + r->x + x] = c;
    return 0;
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

static void put16(uint8 *p, sint32 o, sint32 v)
{
    *(sint16 *)(p + o) = (sint16)v;
}

/* 0x80090298..0x80090458.  The original constructs a screen-sized POLY_G4
 * under two draw-environment packets.  Our native OT stores clipping state
 * separately, while the polygon payload and bucket/order remain identical. */
/* Original: FUN_80090298. */
GDB_CALL void screen_overlay_draw(void)
{
    uint8 *prim;

    if (map_prim_count >= 8192)
        return;
    prim = map_prims[map_prim_count++];
    memset(prim, 0, 40);
    prim[3] = 8;
    prim[4] = 0;
    prim[5] = 0;
    prim[6] = 0x78;
    prim[7] = 0x38;
    put16(prim, 8, 0);
    put16(prim, 10, 0);
    prim[12] = 0;
    prim[13] = 0;
    prim[14] = 0x78;
    put16(prim, 16, 320);
    put16(prim, 18, 0);
    prim[20] = 0;
    prim[21] = 0;
    prim[22] = 0;
    put16(prim, 24, 0);
    put16(prim, 26, 256);
    prim[28] = 0;
    prim[29] = 0;
    prim[30] = 0;
    put16(prim, 32, 320);
    put16(prim, 34, 256);
    /* 80090388/800903AC/800903E8/80090430 all link at frame+4:
     * byte offset 4 is bucket 1, not bucket 4. */
    psx_set_prim_ot_bucket(1);
    AddPrim(g_current_render_frame->ot + 1, prim);
    psx_add_draw_area_rect(g_current_render_frame->ot + 1, 0, 320, 0, 256);
    psx_add_draw_area_rect(g_current_render_frame->ot + 1, 0, 320, 0, 256);
}

static uint16 map_tpage(sint32 texture)
{
    return (texture >= 0 && texture < 64 && map_textures[texture].valid) ? map_textures[texture].tpage : 0;
}

static uint16 map_clut(sint32 texture)
{
    return (texture >= 0 && texture < 64 && map_textures[texture].valid) ? map_textures[texture].clut : 0;
}

static sint32 archive_entry(uint8 *data_start, uint32 size, const char *name, uint8 **data, uint32 *length)
{
    const CC_ARCHIVE_HEADER *header = (const CC_ARCHIVE_HEADER *)data_start;
    const CC_ARCHIVE_ENTRY *entries;
    sint32 index, count;
    if (!data_start || size < sizeof(*header))
        return 0;
    count = header->entry_count;
    if (count < 0 || count > 256 || sizeof(*header) + (uint32)count * sizeof(*entries) > size)
        return 0;
    entries = (const CC_ARCHIVE_ENTRY *)(header + 1);
    for (index = 0; index < count; index++)
    {
        const CC_ARCHIVE_ENTRY *entry = entries + index;
        if (strncmp(entry->name, name, sizeof(entry->name)) == 0 && entry->offset >= 0 && entry->size > 0 && (uint32)entry->offset + (uint32)entry->size <= size)
        {
            *data = data_start + entry->offset;
            *length = (uint32)entry->size;
            return 1;
        }
    }
    return 0;
}

static sint32 load_archive_resource(uint8 *archive, uint32 archive_size, const char *name, uint32 resource, sint32 frames, sint32 mode)
{
    uint8 *entry;
    uint32 entry_size;
    uint16 clut;
    if (!archive_entry(archive, archive_size, name, &entry, &entry_size))
        return 0;
    resource_table_register(resource, entry);
    sprite_create_vram_descriptors(resource, frames, 0, (sint16)mode);
    clut = (uint16)sprite_clut_upload(resource, 0, 0, mode);
    sprite_assign_clut_range(resource, frames, clut);
    return 1;
}

/* HQ execution of the -7 branch in FUN_800AB478.  Every TEXINFO material is
 * registered, allocated, uploaded and assigned a CLUT in source order. */
sint32 psx_load_hq_textures(const uint8 *texinfo, uint32 texinfo_size)
{
    sint32 file_size;
    uint32 archive_size, entry_size;
    sint32 raw, loaded = 0;
    FILE *trace = 0;
    remove("floor_tiles_port.csv");
    hqtex_archive = (uint8 *)game_file_load("HQ\\HQTEX.CC", &file_size);
    archive_size = (uint32)file_size;
    if (!hqtex_archive || !texinfo)
        return 0;
    if (getenv("OA_VRAM_LAYOUT_TRACE"))
    {
        trace = fopen("vram_layout_port.csv", "w");
        if (trace)
            fprintf(trace, "material,tx,resource,u,v,tpage,page_x,page_y,mode,clut\n");
    }
    memset(map_textures, 0, sizeof(map_textures));
    memset(model_textures, 0, sizeof(model_textures));
    for (raw = 0; raw < 63 && raw < (sint32)texinfo_size; raw++)
    {
        uint8 tx = texinfo[raw], *entry;
        VRAM_SPRITE *descriptor;
        uint32 resource;
        uint16 clut;
        char name[16];
        if (tx == 0xff)
            break;
        if (tx >= 46)
            continue;
        resource = g_hq_material_resources[tx];
        sprintf(name, "TX%02u.BIN", (uint32)tx);
        if (!archive_entry(hqtex_archive, archive_size, name, &entry, &entry_size))
            continue;
        resource_table_register(resource, entry);
        descriptor = (VRAM_SPRITE *)sprite_create_vram_descriptors(resource, 1, 0, 0);
        clut = (uint16)sprite_clut_upload(resource, 0, 0, 0);
        sprite_assign_clut_range(resource, 1, clut);
        if (!descriptor)
            continue;
        map_textures[raw].u = (uint8)descriptor->u0;
        map_textures[raw].v = (uint8)descriptor->v0;
        map_textures[raw].tpage = descriptor->tpage;
        map_textures[raw].clut = clut;
        map_textures[raw].valid = 1;
        loaded++;
        if ((resource >> 10) < MODEL_TEXTURE_SLOT_COUNT)
            model_textures[resource >> 10] = map_textures[raw];
        if (trace)
            fprintf(trace, "%d,%u,%#x,%u,%u,%u,%u,%u,%u,%u\n", raw, (uint32)tx, resource, map_textures[raw].u, map_textures[raw].v, map_textures[raw].tpage, descriptor->page_x, descriptor->page_y, descriptor->pixel_mode, clut);
    }
    /* FUN_800AB7F8..800AB894 (-7 command tail).  Stages whose environment
     * record has a nonzero +0x10 field load one 0x30400 background tile;
     * the filename is selected by the stage byte table at 800CC454. */
    {
        const STAGE_ENV_RECORD_TAIL *env = (const STAGE_ENV_RECORD_TAIL *)player_assets_executable_address(0x800cd504u + (uint32)g_stage_index * 0x24u);
        const uint8 *tile_index = (const uint8 *)player_assets_executable_address(0x800cc454u + (uint32)g_stage_index);
        const char *tile_name = tile_index ? (const char *)player_assets_executable_address(0x800cc3d0u + (uint32)*tile_index * 0x0du) : 0;
        if (env && env->background_resource && tile_name)
            load_archive_resource(hqtex_archive, archive_size, tile_name, 0x30400, 1, 0);
    }
    /* Remaining literal commands in 800CCF88: ranges 63..65 and 85..85. */
    load_archive_resource(hqtex_archive, archive_size, "HQBACK.BIN", 0x6400, 1, 1);
    load_archive_resource(hqtex_archive, archive_size, "MAPBITS.BIN", 0x2000, 11, 1);
    load_archive_resource(hqtex_archive, archive_size, "AGNTLOGO.BIN", 0x25c00, 2, 1);
    load_archive_resource(hqtex_archive, archive_size, "MAPMEDAL.BIN", 0x30800, 1, 1);
    if (trace)
        fclose(trace);
    return loaded;
}

static const char *resource_path(sint32 index)
{
    return (const char *)player_assets_executable_address(0x800cc478u + (uint32)index * 0x18u);
}

static const TEXTURE_RESOURCE_RECORD *texture_record(sint32 index)
{
    return (const TEXTURE_RESOURCE_RECORD *)player_assets_executable_address(0x800cbbc0u + (uint32)index * 0x18u);
}

static const TEXTURE_RESOURCE_RECORD *texture_record_by_resource(uint32 resource)
{
    const TEXTURE_RESOURCE_RECORD *record = texture_record(0);
    /* 800AB768..800AB7A4 advances through 0x18-byte records until +0x10
     * equals the requested nonzero material. */
    while (record->resource != resource)
        ++record;
    return record;
}

static void register_direct_texture(uint8 *data, uint32 resource)
{
    uint16 clut;
    resource_table_register(resource, data);
    clut = (uint16)sprite_clut_upload(resource, 0, 0, 0);
    /* FUN_800AB478 -4 stores the upload result in the resource-indexed
     * CLUT table directly.  It does not allocate a frame descriptor and does
     * not call FUN_8008CD98 on this path. */
    g_resource_clut_table[resource >> 10] = clut;
}

static void sync_map_texture_descriptor(sint32 material, uint32 resource, uint16 clut)
{
    const VRAM_SPRITE *descriptor = (const VRAM_SPRITE *)sprite_vram_descriptor_for_resource(resource);
    sint32 resource_index = (sint32)(resource >> 10);
    if (descriptor == 0 || material < 0 || material >= 64)
        return;
    map_textures[material].u = (uint8)descriptor->u0;
    map_textures[material].v = (uint8)descriptor->v0;
    map_textures[material].tpage = descriptor->tpage;
    map_textures[material].clut = clut;
    map_textures[material].valid = 1;
    if (resource_index >= 0 && resource_index < MODEL_TEXTURE_SLOT_COUNT)
        model_textures[resource_index] = map_textures[material];
}

/* Original: FUN_80095300. */
GDB_CALL void stage_models_queue_load(sint32 unused, sint16 *base, sint16 count, sint32 material1, sint32 material2, sint32 material3, const uint8 *data)
{
    PendingStageModels *pending;
    sint32 size = data == resource_script_direct ? resource_script_direct_size : g_archive_member_size;
    (void)unused;
    if (pending_stage_models_count >= 8)
        return;
    pending = &pending_stage_models[pending_stage_models_count++];
    pending->base = base;
    pending->data = (uint8 *)data;
    pending->size = size;
    pending->count = count;
    pending->material1 = material1;
    pending->material2 = material2;
    pending->material3 = material3;
    /* PAL 80095370..800953E4: publish the current descriptor base, then
     * 92D50 advances 4078 once per model. Parsing is deferred on the host,
     * but AB0CC must already see this increment when it saves the count.
     * Otherwise the player target model (8A83C..8A950) overwrites the first
     * stage model after AB220 restores that stale count. */
    *base = g_model_count;
    g_model_count = (sint16)(g_model_count + count);
}

static void *resource_script_writable(uint32 address)
{
    if (address == 0x800d4150u)
        return &g_truck_model_base;
    if (address == 0x800d406cu)
        return &g_gyro_model_base;
    if (address == 0x800d40e4u)
        return &g_radar_vehicle_model_base;
    if (address == 0x800d40f0u)
        return &g_overlay_initial_pose_slot_40f0;
    if (address == 0x800d41e0u)
        return &g_tanya_model_base;
    if (address == 0x800d41e4u)
        return &g_tanya_initial_pose;
    if (address == 0x800d3ed2u)
        return &g_gunship_model_base;
    if (address == 0x800d3ed8u)
        return &g_gunship_initial_pose;
    if (address == 0x800d3e54u)
        return &g_v2_rocket_model_base;
    if (address == 0x800d3e60u)
        return &g_v2_rocket_initial_pose;
    if (address == 0x800d4078u)
        return &g_model_count;
    if (address == 0x800d4148u)
        return &g_mechanoid_model_base;
    if (address == 0x800d3facu)
        return &g_mini_gyro_model_base;
    if (address == 0x800d3fb8u)
        return &g_mini_gyro_initial_pose;
    if (address == 0x800d3e44u)
        return &g_mini_gyro_type_117_model_base;
    if (address == 0x800d3e48u)
        return &g_mini_gyro_type_117_initial_pose;
    if (address == 0x800d4000u)
        return &g_overlay_model_base_slot_4000;
    if (address == 0x800d4004u)
        return &g_overlay_initial_pose_slot_4004;
    if (address == 0x800d3f7cu)
        return &g_scuba_drifting_hazard_model_id;
    if (address == 0x800d3f84u)
        return &DAT_800d3f84;
    if (address == 0x800d4030u)
        return &g_scuba_tug_model_base;
    if (address == 0x800d4038u)
        return &g_scuba_tug_initial_pose;
    if (address == 0x800d404eu)
        return &g_tank_model_base;
    if (address == 0x800d4054u)
        return &g_tank_initial_pose;
    return 0;
}

/* Original boundary 0x800AB478.  Archive ownership and PSX-address
 * relocation are native concerns, but command decoding and recursion retain
 * the original function boundary. */
/* Original: FUN_800AB478. */
GDB_CALL
void resource_script_execute(const sint32 *script)
{
    RESOURCE_SCRIPT_CONTEXT *context = resource_script_context;
    uint8 *archive = 0, *direct = 0;
    sint32 archive_size = 0, direct_size = 0;
    for (;;)
    {
        sint32 command = *script++;
        if (command == -1)
        {
            ++g_psx_resource_opcode_counts[0];
            if (archive != 0)
            {
                if (hqtex_archive == archive)
                    hqtex_archive = 0;
                runtime_heap_defer_free(archive);
            }
            runtime_heap_sweep();
            return;
        }
        if (command == -2)
        {
            ++g_psx_resource_opcode_counts[1];
            sint32 index = *script++;
            const char *path = resource_path(index);
            if (archive != 0)
            {
                if (hqtex_archive == archive)
                    hqtex_archive = 0;
                runtime_heap_defer_free(archive);
                runtime_heap_sweep();
            }
            archive_size = game_file_size(path);
            archive = (uint8 *)runtime_heap_allocate_sector_aligned(archive_size);
            game_file_read(path, archive);
            hqtex_archive = archive;
            continue;
        }
        if (command == -3)
        {
            ++g_psx_resource_opcode_counts[2];
            sint32 index = *script++;
            const char *path = resource_path(index);
            if (archive != 0)
            {
                if (hqtex_archive == archive)
                    hqtex_archive = 0;
                runtime_heap_defer_free(archive);
                archive = 0;
                archive_size = 0;
                runtime_heap_sweep();
            }
            direct_size = game_file_size(path);
            direct = (uint8 *)runtime_heap_allocate_sector_aligned(direct_size);
            game_file_read(path, direct);
            direct = (uint8 *)runtime_heap_shrink(direct, direct_size);
            resource_script_direct = direct;
            resource_script_direct_size = direct_size;
            continue;
        }
        if (command == -4)
        {
            ++g_psx_resource_opcode_counts[3];
            register_direct_texture(direct, (uint32)*script++ << 10);
            continue;
        }
        if (command == -5)
        {
            ++g_psx_resource_opcode_counts[4];
            const sint32 *nested = (const sint32 *)player_assets_executable_address((uint32)*script++);
            if (archive != 0)
            {
                if (hqtex_archive == archive)
                    hqtex_archive = 0;
                runtime_heap_defer_free(archive);
                archive = 0;
                archive_size = 0;
                runtime_heap_sweep();
            }
            resource_script_execute(nested);
            continue;
        }
        if (command == -6)
        {
            ++g_psx_resource_opcode_counts[5];
            uint32 model_base_target = (uint32)*script++;
            sint32 model_count = (sint16)*script++;
            sint32 material1 = *script++;
            sint32 material2 = *script++;
            sint32 material3 = *script++;
            sint16 *model_base = (sint16 *)resource_script_writable(model_base_target);
            stage_models_queue_load(0, model_base, (sint16)model_count, material1, material2, material3, direct);
            continue;
        }
        if (command == -7)
        {
            ++g_psx_resource_opcode_counts[6];
            sint32 raw;
            for (raw = 0; g_material_resource_table[raw] != -1; raw++)
            {
                const TEXTURE_RESOURCE_RECORD *record;
                uint32 resource;
                uint16 clut;
                resource = (uint32)g_material_resource_table[raw];
                if (resource == 0)
                    continue;
                record = texture_record_by_resource(resource);
                sprite_archive_resource_load(record->name, resource, record->frames, 0, archive);
                clut = (uint16)sprite_clut_upload(resource, 0, 0, 0);
                sprite_assign_clut_range(resource, record->frames, clut);
                sync_map_texture_descriptor(raw, resource, clut);
                if (context)
                    context->loaded_materials++;
            }
            /* 800AB814..800AB894: nonzero environment +0x10 enables the
             * stage-selected 0x30400 tile.  Stage 0 selects table entry 1. */
            {
                const STAGE_ENV_RECORD_TAIL *env = (const STAGE_ENV_RECORD_TAIL *)player_assets_executable_address(0x800cd504u + (uint32)g_stage_index * 0x24u);
                const uint8 *selector = (const uint8 *)player_assets_executable_address(0x800cc454u + (uint32)g_stage_index);
                const char *name = env->background_resource ? (const char *)player_assets_executable_address(0x800cc3d0u + (uint32)*selector * 0x0du) : 0;
                if (name)
                {
                    uint16 clut;
                    sprite_archive_resource_load(name, 0x30400, 1, 0, archive);
                    clut = (uint16)sprite_clut_upload(0x30400, 0, 0, 0);
                    sprite_assign_clut_range(0x30400, 1, clut);
                }
            }
            continue;
        }
        if (command == -8)
        {
            ++g_psx_resource_opcode_counts[7];
            uint32 model_base_target = (uint32)*script++;
            sint32 model_count = *script++;
            sint32 material1 = *script++;
            sint32 material2 = *script++;
            sint32 material3 = *script++;
            uint32 pose_target = (uint32)*script++;
            const CC_ARCHIVE_HEADER *archive = (const CC_ARCHIVE_HEADER *)direct;
            uint8 *model_data = (uint8 *)archive_member_find("MODELS.BIN", archive);
            sint16 *model_base = (sint16 *)resource_script_writable(model_base_target);
            void **pose = (void **)resource_script_writable(pose_target);
            stage_models_queue_load(0, model_base, (sint16)model_count, material1, material2, material3, model_data);
            *pose = archive_member_find("POD.BIN", archive);
            continue;
        }
        if (command == -9)
        {
            ++g_psx_resource_opcode_counts[8];
            sint32 index = *script++;
            const uint32 *path_slot = (const uint32 *)player_assets_executable_address(0x800cc928u + (uint32)index * 4u);
            const char *path = (const char *)player_assets_executable_address(*path_slot);
            overlay_module_load((char *)path);
            continue;
        }
        {
            ++g_psx_resource_opcode_counts[9];
            sint32 end = *script++;
            sint32 index;
            for (index = command; index <= end; index++)
            {
                const TEXTURE_RESOURCE_RECORD *record = texture_record(index);
                uint32 resource = record->resource;
                sint32 frames = record->frames;
                uint16 clut;
                sprite_archive_resource_load(record->name, resource, frames, 1, archive);
                clut = (uint16)sprite_clut_upload(resource, 0, 0, 1);
                sprite_assign_clut_range(resource, frames, clut);
            }
            continue;
        }
    }
}

sint32 psx_finalize_stage_resources(void)
{
    sint32 index;
    for (index = 0; index < pending_stage_models_count; index++)
    {
        PendingStageModels *pending = &pending_stage_models[index];
        sint32 base = model_append(pending->data, pending->size, pending->count, pending->material1, pending->material2, pending->material3);
        if (base < 0)
            return 0;
        *pending->base = (sint16)base;
    }
    memset(pending_stage_models, 0, sizeof(pending_stage_models));
    pending_stage_models_count = 0;
    return 1;
}

sint32 psx_load_mission0_textures(const uint8 *texinfo, uint32 texinfo_size)
{
    RESOURCE_SCRIPT_CONTEXT context;
    const sint32 *script = (const sint32 *)player_assets_executable_address(0x800cccf8u);
    if (script == 0 || texinfo == 0)
        return 0;
    memset(map_textures, 0, sizeof(map_textures));
    memset(model_textures, 0, sizeof(model_textures));
    memset(pending_stage_models, 0, sizeof(pending_stage_models));
    pending_stage_models_count = 0;
    context.texinfo = texinfo;
    context.texinfo_size = texinfo_size;
    context.loaded_materials = 0;
    resource_script_context = &context;
    resource_script_execute(script);
    resource_script_context = 0;
    return context.loaded_materials;
}

GDB_CALL
sint32 psx_load_stage_textures(const uint8 *texinfo, uint32 texinfo_size, uint32 script_address)
{
    RESOURCE_SCRIPT_CONTEXT context;
    const sint32 *script = (const sint32 *)player_assets_executable_address(script_address);
    if (script == 0 || texinfo == 0)
        return 0;
    memset(map_textures, 0, sizeof(map_textures));
    memset(model_textures, 0, sizeof(model_textures));
    memset(pending_stage_models, 0, sizeof(pending_stage_models));
    pending_stage_models_count = 0;
    context.texinfo = texinfo;
    context.texinfo_size = texinfo_size;
    context.loaded_materials = 0;
    resource_script_context = &context;
    resource_script_execute(script);
    resource_script_context = 0;
    return context.loaded_materials;
}

static void trace_model_packet(const uint8 *p)
{
    static sint32 count;
    FILE *f;
    sint32 i;
    sint32 ready = !getenv("OA_LOGICAL_OT_REFERENCE_CAMERA") || (g_camera_world_x == 0xc8000 && g_camera_world_y == (sint32)0xffff9100 && g_camera_world_z == 0x68000);
    if (!getenv("OA_MODEL_PACKET_TRACE") || !ready || count >= 128)
        return;
    f = fopen("model_packets_port.log", count ? "a" : "w");
    if (!f)
        return;
    fprintf(f, "PACKET %d bucket=%d code=%#x camera=%#x,%#x,%#x model=%d xyz=%#x,%#x,%#x rot=%#x,%#x,%#x\n", count, prim_ot_bucket, p[7], g_camera_world_x, g_camera_world_y, g_camera_world_z, packet_model_id, packet_model_x, packet_model_y, packet_model_z, packet_model_ry, packet_model_rx, packet_model_rz);
    for (i = 0; i < 10; i++)
        fprintf(f, "%#010x%c", ((const uint32 *)p)[i], i == 9 ? '\n' : ' ');
    fclose(f);
    count++;
}

static void trace_hierarchy_packet(const POLY_FT3 *packet)
{
    const uint8 *p = (const uint8 *)packet;
    static sint32 count;
    FILE *file;
    sint32 index;
    if (!hierarchy_packet_trace_active || !getenv("OA_HIERARCHY_PACKET_TRACE"))
        return;
    file = fopen("hierarchy_packets_port.log", count ? "a" : "w");
    if (file == 0)
        return;
    fprintf(file, "HIER_PACKET %d maxz=%d bucket=%d code=%#x clut=%#x tpage=%#x model=%d frame=%u\n", count, prim_world_depth, prim_ot_bucket, packet->code, packet->clut, packet->tpage, g_current_model_id, g_frame_counter);
    for (index = 0; index < 10; index++)
        fprintf(file, "%#010x%c", ((const uint32 *)p)[index], index == 9 ? '\n' : ' ');
    fclose(file);
    count++;
}

void psx_submit_map_ft3(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, const sint16 *uv, sint32 texture, sint32 shade)
{
    POLY_FT3 *p;
    MapTextureSlot *s;
    if (texture < 0 || texture >= 64)
        return;
    s = &map_textures[texture];
    if (map_prim_count >= 8192)
        return;
    p = (POLY_FT3 *)map_prims[map_prim_count++];
    memset(p, 0, 40);
    p->tag = 0x07000000;
    p->code = 0x24;
    p->r0 = p->g0 = p->b0 = (uint8)shade;
    p->x0 = (sint16)x0;
    p->y0 = (sint16)y0;
    p->u0 = (uint8)(uv[0] + s->u);
    p->v0 = (uint8)(uv[1] + s->v);
    p->clut = model_clut_override ? model_clut_override : map_clut(texture);
    p->x1 = (sint16)x1;
    p->y1 = (sint16)y1;
    p->u1 = (uint8)(uv[2] + s->u);
    p->v1 = (uint8)(uv[3] + s->v);
    p->tpage = map_tpage(texture);
    p->x2 = (sint16)x2;
    p->y2 = (sint16)y2; /* FUN_80092D50 stores triangle vertex 2 UV in polygon words 12/13; words 10/11 are the unused FT4 corner. */
    p->u2 = (uint8)(uv[6] + s->u);
    p->v2 = (uint8)(uv[7] + s->v);
    trace_model_packet((const uint8 *)p);
    AddPrim(g_current_render_frame->ot + 2, p);
}

void psx_submit_map_ft4(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, const sint16 *uv, sint32 texture, sint32 shade)
{
    POLY_FT4 *p;
    MapTextureSlot *s;
    if (texture < 0 || texture >= 64)
        return;
    s = &map_textures[texture];
    if (map_prim_count >= 8192)
        return;
    p = (POLY_FT4 *)map_prims[map_prim_count++];
    memset(p, 0, 40);
    p->tag = 0x09000000;
    p->code = 0x2c;
    p->r0 = p->g0 = p->b0 = (uint8)shade;
    p->x0 = (sint16)x0;
    p->y0 = (sint16)y0;
    p->u0 = (uint8)(uv[0] + s->u);
    p->v0 = (uint8)(uv[1] + s->v);
    p->clut = model_clut_override ? model_clut_override : map_clut(texture);
    p->x1 = (sint16)x1;
    p->y1 = (sint16)y1;
    p->u1 = (uint8)(uv[2] + s->u);
    p->v1 = (uint8)(uv[3] + s->v);
    p->tpage = map_tpage(texture);
    p->x2 = (sint16)x2;
    p->y2 = (sint16)y2;
    p->u2 = (uint8)(uv[4] + s->u);
    p->v2 = (uint8)(uv[5] + s->v);
    p->x3 = (sint16)x3;
    p->y3 = (sint16)y3;
    p->u3 = (uint8)(uv[6] + s->u);
    p->v3 = (uint8)(uv[7] + s->v);
    trace_model_packet((const uint8 *)p);
    AddPrim(g_current_render_frame->ot + 2, p);
}

sint32 psx_get_map_texture_info(sint32 texture, PSXMapTextureInfo *out)
{
    MapTextureSlot *slot;
    if (out == 0 || texture < 0 || texture >= MODEL_TEXTURE_SLOT_COUNT)
        return 0;
    slot = &model_textures[texture];
    if (!slot->valid)
    {
        const VRAM_SPRITE *descriptor = (const VRAM_SPRITE *)sprite_vram_descriptor_for_resource((uint32)texture << 10);
        if (descriptor != 0)
        {
            slot->u = (uint8)descriptor->u0;
            slot->v = (uint8)descriptor->v0;
            slot->tpage = descriptor->tpage;
            slot->clut = g_resource_clut_table[texture];
            slot->valid = 1;
        }
    }
    out->tpage = slot->tpage;
    out->clut = slot->clut;
    out->u = slot->u;
    out->v = slot->v;
    out->valid = slot->valid;
    return slot->valid != 0;
}

void psx_submit_prepared_model_ft3(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, const uint8 *source, sint32 shade)
{
    POLY_FT3 *p;
    if (source == 0 || map_prim_count >= 8192)
        return;
    p = (POLY_FT3 *)map_prims[map_prim_count++];
    memcpy(p, source, 40);
    p->r0 = p->g0 = p->b0 = (uint8)shade;
    p->x0 = (sint16)x0;
    p->y0 = (sint16)y0;
    p->x1 = (sint16)x1;
    p->y1 = (sint16)y1;
    p->x2 = (sint16)x2;
    p->y2 = (sint16)y2;
    if (model_clut_override)
        p->clut = model_clut_override;
    trace_hierarchy_packet(p);
    trace_model_packet((const uint8 *)p);
    AddPrim(g_current_render_frame->ot + 2, p);
}

void psx_submit_prepared_model_ft4(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, const uint8 *source, sint32 shade)
{
    POLY_FT4 *p;
    if (source == 0 || map_prim_count >= 8192)
        return;
    p = (POLY_FT4 *)map_prims[map_prim_count++];
    memcpy(p, source, 40);
    p->r0 = p->g0 = p->b0 = (uint8)shade;
    p->x0 = (sint16)x0;
    p->y0 = (sint16)y0;
    p->x1 = (sint16)x1;
    p->y1 = (sint16)y1;
    p->x2 = (sint16)x2;
    p->y2 = (sint16)y2;
    p->x3 = (sint16)x3;
    p->y3 = (sint16)y3;
    if (model_clut_override)
        p->clut = model_clut_override;
    trace_hierarchy_packet((const POLY_FT3 *)p);
    trace_model_packet((const uint8 *)p);
    AddPrim(g_current_render_frame->ot + 2, p);
}

sint32 psx_get_cell_frame_objects(sint32 cell_x, sint32 cell_z, FrameObjectPartial **out)
{
    SpatialBucket *b;
    sint32 n = 0, i;
    if (!g_spatial_cell_bucket_indices || !g_spatial_buckets || !g_frame_object_lookup || cell_x < 0 || cell_z < 0 || cell_x >= g_map_width_cells || cell_z >= g_map_depth_cells)
        return 0;
    b = &g_spatial_buckets[g_spatial_cell_bucket_indices[cell_z * g_map_width_cells + cell_x]];
    for (i = 0; i < b->count && i < SPATIAL_BUCKET_CAPACITY; i++)
    {
        FrameObjectPartial *o = g_frame_object_lookup[b->object_indices[i]];
        if (o)
            out[n++] = o;
    } /* FUN_80094548 appends special records before performing its single stable sort. Preserve bucket insertion order here. */
    return n;
}

static void sort_frame_objects_stable(FrameObjectPartial **list, sint32 n)
{
    sint32 swapped, i;
    do
    {
        swapped = 0;
        for (i = 0; i + 1 < n; i++)
            if (ot_depth_should_swap_cell_objects(list[i]->world_z, list[i + 1]->world_z))
            {
                FrameObjectPartial *t = list[i];
                list[i] = list[i + 1];
                list[i + 1] = t;
                swapped = 1;
            }
    } while (swapped);
}

void psx_submit_frame_object(FrameObjectPartial *o, sint32 row_bucket)
{
    if (!o)
        return;
    o->ot_bucket = (sint16)ot_depth_inherit_row_bucket(o->ot_bucket, row_bucket);
    psx_ot_trace_object(o, row_bucket);
    native_ot_owner = o;
    if (o->model_plus_one)
    {
        /* FUN_80094548 writes the +0x26/+0x29 light result only to the
         * primitive referenced by FrameObject+0.  FUN_8008DDEC's model path
         * (+0x1C != 0) never feeds that result back to gp+0x2A4, so the
         * model inherits the current cell/model-layer shade unchanged. */
        model_draw_runtime_model(o->model_plus_one - 1, o->world_x, o->world_y, o->world_z, o->model_rot_y, o->model_rot_x, o->model_rot_z, row_bucket);
    }
    else if (o->prim)
    {
        sint32 object_shade;
        uint8 *prim = (uint8 *)o->prim;
        if (o->light_delta == 0 && !o->force_light)
            object_shade = cell_sprite_light;
        else
            object_shade = o->light_delta + (o->force_light ? 0x80 : cell_sprite_light);
        if (object_shade < 0)
            object_shade = 0;
        if (object_shade > 255)
            object_shade = 255;
        cell_sprite_light = object_shade;
        /* FUN_80094548 writes the current cell light into the source packet
           and FUN_8008DDEC copies that packet into the primitive arena. */
        if (getenv("OA_LIGHT_TRACE"))
        {
            static sint32 traced;
            if (traced < 8)
            {
                FILE *f = fopen("light_trace_port.log", traced ? "a" : "w");
                if (f)
                {
                    fprintf(f, "xyz=%#x,%#x,%#x sub=%d width=%d baseline=%d light=%d\n", o->world_x, o->world_y, o->world_z, o->x_subcell_offset, o->draw_env_height, g_depth_lighting_bias, cell_sprite_light);
                    fclose(f);
                }
                traced++;
            }
        }
        if (map_prim_count < 8192)
        {
            uint8 *copy = map_prims[map_prim_count++];
            memcpy(copy, prim, 40);
            copy[4] = copy[5] = copy[6] = (uint8)cell_sprite_light;
            prim = copy;
        }
        trace_sprite_packet(prim, o);
        if (o->draw_env_height > 0 && o->world_z > g_camera_world_z)
        {
            sint32 depth = o->world_z - g_camera_world_z;
            sint32 wx = o->sort_x + o->x_subcell_offset * 0x100;
            sint32 clip_x0 = 160 + (wx - g_camera_world_x) * 0x140 / depth;
            sint32 clip_x1 = 160 + ((wx + o->draw_env_height * 0x100) - g_camera_world_x) * 0x140 / depth;
            void *ot = g_current_render_frame->ot + o->ot_bucket;
            if (clip_x0 < 0)
                clip_x0 = 0;
            if (clip_x1 > FW)
                clip_x1 = FW;
            if (clip_x1 < clip_x0)
                clip_x1 = clip_x0;
            /* FUN_8008DDEC prepends restore, polygon, then clipped DR_ENV.
             * Linked-OT playback therefore sees clip -> polygon -> restore. */
            add_draw_area(ot, 0, FW);
            psx_set_prim_ot_bucket(o->ot_bucket);
            AddPrim(ot, prim);
            add_draw_area(ot, clip_x0, clip_x1);
        }
        else
        {
            psx_set_prim_ot_bucket(o->ot_bucket);
            AddPrim(g_current_render_frame->ot + o->ot_bucket, prim);
        }
    }
    native_ot_owner = 0;
    o->force_light = 0;
    o->light_delta = 0;
}

/* Original 0x8008DDEC boundary.  DAT_800D4074 is the current map-row OT
 * bucket established by FUN_80093DE0/FUN_80094548. */
/* Original: FUN_8008DDEC. */
void frame_model_submit(FrameObjectPartial *object)
{
    psx_submit_frame_object(object, g_render_depth_bucket);
}

void psx_submit_cell_objects_below(sint32 cell_x, sint32 cell_z, sint32 y_limit, sint32 row_bucket)
{
    FrameObjectPartial *list[SPATIAL_BUCKET_CAPACITY];
    sint32 i, n = psx_get_cell_frame_objects(cell_x, cell_z, list);
    sort_frame_objects_stable(list, n);
    for (i = 0; i < n; i++)
        if (!list[i]->drawn && list[i]->world_y <= y_limit)
            psx_submit_frame_object(list[i], row_bucket);
}

void psx_submit_cell_objects_remaining(sint32 cell_x, sint32 cell_z, sint32 row_bucket)
{
    FrameObjectPartial *list[SPATIAL_BUCKET_CAPACITY];
    sint32 i, n = psx_get_cell_frame_objects(cell_x, cell_z, list);
    sort_frame_objects_stable(list, n);
    for (i = 0; i < n; i++)
        if (!list[i]->drawn)
            psx_submit_frame_object(list[i], row_bucket);
}

static void log_floor_tile(const MAP_FLOOR_CELL *cell, const RV *q, const MapTextureSlot *s)
{
    static uint32 seen;
    sint32 index, x, z, bit;
    FILE *f;
    if (!getenv("OA_FLOOR_TILE_TRACE") || !g_map_floor_cells || g_map_width_cells <= 0)
        return;
    index = (sint32)(cell - g_map_floor_cells);
    x = index % g_map_width_cells;
    z = index / g_map_width_cells;
    if (z < 7 || z > 9 || x < 46 || x > 52)
        return;
    bit = (z - 7) * 7 + x - 46;
    if (seen & (1u << bit))
        return;
    seen |= 1u << bit;
    f = fopen("floor_tiles_port.csv", "a");
    if (!f)
        return;
    if (bit == 0)
        fprintf(f, "z,x,tex,flags,x0,y0,u0,v0,x1,y1,u1,v1,x2,y2,u2,v2,x3,y3,u3,v3,tpage,clut\n");
    fprintf(f, "%d,%d,%u,%u,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%u,%u\n", z, x, cell->material, cell->flags, q[0].x, q[0].y, q[0].u, q[0].v, q[1].x, q[1].y, q[1].u, q[1].v, q[2].x, q[2].y, q[2].u, q[2].v, q[3].x, q[3].y, q[3].u, q[3].v, s->tpage, s->clut);
    fclose(f);
}

void psx_draw_floor_tile(sint32 a, sint32 b, sint32 c, sint32 d, sint32 e, sint32 f, sint32 g, sint32 h, const MAP_FLOOR_CELL *cell, sint32 tile)
{
    sint32 raw, i, rot, flipx, flipy, shade, bu, bv, map_light = 0, index;
    RV q[4];
    sint32 uv[8];
    MapTextureSlot *s;
    POLY_FT4 *p;
    if (!cell || cell->material == 255)
        return;
    raw = cell->material;
    if (raw < 0 || raw >= 64)
        return;
    s = &map_textures[raw];
    if (!s->valid || map_prim_count >= 8192)
        return;
    bu = s->u + (tile & 3) * 64;
    bv = s->v + (tile >> 2) * 64;
    q[0].x = a;
    q[0].y = b;
    q[1].x = c;
    q[1].y = d;
    q[2].x = g;
    q[2].y = h;
    q[3].x = e;
    q[3].y = f; /* FUN_80093600 keeps the cyclic UV corners as TL,TR,BR,BL, but
                 writes them to PSX FT4 vertices 0,1,3,2. */
    uv[0] = bu;
    uv[1] = bv;
    uv[2] = bu + 63;
    uv[3] = bv;
    uv[4] = bu + 63;
    uv[5] = bv + 63;
    uv[6] = bu;
    uv[7] = bv + 63;
    rot = cell->packed_orientation & 7;
    for (i = 0; i < 4; i++)
    {
        sint32 n = (rot + i * 2) & 7;
        sint32 vertex = (i < 2) ? i : 5 - i;
        q[vertex].u = uv[n];
        q[vertex].v = uv[n + 1];
    }
    flipx = (cell->flags & 0x40) != 0;
    flipy = (cell->flags & 0x80) != 0;
    if (flipx)
    {
        sint32 t = q[0].u;
        q[0].u = q[1].u;
        q[1].u = t;
        t = q[2].u;
        q[2].u = q[3].u;
        q[3].u = t;
    }
    if (flipy)
    {
        sint32 t = q[0].v;
        q[0].v = q[2].v;
        q[2].v = t;
        q[1].v = q[0].v;
        q[3].v = q[2].v;
    }
    log_floor_tile(cell, q, s); /* 800936A8..8009376C: floor modulation includes the runtime
                      light stamp and the high five slope/light bits. */
    /* FUN_80094548 stores this cell's world X/Z in gp+0x278/0x280 before
   * FUN_80093600 reads them at 0x80093748..0x800937B8.  The host renderer
   * keeps that scratch-data flow local and indexes the same current cell. */
    index = (sint32)(cell - g_map_floor_cells);
    if (g_map_cell_light_values && index >= 0 && index < g_map_width_cells * g_map_depth_cells)
        map_light = g_map_cell_light_values[index];
    shade = (sint32)cell->field_05 - g_depth_lighting_bias + map_light + g_scene_brightness_bias + ((sint32)(cell->packed_orientation >> 3) * 0x10);
    if (shade < 0)
        shade = 0;
    if (shade > 255)
        shade = 255;
    p = (POLY_FT4 *)map_prims[map_prim_count++];
    memset(p, 0, 40);
    p->tag = 0x09000000;
    p->code = 0x2c;
    p->r0 = p->g0 = p->b0 = (uint8)shade;
    p->x0 = q[0].x;
    p->y0 = q[0].y;
    p->u0 = (uint8)q[0].u;
    p->v0 = (uint8)q[0].v;
    p->clut = s->clut;
    p->x1 = q[1].x;
    p->y1 = q[1].y;
    p->u1 = (uint8)q[1].u;
    p->v1 = (uint8)q[1].v;
    p->tpage = s->tpage;
    p->x2 = q[2].x;
    p->y2 = q[2].y;
    p->u2 = (uint8)q[2].u;
    p->v2 = (uint8)q[2].v;
    p->x3 = q[3].x;
    p->y3 = q[3].y;
    p->u3 = (uint8)q[3].u;
    p->v3 = (uint8)q[3].v;
    {
        const char *trace_name = getenv("OA_FLOOR_PACKET_TRACE");
        if (trace_name)
        {
            static sint32 floor_packet_count;
            static sint32 floor_packet_tick_count;
            static uint32 floor_packet_tick = 0xffffffffu;
            sint32 reference_ready = !getenv("OA_LOGICAL_OT_REFERENCE_CAMERA") || (g_camera_world_x == 0xc8000 && g_camera_world_y == (sint32)0xffff9100 && g_camera_world_z == 0x68000);
            if (floor_packet_tick != g_frame_counter)
            {
                floor_packet_tick = g_frame_counter;
                floor_packet_tick_count = 0;
            }
            /* The PAL GDB evidence records the first 45 visible water packets per
     * frame. Keep the native diagnostic sampler identical; this does not
     * affect packet creation or submission. */
            if (reference_ready && floor_packet_count < 96 && floor_packet_tick_count < 45)
            {
                FILE *trace = fopen(strcmp(trace_name, "1") == 0 ? "floor_packets_port.log" : trace_name, floor_packet_count ? "a" : "w");
                if (trace)
                {
                    uint32 *words = (uint32 *)p;
                    sint32 word;
                    index = (sint32)(cell - g_map_floor_cells);
                    fprintf(trace, "PACKET %d tick=%u camera=%#x,%#x,%#x bucket=%d cell=%p x=%d z=%d raw=%u flags=%u h0=%d v0=%d h1=%d v1=%d tile=%d\n", floor_packet_count, g_frame_counter, g_camera_world_x, g_camera_world_y, g_camera_world_z, g_render_depth_bucket, (const void *)cell, index % g_map_width_cells, index / g_map_width_cells, (uint32)cell->material, (uint32)cell->flags, cell->height, (sint32)cell->slope, (cell + 1)->height, (sint32)(cell + 1)->slope, tile);
                    for (word = 0; word < 10; ++word)
                        fprintf(trace, "%#010x%c", words[word], word == 9 ? '\n' : ' ');
                    fclose(trace);
                }
                ++floor_packet_count;
                ++floor_packet_tick_count;
            }
        }
    }
    AddPrim(g_current_render_frame->ot + 2, p);
}

/* Original CELLSDAT/model ordering boundary, 0x80094548.  The native
 * renderer stores host records instead of PSX pointers, but the arguments
 * and the conversion back to the original cell coordinates are exact. */
/* Original: FUN_80094548. */
void map_cell_objects_render(MAP_FLOOR_CELL *cell, uint8 *bucket, sint32 world_x, sint32 row_front)
{
    sint32 x = world_x >> 14;
    sint32 z = (g_map_depth_cells * 0xc000 - row_front) / 0x4000 - 1;
    MAP_MODEL_GROUP *group;
    (void)bucket;
    if (cell == 0 || x < 0 || x >= g_map_width_cells || z < 0 || z >= g_map_depth_cells)
        return;
    trace_cell_x = x;
    trace_cell_z = z;
    trace_cell = cell;
    psx_set_cell_sprite_light(cell, x, z);
    if (g_map_model_groups)
    {
        group = (MAP_MODEL_GROUP *)((uint8 *)g_map_model_groups + (uint32)cell->group_index * sizeof(*group));
        model_draw_cell(group, x, z, g_render_depth_bucket);
    }
    else
        psx_submit_cell_objects_remaining(x, z, g_render_depth_bucket);
}

/* Original floor/water packet boundary, 0x80093600.  packet is the original
 * caller-owned POLY_FT4 template; the Windows backend allocates its packet
 * from map_primitives, so only its logical fields are material here. */
/* Original: FUN_80093600. */
void map_floor_tile_render(uint32 *packet, sint32 row_back, sint32 X, MAP_FLOOR_CELL *cell)
{
    sint32 y00, y10, y01, y11, a, b, c, d, e, f, g, h, back_depth, front_depth, slope, orientation, tile, material, special, row_front;
    MAP_FLOOR_CELL *next;
    (void)packet;
    if (!cell)
        return;
    row_front = row_back - 0x4000;
    next = cell + g_map_width_cells;
    tile = cell->flags & 0x3f;
    material = g_material_resource_table[cell->material];
    special = g_stage_env ? g_stage_env->map_resource : 0;
    orientation = cell->packed_orientation >> 3;
    slope = (orientation < 4 ? -orientation : orientation - 8) * 0x100;
    /* FUN_80093600 projects this frame's four heights before modifying the two
  * current-row water heights for the next frame. Capturing them first is
  * observable: updating before projection advances only the near edge by one
  * simulation step and bends the water quad differently from the MIPS. */
    y00 = cell->height * 0x100 + slope;
    y10 = (cell + 1)->height * 0x100 + slope;
    y01 = next->height * 0x100 + slope;
    y11 = (next + 1)->height * 0x100 + slope;
    /* 800937E4..80093910: one material animates its tile and floor heights on
  * even updates. The signed velocity bytes are part of the 10-byte cells. */
    if ((sint8)cell->material != -1 && material == special)
    {
        tile = tile + ((g_frame_counter >> 2) & 7);
        if ((g_frame_counter & 1) == 0)
        {
            sint16 value = (sint16)(cell->height + cell->slope);
            cell->height = value;
            if (value < -(sint32)g_terrain_variation_limit)
                cell->slope = 1;
            if (value > 0)
                cell->slope = -1;
            if (g_material_resource_table[(cell + 1)->material] != special)
            {
                value = (sint16)((cell + 1)->height + (cell + 1)->slope);
                (cell + 1)->height = value;
                if (value < -(sint32)g_terrain_variation_limit)
                    (cell + 1)->slope = 1;
                if (value > 0)
                    (cell + 1)->slope = -1;
            }
        }
    }
    back_depth = row_back - g_camera_world_z;
    front_depth = row_front - g_camera_world_z;
    if (back_depth <= 0 || front_depth <= 0)
        return;
    /* Exact 80093620..80093BB8 projection. Coordinates are made absolute here
  * by adding FUN_8008EB88's PAL draw offset (160,64). The right edge is the
  * left projection plus 0x4000*0x140/depth + 1, exactly as DAT_800D408C and
  * DAT_800D41C4 are computed by FUN_80093DE0. */
    a = 160 + (X - g_camera_world_x) * 0x140 / back_depth;
    c = a + 0x500000 / back_depth + 1;
    g = 160 + (X - g_camera_world_x) * 0x140 / front_depth;
    e = g + 0x500000 / front_depth + 1;
    b = 64 + (y00 - g_camera_world_y) * 0x163 / back_depth;
    d = 64 + (y10 - g_camera_world_y) * 0x163 / back_depth;
    h = 64 + (y01 - g_camera_world_y) * 0x163 / front_depth + 1;
    f = 64 + (y11 - g_camera_world_y) * 0x163 / front_depth + 1;
    if ((b - 64) >= 0xe1 && (d - 64) >= 0xe1)
        return;
    if ((sint8)cell->material != -1)
    {
        /* The native AddPrim receives this row's OT pointer only when a floor
   * packet is actually emitted.  Keep the host-side packet context equally
   * scoped: an empty cell must not leak its bucket into the next primitive
   * (notably the stage background submitted after the map). */
        psx_set_prim_depth(row_back - 0x2000);
        psx_set_prim_ot_bucket(g_render_depth_bucket);
        psx_ot_trace_floor(g_render_depth_bucket, row_back, X);
        psx_draw_floor_tile(a, b, c, d, e, f, g, h, cell, tile);
        /* A missing native texture descriptor suppresses packet emission. */
        psx_set_prim_depth(-1);
        psx_set_prim_ot_bucket(-1);
    }
}

static void draw_map_cell(sint32 x, sint32 z, sint32 row_back, sint32 row_front, MAP_FLOOR_CELL *cell)
{
    uint32 packet[10];
    uint8 *bucket = 0;
    if (!cell || x < 0 || x >= g_map_width_cells)
        return;
    if (g_spatial_cell_bucket_indices && g_spatial_buckets)
        bucket = (uint8 *)&g_spatial_buckets[g_spatial_cell_bucket_indices[z * g_map_width_cells + x]];
    /* 8009425C and 80094308: object/model ordering is always processed before
  * the optional floor primitive for this cell. */
    map_cell_objects_render(cell, bucket, x * 0x4000, row_front);
    if ((sint8)cell->material != -1)
        map_floor_tile_render(packet, row_back, x * 0x4000, cell);
}

/* Original: FUN_80093DE0. */
void render_map_scene(void)
{
    sint32 z, x, row_back, row_front, far_z, left, right, depth, count, split, deferred[256], deferred_count;
    MAP_FLOOR_CELL *row;
    /* Trace-only deterministic camera paired with
  * capture-mission-model-packets-fixed.gdb.  It is unreachable unless the
  * audit environment variable is explicitly present. */
    if (getenv("OA_MODEL_PACKET_REFERENCE_CAMERA"))
    {
        g_camera_world_x = 0x3ea01;
        g_camera_world_y = (sint32)0xffff1800;
        g_camera_world_z = 0xa8700;
    }
    psx_ot_trace_begin();
    g_sprite_texture_cache_key = -1;
    DAT_800d4018 = -1;
    g_foreground_sprite_clut_offset = 0;
    g_stage_background_resource = 0;
    g_render_depth_bucket = ot_depth_row_bucket(0);
    g_render_row_world_z = 0;
    far_z = g_camera_world_z + g_effect_visibility_depth_cells * 0x100;
    if (g_scene_far_z < far_z)
        far_z = g_scene_far_z;
    row = g_map_floor_cells;
    /* Literal 0x80093E50..0x80094364 row and horizontal-cell traversal. */
    for (z = 0; z < g_map_depth_cells; z++, row += g_map_width_cells)
    {
        row_back = g_map_depth_cells * 0xc000 - z * 0x4000;
        row_front = row_back - 0x4000;
        if (row_front < 1)
        {
            psx_ot_trace_end();
            return;
        }
        g_sprite_subdivision_cooldown = 0;
        if (row_front > far_z)
            continue;
        if (g_render_row_world_z == 0)
        {
            g_render_row_world_z = row_front;
            if (frame_render_work_enabled() == 0)
            {
                psx_ot_trace_end();
                return;
            }
        }
        if (z == 0)
            g_stage_background_resource = 1;
        if (row_front < g_camera_world_z + 0x4000)
        {
            psx_ot_trace_end();
            return;
        }
        /* FUN_80093DE0: depth-dependent baseline subtracted from MAPFLOOR light. */
        {
            sint32 light_depth = (row_front - g_camera_world_z) / 0x100;
            sint16 bias = 0, mode = 0;
            if (g_stage_env)
            {
                bias = g_stage_env->display.fields.light_bias;
                mode = (sint16)g_stage_env->music.fields.flags;
            }
            light_depth += bias;
            g_depth_lighting_bias = mode ? -(light_depth / 5 + 0x80) : light_depth / 7;
        }
        /* Literal FUN_80093DE0 horizontal bounds.  More importantly, cells left of
   * screen centre are deferred and replayed in reverse.  Since AddPrim is
   * LIFO, replacing this with a simple left-to-right loop changes wall/object
  * occlusion even though every primitive has the same row bucket. */
        depth = row_front - (g_camera_world_z - 0x4000);
        g_render_row_left_world_x = g_camera_world_x - (g_screen_half_width * depth) / 320;
        left = g_render_row_left_world_x & 0xffff4000;
        if (left < 0)
            left = 0;
        right = g_camera_world_x + (g_screen_half_width * depth) / 320;
        count = ((right - left) >> 14) + 1;
        split = 1;
        deferred_count = 0;
        g_render_row_back_projection_scale = 0x500000 / (row_back - g_camera_world_z) + 1;
        g_render_row_front_projection_scale = 0x500000 / (row_front - g_camera_world_z) + 1;
        for (x = left >> 14; count > 0; x++, count--)
        {
            MAP_FLOOR_CELL *cell;
            if (x >= g_map_width_cells)
                break;
            cell = row ? row + x : 0;
            if (split && ((x * 0x4000 - (g_camera_world_x - 0x4000)) * 0x140) / (row_front - g_camera_world_z) < 0)
            {
                if (deferred_count < 256)
                    deferred[deferred_count++] = x;
                continue;
            }
            split = 0;
            draw_map_cell(x, z, row_back, row_front, cell);
        }
        while (deferred_count > 0)
        {
            x = deferred[--deferred_count];
            draw_map_cell(x, z, row_back, row_front, row + x);
        }
        g_render_depth_bucket -= 0x40;
    }
    psx_ot_trace_end();
}
