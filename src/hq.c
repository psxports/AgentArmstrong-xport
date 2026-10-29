#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_config.h"
#include "model.h"
#include "animation.h"
#include "game_sound.h"
#include "collision.h"
#include "effect_update.h"
#include "game.h"
#include "map_render.h"
#include "camera.h"
#include "input_record.h"
#include "game_math.h"
#include "hq.h"
#include "level_data.h"
#include "map.h"
#include "menu.h"
#include "object.h"
#include "game_file.h"
#include "game_runtime.h"
#include "input.h"
#include "player.h"
#include "projectile.h"
#include "psx.h"
#include "psx_spu.h"
#include "random.h"
#include "render.h"
#include "resource_table.h"
#include "runtime_heap.h"
#include "save.h"
#include "spatial.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "text_renderer.h"

/* Original data: HQ/HQ.BIN:0x800FE394..0x800FE3C7 */
static const ANIM_COMMAND world_map_animation[7] = {
    {ANIM_STEP_FRAME, {4, 8192, 0}, 0}, {ANIM_STEP_FRAME, {4, 8193, 0}, 0}, {ANIM_STEP_FRAME, {4, 8194, 0}, 0}, {ANIM_STEP_FRAME, {4, 8195, 0}, 0}, {ANIM_STEP_FRAME, {4, 8196, 0}, 0}, {ANIM_STEP_FRAME, {4, 8197, 0}, 0}, {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Private HQ actors and UI state */
typedef struct
{
    COLLISION col;
    sint16 age;
    EFFECT *marker;
    uint32 render_resource;
} HQ_OBJECT;

typedef struct
{
    COLLISION col;
    uint32 sampled_buttons;
    ANIM animation;
    sint16 briefing_active;
    sint16 briefing_scroll_y;
    sint16 briefing_extent;
    sint16 initialized;
} HQ_WORLD_MAP_SELECTOR;

/* HQ object type 0x3C callback at 0x800FC584 */

typedef struct HQ_SLIDING_HUD
{
    COLLISION col;
    sint32 closing;
} HQ_SLIDING_HUD;

typedef struct
{
    COLLISION col;
    sint16 extent;
    const char *text;
} HQ_TEXT_VIEW;

/* HQ.BIN portrait text and prompt references decoded to native strings */
typedef struct
{
    const char *text;
    const char *prompt;
} HQ_PORTRAIT_TEXT;

typedef struct
{
    COLLISION col;       /* Legacy +0x00 */
    uint32 prev_buttons; /* Legacy +0x7C */
    sint16 sel_track;    /* Legacy +0x80 */
} HQ_MUSIC_SELECTOR;

/* Original: SLES_004.74:DAT_800D3DA2 */
sint16 g_hq_map_region = 1;

/* Original: SLES_004.74:DAT_800D3DA0 */
sint16 g_hq_sel_map_node;

/* Original: SLES_004.74:DAT_800C28E4, mission graph and progress */
static const HQ_MAP_NODE hq_map_definition[HQ_MAP_NODE_COUNT + 1] = {
    {1, 0, 1, 35, 35, {-1, 2, 2, -1}, 0, 0, 2},       {2, 2, 1, 72, 49, {67, 67, -1, 1}, 0, 0, 3},     {4, 5, 1, 49, 95, {131, -1, 70, 69}, 0, 0, 255},     {9, 8, 1, 186, 46, {136, 135, 74, -1}, 0, 0, 10},   {12, 1, 1, 112, 100, {-1, 13, 139, 139}, 0, 0, 13},  {13, 7, 1, 180, 104, {-1, 78, 78, 12}, 0, 0, 14},  {67, 255, 1, 87, 5, {-1, -1, 2, -1}, 0, 0, 0},     {131, 255, 1, 3, 60, {-1, 4, 4, -1}, 0, 0, 0},    {69, 255, 1, 3, 118, {-1, 4, -1, -1}, 0, 0, 0},     {70, 255, 1, 54, 155, {4, -1, -1, -1}, 0, 0, 0},  {135, 255, 1, 252, 30, {-1, -1, 9, 9}, 0, 0, 0},    {136, 255, 1, 158, 5, {-1, -1, 9, -1}, 0, 0, 0},   {74, 255, 1, 252, 110, {9, -1, -1, 9}, 0, 0, 0},    {139, 255, 1, 98, 155, {12, 12, -1, -1}, 0, 0, 0}, {78, 255, 1, 219, 155, {13, -1, -1, 13}, 0, 0, 0}, {27, 3, 1, 128, 70, {154, -1, -1, -1}, 0, 0, 99},  {154, 255, 1, 125, 5, {-1, -1, 27, -1}, 0, 0, 0},
    {3, 9, 2, 65, 53, {68, -1, -1, 130}, 0, 0, 4},    {5, 22, 2, 55, 97, {-1, 7, -1, 132}, 0, 0, 7},   {7, 24, 2, 116, 124, {5, -1, 73, 5}, 0, 0, 9},       {20, 23, 2, 145, 45, {145, 21, 21, -1}, 0, 0, 21},  {21, 25, 2, 200, 63, {20, -1, 22, 29}, 0, 0, 255},   {22, 10, 2, 210, 109, {21, -1, 87, -1}, 0, 0, 23}, {29, 26, 2, 166, 77, {21, 21, -1, -1}, 0, 0, 255}, {130, 255, 2, 3, 41, {-1, 3, 3, -1}, 0, 0, 0},    {68, 255, 2, 84, 5, {-1, -1, 3, 3}, 0, 0, 0},       {132, 255, 2, 3, 111, {5, 5, -1, -1}, 0, 0, 0},   {73, 255, 2, 101, 155, {7, 7, -1, -1}, 0, 0, 0},    {145, 255, 2, 167, 5, {-1, -1, 20, -1}, 0, 0, 0},  {87, 255, 2, 175, 155, {22, -1, -1, -1}, 0, 0, 0},  {6, 15, 4, 48, 47, {-1, 8, -1, 388}, 0, 0, 8},     {8, 13, 4, 115, 60, {585, 28, -1, 6}, 0, 0, 255},  {28, 17, 4, 154, 64, {-1, -1, -1, 8}, 0, 0, 255},  {18, 12, 4, 79, 111, {144, 19, 143, 143}, 0, 0, 19},
    {19, 14, 4, 193, 97, {-1, 81, 81, 18}, 0, 0, 17}, {388, 255, 4, 3, 45, {-1, 6, -1, -1}, 0, 0, 0},  {585, 255, 4, 135, 5, {-1, -1, 8, -1}, 0, 0, 0},     {143, 255, 4, 3, 135, {18, 18, -1, -1}, 0, 0, 0},   {144, 255, 4, 3, 75, {-1, 18, 18, -1}, 0, 0, 0},     {81, 255, 4, 235, 155, {19, -1, -1, -1}, 0, 0, 0}, {14, 27, 5, 57, 43, {141, 16, 79, -1}, 0, 0, 255}, {16, 31, 5, 136, 55, {82, 82, -1, 14}, 0, 0, 18}, {23, 28, 5, 65, 108, {24, -1, 25, 150}, 0, 0, 255}, {24, 32, 5, 186, 60, {-1, 31, 31, 23}, 0, 0, 31}, {25, 29, 5, 150, 130, {-1, -1, -1, 23}, 0, 0, 255}, {31, 4, 5, 195, 100, {24, 90, -1, -1}, 0, 0, 26},  {141, 255, 5, 8, 5, {-1, 14, 14, -1}, 0, 0, 0},     {79, 255, 5, 3, 100, {14, 14, -1, -1}, 0, 0, 0},   {82, 255, 5, 164, 5, {-1, -1, 16, 16}, 0, 0, 0},   {150, 255, 5, 10, 155, {23, 23, -1, -1}, 0, 0, 0}, {90, 255, 5, 252, 100, {-1, -1, -1, 31}, 0, 0, 0},
    {10, 19, 3, 40, 31, {-1, 11, 11, 137}, 0, 0, 11}, {11, 18, 3, 84, 52, {76, 76, -1, 10}, 0, 0, 12}, {15, 21, 3, 74, 112, {-1, -1, 1106, 142}, 0, 0, 18}, {17, 16, 3, 155, 122, {-1, 84, 147, -1}, 0, 0, 20}, {26, 20, 3, 183, 38, {152, 159, 91, 152}, 0, 0, 27}, {137, 255, 3, 4, 58, {-1, 10, 10, -1}, 0, 0, 0},   {76, 255, 3, 103, 5, {-1, -1, 11, -1}, 0, 0, 0},   {152, 255, 3, 165, 5, {-1, -1, 26, -1}, 0, 0, 0}, {159, 255, 3, 253, 30, {-1, -1, -1, 26}, 0, 0, 0},  {142, 255, 3, 4, 104, {-1, 15, -1, -1}, 0, 0, 0}, {147, 255, 3, 155, 155, {17, -1, -1, -1}, 0, 0, 0}, {84, 255, 3, 253, 140, {-1, -1, -1, 17}, 0, 0, 0}, {1106, 255, 3, 74, 155, {15, -1, -1, -1}, 0, 0, 0}, {91, 255, 3, 253, 100, {26, -1, -1, 26}, 0, 0, 0}, {65535, 0, 0, 0, 0, {0, 0, 0, 0}, 0, 0, 0},
};

HQ_MAP_NODE g_hq_map_nodes[HQ_MAP_NODE_COUNT + 1];

/* Macros. */
#pragma optimize("", off)

#pragma optimize("", on)

#pragma optimize("", off)

#pragma optimize("", on)

#pragma optimize("", off)

#pragma optimize("", on)

#pragma optimize("", off)

#pragma optimize("", on)

#pragma optimize("", off)

#pragma optimize("", on)

/* Variables. */
/* Original: SLES_004.74:DAT_800D3DB0 */
static uint16 hq_scroll_frame_divider;
/* Original: SLES_004.74:DAT_800D3CBC, initial executable value 5000 */
static sint32 hq_attract_timeout = 5000;
/* Original: HQ/HQ.BIN:DAT_800FEAA4 */
uint8 g_hq_weapon_area_enabled;
/* Original: HQ/HQ.BIN:DAT_800FE996, retained debugger export */
__declspec(dllexport) sint16 g_hq_session_timer;
/* Original: HQ/HQ.BIN:DAT_800FE994 */
static sint16 hq_tex_cache;

/* Original: HQ/HQ.BIN:DAT_800FE990 */
static sint32 hq_interaction_countdown;

/* Original: HQ/HQ.BIN:DAT_800FEA9C */
static sint16 hq_prompt_latched;

static const char hq_loading_paths[4][12] = {"HQ\\EGT.BIN", "HQ\\FGT.BIN", "HQ\\GGT.BIN", "HQ\\SGT.BIN"};

static const HQ_PORTRAIT_TEXT hq_portrait_texts[7] = {
    {"\023LEAD DESIGNER\012\023LEAD PROGRAMMER\012\023VOCALS\012\014\001\023RAFFAELE CECCO", "RAFFAELE"}, {"\023LEAD DESIGNER\012\023LEAD ARTIST\012\023ANIMATOR\012\023MUSICIAN\012\023SOUND EFFECTS\012\023VOCALS\012\014\001\023JOLYON MYERS", "JOYLYON"}, {"\023DESIGNER\012\023GRAPHIC ARTIST\012\014\001\023MARK BENTLEY", "MARK"}, {"\023PROGRAMMER\012\0233D MEISTER\012\014\001\023PAUL MARGRAVE", "PAUL"}, {"\023DESIGNER\012\023PRODUCER\012\014\001\023ST\311PHANE KOENIG", "STEPHANE"}, {"\023VIE PRODUCER\012\014\001\023MARCUS IREMONGER", "MARCUS"}, {"\014\003\023VIE UK\012\012\012\014", "VIRGIN STAFF"},
};

/* HQ.BIN module-local storage at 0x800FE98C..0x800FEAA8. These are native
 * relocations of the original fixed addresses, not main-executable globals. */
/* Original: HQ/HQ.BIN:DAT_800FEA98 */
static sint32 hq_pending_transition;

/* Original: HQ/HQ.BIN:DAT_800FEAA0 */
static EFFECT *hq_effect_3b;

/* Original: HQ/HQ.BIN:DAT_800FEAA8 */
static EFFECT *hq_effect_3d;

/* Original: HQ/HQ.BIN:DAT_800FE988 */
static EFFECT *hq_effect_3f;

/* HQ.BIN data consumed by this state machine (0x800FADD4, 0x800FADDC and
 * 0x800FADE4..0x800FAE70), retained byte-for-byte instead of host pointers. */

void hq_heap_reset(void)
{
    hq_effect_3b = 0;
    hq_effect_3d = 0;
    hq_effect_3f = 0;
}

/* Functions. */
/* Original: HQ:FUN_800FD95C. */
GDB_CALL void hq_ambient_object_update(void *raw_object)
{
    HQ_OBJECT *object = (HQ_OBJECT *)raw_object;
    sint32 floor_y;

    ++object->age;
    if (object->age < 20)
    {
        object->col.y -= 0x400;
        floor_y = object->marker->y + 0x2000;
        if (object->col.y < floor_y)
            object->col.y = floor_y;
    }
    if (object->age >= 101)
        object->col.y += 0x400;
    if (object->age == 130)
    {
        object_destroy(object);
        object->marker->vals[0] = 0;
    }

    /* Original clears visibility flag +0x26 on the current frame object. */
    g_next_frame_object->light_delta = 0;

    render_world_spr(object->render_resource, object->col.x, object->col.y, object->col.z, 0, 0x1000, 0x1000, 0, 0, 2, 0, 0);
}

/* Original: HQ:FUN_800FDA70. */
GDB_CALL void hq_ambient_object_collect(void *raw_object, void *col_src)
{
    HQ_OBJECT *object = (HQ_OBJECT *)raw_object;
    SPRITE *reward = (SPRITE *)destroyable_object_damage_and_reward(raw_object, col_src);

    if (reward != 0)
    {
        animation_update(&reward->animation);
        reward->animation.delay = 9999;
        reward->animation.frame = object->render_resource;
        object->marker->vals[0] = 0;
    }
}

/* Original: HQ:FUN_800FC584. */
GDB_CALL void hq_sliding_hud_update(void *raw_object)
{
    HQ_SLIDING_HUD *object = (HQ_SLIDING_HUD *)raw_object;
    sint32 mirrored_x;

    if (!object->closing)
    {
        if (object->col.x != 0)
            object->col.x += 8;
        else if (g_input.held != 0)
            object->closing = 1;
    }
    if (object->closing)
    {
        object->col.x -= 8;
        if (object->col.x == -0x120)
        {
            object_destroy(object);
        }
    }

    render_scr_spr(object->col.x, 0x20, 0x25c00, 0);
    mirrored_x = object->col.x < 0 ? -object->col.x : object->col.x;
    render_scr_spr(mirrored_x, 0x20, 0x25c01, 0);
}

/* Original: FUN_800ABEA0. */
GDB_CALL sint16 hq_scrolling_text_render(const char *text, sint16 x, sint16 extent, const char *caption, sint32 automatic)
{
    sint32 width;
    if (!automatic)
    {
        if (g_input.held & PADLup)
            x += 2;
        if (g_input.held & PADLdown)
            x -= 2;
    }
    else if (++hq_scroll_frame_divider > 2)
    {
        --x;
        hq_scroll_frame_divider = 0;
    }
    if (!automatic && x > -0x50)
        x = -0x50;
    if (x < -extent)
        x = -extent;
    width = text_byte_len(caption);
    if (g_render_state.frame & 0x10)
        text_render(caption, (-width * g_text_renderer.glyph_width) / 2, g_render_state.height / 2 - 0x18);
    psx_add_draw_area_rect(g_cur_render_frame->ot + 1, 0, g_render_state.width, 0, g_render_state.height);
    text_render(text, -0x82, x);
    g_map_render.brightness = -0x78;
    psx_add_draw_area_rect(g_cur_render_frame->ot + 1, 0, g_render_state.width, 0, g_render_state.height - 0x20);
    return x;
}

/* Original: HQ:FUN_800FDBF4. */
static void hq_scrolling_text_update(void *raw_object)
{
    HQ_TEXT_VIEW *object = (HQ_TEXT_VIEW *)raw_object;
    sint16 y;
    g_player->input_mask = 0;
    g_hq_session_timer = 0;
    y = hq_scrolling_text_render(object->text, object->col.y, object->extent, g_game_text.entries[71], 1);
    object->col.y = y;
    if ((g_input.pressed & PADRup) != 0)
    {
        g_player->input_mask = -1;
        g_player->menu_resume = 1;
        object_destroy(object);
    }
}

/* Original: FUN_8008EDA4. */
GDB_CALL sint16 text_line_break_count(const char *text)
{
    sint16 lines = 0;
    for (;;)
    {
        uint8 code = *text;
        if (code == 0x0c)
        {
            text += 2;
            continue;
        }
        if (code == 0)
            return lines;
        ++text;
        if (code == 0x0a)
            ++lines;
    }
}

/* Original: HQ:FUN_800FDB78. */
static void hq_scrolling_text_open(const char *text)
{
    HQ_TEXT_VIEW *object = (HQ_TEXT_VIEW *)object_create(sizeof(*object), hq_scrolling_text_update);
    object->text = text;
    object->col.object_type = 0x8d;
    object->col.y = g_render_state.height / 2;
    object->extent = (g_text_renderer.line_height * text_line_break_count(text));
}

/* Original: HQ:FUN_800FDE34. */
GDB_CALL void hq_loading_scr_run(void)
{
    char path[12];
    sint32 size;
    void *archive;
    sint32 path_idx = 0;
    disp_buffers_initialize();
    runtime_heap_initialize();
    linked_list_initialize(&g_object_list);
    if (g_session.language == 2)
        path_idx = 1;
    if (g_session.language == 3)
        path_idx = 2;
    if (g_session.language == 4)
        path_idx = 3;
    memcpy(path, hq_loading_paths[path_idx], 11);
    size = game_file_size(path);
    archive = runtime_heap_allocate_sector_aligned(size);
    game_file_read(path, archive, size);
    music_track_select(0x0c);
    hq_scrolling_text_open(archive);
    do
    {
        vertical_sync_wait(0);
        frame_begin();
        object_list_update();
        scr_overlay_draw();
        end_frame_submit(0);
    } while ((g_input.pressed & PADRup) == 0);
    runtime_heap_defer_free(archive);
    runtime_heap_sweep();
    DrawSync(0);
}

/* Original: HQ:FUN_800FD59C. */
static sint32 hq_interaction_prompt_update(const char *prompt, sint16 blocking_object_type)
{
    PLAYER *player = g_player;
    sint32 width;

    hq_prompt_latched = 1;
    if (hq_interaction_countdown != 0)
    {
        --hq_interaction_countdown;
        if (hq_interaction_countdown == 0)
            return 1;
    }

    player->input_mask &= ~0x40u;
    if (object_find_next_by_type(0, blocking_object_type) != 0)
        return 0;
    if ((g_input.pressed & PADRdown) != 0)
    {
        hq_interaction_countdown = 2;
        return 0;
    }
    if ((g_input.held & PADRdown) != 0)
        return 0;

    if ((g_render_state.frame & 8) == 0)
        text_render(g_game_text.entries[73], -0x60, 0x40);
    width = text_byte_len(prompt) * g_text_renderer.glyph_width;
    text_render(prompt, -(width / 2), 0x50);
    return 0;
}

/* Exact HQ.BIN 800FDAD0..800FDB78: persistent type-8D portrait trigger.
 * Unlike constructors, it must not clear the source effect type. */
/* Original: HQ:FUN_800FDAD0. */
GDB_CALL void hq_portrait_trigger_update(EFFECT *effect)
{
    const HQ_PORTRAIT_TEXT *entry = &hq_portrait_texts[(sint16)effect->vals[0] - 1];
    const char *text = entry->text;
    if (effect->x - 0x2000 < g_player_world.x && g_player_world.x < effect->x + 0x2000 && effect->z < g_player_world.z)
    {
        const char *prompt = entry->prompt;
        if (hq_interaction_prompt_update(prompt, 0x8d))
            hq_scrolling_text_open(text);
    }
}

/* Original: HQ:FUN_800FD6DC. */
static void hq_background_tex_select(sint16 requested_idx)
{
    sint16 prev_idx = g_hq_map_region;
    uint32 resource_offset;
    uint32 resource;
    sint16 saved_width;
    sint16 saved_height;

    if (prev_idx == hq_tex_cache && requested_idx == -1)
        return;
    if (requested_idx != -1)
        g_hq_map_region = requested_idx;

    resource_offset = g_hq_map_region * 1024;
    if (resource_offset >= 0x1401u)
        resource_offset = 0x1400u;
    resource = 0x24400u + resource_offset;

    saved_width = g_spr_pal.cursor_x;
    saved_height = g_spr_pal.cursor_y;
    g_spr_pal.cursor_x = 0x140;
    /* 0x800D772E aliases DAT_800D75D8[0xAB], the CLUT saved for
     * resource group 0x2AC00.  It is not an independent global. */
    g_spr_pal.cursor_y = (g_spr_pal.cluts[0x2ac00 >> 10] >> 6);
    spr_clut_upload(resource, 0);
    g_spr_pal.cursor_x = saved_width;
    g_spr_pal.cursor_y = saved_height;

    /* 0x800E53B4 is descriptor_bases[0x2AC00 >> 10] in the original
     * 0x800E5108 table, not independent storage. */
    spr_upload_pix(spr_vram_desc_for_resource(0x2ac00), resource, 1);

    if (requested_idx == -1)
        hq_tex_cache = g_hq_map_region;
    else
    {
        g_hq_map_region = prev_idx;
        hq_tex_cache = -1;
    }
}

/* Original: FUN_8009C714. */
static void hq_world_map_camera_update(sint32 x, sint32 y, sint32 z)
{
    if (g_camera.world_x < x)
        g_camera.world_x += 0x200;
    if (x < g_camera.world_x)
        g_camera.world_x -= 0x200;
    if (g_camera.world_y < y)
    {
        g_camera.world_y += 0x200;
        g_camera.target_y += 0x200;
    }
    if (y < g_camera.world_y)
    {
        g_camera.world_y -= 0x200;
        g_camera.target_y -= 0x200;
    }
    if (g_camera.world_z < z)
        g_camera.world_z += 0x200;
    if (z < g_camera.world_z)
        g_camera.world_z -= 0x200;
    g_camera.render_x = g_camera.world_x < 0 ? (g_camera.world_x + 0xff) >> 8 : g_camera.world_x >> 8;
    g_camera.render_y = g_camera.world_y < 0 ? (g_camera.world_y + 0xff) >> 8 : g_camera.world_y >> 8;
    g_camera.render_z = g_camera.world_z < 0 ? (g_camera.world_z + 0xff) >> 8 : g_camera.world_z >> 8;
    g_camera.prev_player_x = g_player_world.x;
    g_camera.prev_player_y = g_player_world.y;
    g_camera.prev_player_z = g_player_world.z;
}

/* Original: SLES_004.74:FUN_80087C68. */
GDB_CALL HQ_MAP_NODE *hq_world_map_node_find_by_id(sint16 id)
{
    sint32 idx;
    for (idx = 0; idx < HQ_MAP_NODE_COUNT; ++idx)
    {
        HQ_MAP_NODE *node = g_hq_map_nodes + idx;
        if (node->signed_connection == id)
            return node;
    }
    fatal_error_with_val("NO CONNECTION", id);
    abort();
}

/* Original: SLES_004.74:FUN_80087CEC. */
HQ_MAP_NODE *hq_world_map_node_find_by_stage(sint16 stage)
{
    sint32 idx;
    for (idx = 0; idx < HQ_MAP_NODE_COUNT; ++idx)
    {
        HQ_MAP_NODE *node = g_hq_map_nodes + idx;
        if (node->stage == stage)
            return node;
    }
    fatal_error_with_val("RAF NOT FOUND", stage);
    abort();
}

/* Original: HQ:FUN_800FC37C. */
static void hq_world_map_route_draw(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1)
{
    FIXED_DDA_LINE line;
    sint32 steps = fixed_dda_initialize(x0, y0, z0, x1, y1, z1, &line) / 0xc00;
    line.step.x *= 0xc;
    line.step.y *= 0xc;
    line.step.z *= 0xc;
    while (steps-- >= 0)
    {
        sint32 den = z0 - g_camera.world_z;
        sint32 x = ((line.pos.x < 0 ? line.pos.x + 0xff : line.pos.x) >> 8) - g_camera.world_x;
        sint32 y = ((line.pos.y < 0 ? line.pos.y + 0xff : line.pos.y) >> 8) - g_camera.world_y;
        if (den)
            render_scr_spr((x * 0x140 / den), (y * 0x163 / den - 0x40), 0x2006, (g_model_draw.depth_bucket + 1));
        fixed_dda_advance(&line);
    }
}

/* Original: HQ:FUN_800FB700. */
static void hq_world_map_update(void *raw_object)
{
    HQ_WORLD_MAP_SELECTOR *object = (HQ_WORLD_MAP_SELECTOR *)raw_object;
    HQ_MAP_NODE *nodes = g_hq_map_nodes;
    HQ_MAP_NODE *sel, *target = 0;
    EFFECT *effect;
    sint32 sel_is_portal, idx;

    if (object->initialized == 0)
    {
        object->initialized = 1;
        return;
    }
    XASSERT(!(g_hq_sel_map_node < 0 || g_hq_sel_map_node >= HQ_MAP_NODE_COUNT));
    g_player->input_mask = 0;
    g_hq_session_timer = 0;
    effect = hq_effect_3b;
    hq_world_map_camera_update(effect->x + 0x6000, effect->y - 0x8000, effect->z - 0x11800);
    sel = nodes + g_hq_sel_map_node;
    g_session.next_stage = sel->stage + 1;
    sel_is_portal = (sel->connection & 0xc0) != 0;

    if (nodes[0].connection != 0xffff && object->briefing_active == 0)
    {
        for (idx = 0; idx < HQ_MAP_NODE_COUNT && object->briefing_active == 0; idx++)
        {
            HQ_MAP_NODE *node = nodes + idx;
            sint32 available = node->unlocked;
            sint32 x, y, dx, dy, z, ot, frame, route;
            SPRT *packet;
            uint8 drawn_nodes[HQ_MAP_NODE_COUNT];
            if ((node->connection & 0x80) != 0)
            {
                HQ_MAP_NODE *linked = hq_world_map_node_find_by_id((node->connection & 0x3f));
                if (linked->unlocked != 0)
                {
                    uint8 prev = node->unlocked;
                    node->unlocked = 1;
                    if (prev != 1)
                        game_progress_save();
                }
                available = node->unlocked;
            }
            if (node->region != g_hq_map_region)
                continue;
            if (!available && (node->connection & 0x80) == 0)
            {
                sint32 neighbour_idx = 0;
                while (neighbour_idx < 4)
                {
                    sint32 next_idx = node->neighbour[neighbour_idx];
                    /* Original -1 neighbour reads the zero byte at 800C28E1 */
                    if (next_idx != -1)
                    {
                        XASSERT(!(next_idx < 0 || next_idx >= HQ_MAP_NODE_COUNT));
                        available |= nodes[next_idx].unlocked;
                    }
                    ++neighbour_idx;
                }
            }
            if (idx != g_hq_sel_map_node && !available)
                continue;
            if ((g_render_state.frame & 0x10) != 0)
            {
                text_render(g_game_text.entries[70], (-g_text_renderer.glyph_width * 0x14) / 2, 0x50);
                text_render(g_game_text.entries[71], (-g_text_renderer.glyph_width * 0x18) / 2, 0x60);
            }
            route = 0x800;
            dy = 0;
            x = object->col.x + node->x * 0x100;
            y = object->col.y + node->y * 0x100;
            if (idx != g_hq_sel_map_node || sel_is_portal)
            {
                if ((node->connection & 0xc0) == 0)
                {
                    frame = node->unlocked == 1 ? 0x2009 : (node->unlocked == 2 ? 0x30800 : 0x2000);
                }
                else
                {
                    frame = (node->connection & 0x80) ? 0x2008 : 0x2007;
                    dy = 0x600;
                    route = 0;
                }
            }
            else
            {
                frame = animation_update(&object->animation);
                if (node->unlocked == 1)
                    frame = 0x2009;
                if (node->unlocked == 2)
                    frame = 0x30800;
                if (g_session.next_stage != -1)
                {
                    const char *name;
                    XASSERT(!(g_session.next_stage < 0 || g_session.next_stage >= GAME_TEXT_COUNT - 32));
                    name = g_game_text.entries[32 + g_session.next_stage];
                    sint32 text_x = ((x - g_camera.world_x) * 0x140) / (object->col.z - g_camera.world_z);
                    sint32 right;
                    right = text_x + text_byte_len(name) * g_text_renderer.glyph_width;
                    if (right > g_render_state.width / 2 - 0x10)
                        text_x -= right - (g_render_state.width / 2 - 0x10);
                    text_render(name, text_x, ((y - g_camera.world_y) * 0x163) / (object->col.z - g_camera.world_z) - 0x3c);
                }
            }
            z = object->col.z - g_camera.world_z;
            ot = 0x480 - ((g_map_render.depth_anchor_z - object->col.z + (g_map_render.depth_anchor_z < object->col.z ? 0xff : 0)) >> 8);
            g_model_draw.depth_bucket = ot;
            dx = ((x + route - g_camera.world_x) * 0x140) / z;
            dy = ((y + dy - g_camera.world_y) * 0x163) / z - 0x40;
            packet = render_scr_spr(dx, dy, frame, ot);
            if (idx == g_hq_sel_map_node)
            {
                uint8 c = (g_render_state.frame << 4) - 0x80;
                packet->rgbc.r = c;
                packet->rgbc.g = c;
                packet->rgbc.b = c;
            }
            memset(drawn_nodes, 0, sizeof(drawn_nodes));
            for (route = 0; route < 4; route++)
            {
                sint32 ni = node->neighbour[route];
                HQ_MAP_NODE *next;
                if (ni == -1)
                    continue;
                XASSERT(!(ni < 0 || ni >= HQ_MAP_NODE_COUNT));
                next = nodes + ni;
                if (next->region != g_hq_map_region || drawn_nodes[ni])
                    continue;
                if ((next->connection & 0x80) != 0)
                {
                    if (!node->unlocked)
                    {
                        if (next->unlocked)
                        {
                            drawn_nodes[ni] = 1;
                            hq_world_map_route_draw(x, y + 0x200, object->col.z, object->col.x + next->x * 0x100, object->col.y + (next->y + 2) * 0x100, object->col.z);
                        }
                        continue;
                    }
                }
                if (!node->unlocked)
                    continue;
                if (((next->connection & 0x80) == 0 || next->unlocked) && idx <= ni)
                {
                    drawn_nodes[ni] = 1;
                    hq_world_map_route_draw(x, y + 0x200, object->col.z, object->col.x + next->x * 0x100, object->col.y + (next->y + 2) * 0x100, object->col.z);
                }
            }
        }
    }

    if ((g_input.pressed & 0xf040) != 0 && object->briefing_active == 0)
    {
        sint32 old = g_hq_sel_map_node;
        sel = nodes + old;
        target = sel;
        sound_play_nonpositional(3, 0, 0x7f);
        if (sel->neighbour[0] >= 0 && (g_input.pressed & PADLup))
        {
            XASSERT(sel->neighbour[0] < HQ_MAP_NODE_COUNT);
            target = nodes + sel->neighbour[0];
            if (sel->unlocked || target->unlocked)
                g_hq_sel_map_node = target->map_idx;
        }
        if (sel->neighbour[1] >= 0 && (g_input.pressed & PADLright))
        {
            XASSERT(sel->neighbour[1] < HQ_MAP_NODE_COUNT);
            target = nodes + sel->neighbour[1];
            if (sel->unlocked || target->unlocked)
                g_hq_sel_map_node = target->map_idx;
        }
        if (sel->neighbour[2] >= 0 && (g_input.pressed & PADLdown))
        {
            XASSERT(sel->neighbour[2] < HQ_MAP_NODE_COUNT);
            target = nodes + sel->neighbour[2];
            if (sel->unlocked || target->unlocked)
                g_hq_sel_map_node = target->map_idx;
        }
        if (sel->neighbour[3] >= 0 && (g_input.pressed & PADLleft))
        {
            XASSERT(sel->neighbour[3] < HQ_MAP_NODE_COUNT);
            target = nodes + sel->neighbour[3];
            if (sel->unlocked || target->unlocked)
                g_hq_sel_map_node = target->map_idx;
        }
        if (g_hq_sel_map_node != old && (target->connection & 0x80) && !target->unlocked)
            g_hq_sel_map_node = old;
        if (sel_is_portal)
        {
            if (g_input.pressed & PADRdown)
            {
                target = hq_world_map_node_find_by_id((nodes[g_hq_sel_map_node].connection & 0x3f));
                g_hq_sel_map_node = (target - nodes);
                g_hq_map_region = target->region;
                sound_play_nonpositional(4, 0, 0x7f);
                hq_background_tex_select(-1);
            }
            g_session.next_stage = -1;
        }
        else
            g_session.next_stage = nodes[g_hq_sel_map_node].stage + 1;
    }
    else if (sel_is_portal)
        g_session.next_stage = -1;

    if ((g_input.pressed & PADRdown) != 0)
    {
        if (object->briefing_active != 0)
        {
            hq_pending_transition = 1;
        }
        if (!sel_is_portal && object->briefing_active == 0)
        {
            const char *label;
            XASSERT(!(g_session.next_stage <= 0 || g_session.next_stage > GAME_TEXT_COUNT));
            label = g_game_text.entries[g_session.next_stage - 1];
            object->briefing_active = 1;
            object->briefing_extent = (g_text_renderer.line_height * text_line_break_count(label));
        }
    }
    if (object->briefing_active != 0)
    {
        const char *label;
        XASSERT(!(g_session.next_stage <= 0 || g_session.next_stage > GAME_TEXT_COUNT));
        label = g_game_text.entries[g_session.next_stage - 1];
        if ((g_input.pressed & PADRup) != 0)
        {
            object->briefing_active = 0;
            return;
        }
        object->briefing_scroll_y = hq_scrolling_text_render(label, object->briefing_scroll_y, object->briefing_extent, g_game_text.entries[134], 0);
    }
    else
        object->briefing_scroll_y = -0x50;
    if ((g_input.pressed & PADRup) != 0)
    {
        if (object_destroy_all_by_type(0x3a) != 0)
            g_camera.frozen = 0;
        hq_background_tex_select(0);
        g_player->input_mask = -1;
        g_player->menu_resume = 1;
    }
}

/* Original: HQ:FUN_800FB648. */
static void hq_world_map_open(sint32 x, sint32 y, sint32 z)
{
    HQ_WORLD_MAP_SELECTOR *object;
    if (object_find_next_by_type(0, 0x3a) != 0)
        return;
    object = (HQ_WORLD_MAP_SELECTOR *)object_create(sizeof(*object), hq_world_map_update);
    object->col.object_type = 0x3a;
    object->col.x = x;
    object->col.y = y;
    object->col.z = z;
    g_camera.frozen = 1;
    animation_start(&object->animation, world_map_animation);
    object->briefing_scroll_y = -0x50;
    object->sampled_buttons = g_input.held;
    hq_background_tex_select(-1);
}

/* Original: HQ:FUN_800FC81C. */
GDB_CALL void hq_gramophone_close(void)
{
    PLAYER *player = g_player;
    HQ_MUSIC_SELECTOR *removed = (HQ_MUSIC_SELECTOR *)object_destroy_all_by_type(0x3d);
    if (removed == 0)
        return;
    if (removed->sel_track != 2)
        music_track_select(2);
    player->input_mask = -1;
    player->menu_resume = 1;
}

/* Original: HQ:FUN_800FC6DC. */
GDB_CALL void hq_gramophone_update(void *raw_object)
{
    HQ_MUSIC_SELECTOR *object = (HQ_MUSIC_SELECTOR *)raw_object;
    PLAYER *player = g_player;
    uint32 pressed = g_input.held & ~object->prev_buttons;
    object->prev_buttons = g_input.held;
    player->input_mask = 0;
    if ((pressed & 0x5000) != 0)
    {
        if ((pressed & 0x1000) != 0)
            ++object->sel_track;
        if ((pressed & 0x4000) != 0)
            --object->sel_track;
        sound_play_nonpositional(0x68, 0, 0x7f);
        if (object->sel_track >= 0x11)
            object->sel_track = 2;
        if (object->sel_track < 2)
            object->sel_track = 0x10;
        music_track_select(object->sel_track);
    }
    text_render(g_game_text.entries[71], 0, 0x20);
    text_render(g_game_text.entries[72], -0x60, 0);
    text_render(integer_to_text(object->sel_track), 0, g_text_renderer.glyph_height + 4);
    g_hq_session_timer = 0;
    if ((g_input.pressed & PADRup) != 0)
        hq_gramophone_close();
}

/* Original: HQ:FUN_800FC660. */
GDB_CALL void hq_gramophone_open(void)
{
    HQ_MUSIC_SELECTOR *object;
    if (object_find_next_by_type(0, 0x3d) != 0)
        return;
    object = (HQ_MUSIC_SELECTOR *)object_create(sizeof(*object), hq_gramophone_update);
    object->col.object_type = 0x3d;
    object->sel_track = 3;
    object->prev_buttons = g_input.held;
    music_track_select(object->sel_track);
}

/* Original: HQ:FUN_800FC538. */
GDB_CALL void hq_sliding_hud_create(void)
{
    HQ_SLIDING_HUD *object;

    if (object_find_next_by_type(0, 0x3c) != 0)
        return;
    object = (HQ_SLIDING_HUD *)object_create(sizeof(*object), hq_sliding_hud_update);
    object->col.x = -0x120;
    object->col.object_type = 0x3c;
}

/* Original: HQ:sub_800FD8B0. */
static void ambient_object_create(EFFECT *src)
{
    HQ_OBJECT *object = (HQ_OBJECT *)object_create(sizeof(*object), hq_ambient_object_update);

    object->col.x = src->x;
    object->col.y = src->y + 0x5000u;
    object->col.z = src->z + 0x2100u;
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = hq_ambient_object_collect;
    object->marker = src;
    src->vals[0] = 1;
    object->render_resource = random_range(1) + 0x2f400;
    col_box_set(object, 0x30, 0x50, 0x10);
}

/* Original: HQ:FUN_800FD83C. */
GDB_CALL void hq_ambient_spawn_update(void)
{
    EFFECT *src;
    sint32 idx;

    if ((g_render_state.frame & 0x3f) != 0)
        return;
    idx = random_range(7);
    src = effect_find_next(0x83, 0);
    while (idx-- > 0)
        src = effect_find_next(0x83, src + 1);
    if (src->vals[0] == 0)
        ambient_object_create(src);
}

/* Original: HQ:FUN_800FAE70. */
void hq_level_update(void)
{
    hq_tex_cache = -1;
    g_hq_session_timer = 0;
    hq_interaction_countdown = 0;
    hq_pending_transition = 0;
    hq_effect_3b = effect_find_next(0x3b, 0);
    hq_effect_3d = effect_find_next(0x3d, 0);
    hq_effect_3f = effect_find_next(0x3f, 0);

    hq_sliding_hud_create();
    hq_background_tex_select(0);

    while (!xport_isquit())
    {
        hq_ambient_spawn_update();
        g_map_render.brightness = 0;
        room_camera_limits_update();
        /* 800FAF20..800FAF38.  The common inactivity counter first restores
         * the two AGNTLOGO halves; its later threshold selects attract mode. */
        if (g_hq_session_timer == 800)
            hq_sliding_hud_create();

        if (g_camera.prev_z != -1)
        {
            g_camera.prev_x = g_camera.world_x;
            g_camera.prev_y = g_camera.world_y;
            g_camera.prev_z = g_camera.world_z;
        }

        frame_begin();
        spatial_buckets_reset();
        g_next_frame_object = g_frame_objects;
        hq_prompt_latched = 0;
        g_next_transient_prim = g_transient_prims;
        effects_update_visible();
        object_list_update();
        if (g_surface_impact_cooldown != 0)
            --g_surface_impact_cooldown;
        object_cols_dispatch();
        spatial_buckets_build();

        if (g_player_muzzle_light.pending != 0)
            map_light_stamp_apply(g_player_world.x + g_player_muzzle_light.dir_x * 0x4000, g_player_world.z + g_player_muzzle_light.dir_z * 0x4000, 0, &g_player_muzzle_light_stamp);
        render_map_scene();
        g_player_muzzle_light.pending = 0;
        map_light_stamp_apply(g_player_world.x + g_player_muzzle_light.dir_x * 0x4000, g_player_world.z + g_player_muzzle_light.dir_z * 0x4000, 0x80, &g_player_muzzle_light_stamp);

        g_map_render.background_resource = g_stage_env->background_resource;
        if (g_map_render.background_resource != 0)
            render_stage_background();
        g_input.prev = g_input.held;
        g_map_render.far_z = g_map.depth * 0xc000;

        /* 800FB10C..800FB2E8: the three HQ interaction volumes. */
        if (hq_effect_3b != 0 && g_player_world.x > hq_effect_3b->x - 0x2000 && g_player_world.x < hq_effect_3b->x + 0xe000 && g_player_world.z > hq_effect_3b->z)
        {
            const char *prompt = g_game_text.entries[67];
            if (hq_interaction_prompt_update(prompt, 0x3a))
                hq_world_map_open(hq_effect_3b->x - 0x2000, -0xc000, hq_effect_3b->z + 0x1f00);
        }
        else
        {
            if (object_destroy_all_by_type(0x3a) != 0)
                g_camera.frozen = 0;
        }

        if (hq_effect_3d != 0 && g_player_world.x > hq_effect_3d->x - 0x3000 && g_player_world.x < hq_effect_3d->x + 0x3000 &&
            /* 800FB1E4 SLT player.z, marker.z; BNE skips when below. */
            g_player_world.z >= hq_effect_3d->z)
        {
            const char *prompt = g_game_text.entries[68];
            if (hq_interaction_prompt_update(prompt, 0x3d))
                hq_gramophone_open();
        }
        else
        {
            hq_gramophone_close();
        }

        end_frame_submit(g_map_render.background_resource);

        /* 800FB2F8..800FB400.  Preserve the original outer-session state
         * transition stores even though RECORD.BIN playback is a separate
         * dependency of the caller after this function returns. */
        if (g_input.held != 0)
        {
            g_hq_session_timer = 0;
            hq_attract_timeout = 5000;
        }
        else if (g_input_rec.mode != INPUT_RECORD_WRITE)
        {
            g_hq_session_timer = (g_hq_session_timer + 1);
        }
        if (g_hq_session_timer == hq_attract_timeout)
        {
            XASSERT(!(g_session.attract_demo_idx < 0 || g_session.attract_demo_idx >= ATTRACT_DEMO_COUNT));
            g_session.stage = g_attract_demos[g_session.attract_demo_idx].stage;
            if (g_session.stage == -1)
            {
                g_session.attract_demo_variant = (g_session.attract_demo_variant ^ 1);
                g_session.next_stage = 0;
                g_session.play_fmv = 1;
                hq_attract_timeout = 1250;
                g_session.attract_demo_idx = 0;
                g_session.next_attract_mode = g_session.attract_demo_variant;
            }
            else
            {
                g_session.next_stage = g_session.stage + 1;
                hq_attract_timeout = 1250;
                g_input_rec.mode = INPUT_RECORD_PLAYBACK;
            }
            break;
        }

        if (hq_pending_transition != 0 && g_session.next_stage != -1)
        {
            if (g_session.next_stage == 0)
                g_session.next_stage = 1;
            g_player_world.saved_x = g_player_world.x;
            g_player_world.saved_y = g_player_world.y;
            g_player_world.saved_z = g_player_world.z;
            break;
        }

        if ((g_input.pressed & PADselect) != 0 && hq_prompt_latched == 0 && object_find_next_by_type(0, 0x3f) == 0 && g_render_state.frame >= 9)
        {
            if (g_level_select_cheat_enabled != 0)
            {
                if (menu_run(g_hq_level_menu, 0, 0) == 0x4c)
                {
                    g_session.next_stage = g_hq_level_menu[g_menu_selection_idx].choice + 1;
                    g_player_world.saved_x = g_player_world.x;
                    g_player_world.saved_y = g_player_world.y;
                    g_player_world.saved_z = g_player_world.z;
                    break;
                }
            }
            else
            {
                menu_run(g_hq_menu, 0, 0);
            }
        }

        debug_input_update();
        if (object_find_next_by_type(0, 0x3f) == 0 && object_find_next_by_type(0, 0x3a) == 0)
            pause_menu_update();

        /* 0x800FB5B4..0x800FB620: HQ X-cell state consumed directly by
         * FUN_8009E29C and the player's item/action branches. Division is
         * signed truncation toward zero, exactly as the MIPS bias/shift. */
        sint32 player_cell_x = g_player_world.x;
        if (player_cell_x < 0)
            player_cell_x += 0x3fff;
        player_cell_x >>= 14;
        g_hq_weapon_area_enabled = (player_cell_x >= 4 && player_cell_x < 15) ? 1 : 0;
        if ((player_cell_x >= 21 && player_cell_x < 26))
            g_debug_cheats_enabled = 1;

        disp_shake_update();
    }
}

/* 0x80087B60..0x80087C64.  The executable stores neighbour mission IDs in
 * each 0x14-byte record.  Startup resolves them in place to record indices;
 * the byte at +0x10 receives the record's own index.  IDA's long multiply /
 * negate / shift sequences are the compiler's signed division by 0x14. */
/* Original: SLES_004.74:FUN_80087B60. */
GDB_CALL void hq_world_map_relocate_nodes(void)
{
    sint32 idx;
    memcpy(g_hq_map_nodes, hq_map_definition, sizeof(g_hq_map_nodes));
    g_hq_sel_map_node = 0;
    g_hq_map_region = 1;
    for (idx = 0; idx < HQ_MAP_NODE_COUNT; ++idx)
    {
        HQ_MAP_NODE *node = g_hq_map_nodes + idx;
        sint32 dir;
        for (dir = 0; dir < 4; ++dir)
        {
            sint16 id = node->neighbour[dir];
            if (id != -1)
            {
                const HQ_MAP_NODE *linked = hq_world_map_node_find_by_id(id);
                node->neighbour[dir] = (linked - g_hq_map_nodes);
                node->map_idx = idx;
            }
        }
    }
}
