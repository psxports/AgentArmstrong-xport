#include <string.h>
#include <stdlib.h>

#include "object.h"
#include "sprite.h"
#include "game.h"
#include "game_loop.h"
#include "game_runtime.h"
#include "game_sound.h"
#include "input.h"
#include "menu.h"
#include "mission.h"
#include "player.h"
#include "render.h"
#include "sprite_renderer.h"
#include "text_renderer.h"

/* Original: SLES_004.74:DAT_800D3CDC */
sint32 g_menu_selection_idx;
static const char *menu_heading;

/* Original: SLES_004.74:DAT_800C8054 */
MENU_ENTRY g_language_menu[] = {
    {1, 28, MENU_SPRITE, 0, 0, 0x6c00, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {2, 28, MENU_SPRITE, 0, 0, 0x6c01, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {3, 28, MENU_SPRITE, 0, 0, 0x6c03, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {4, 28, MENU_SPRITE, 0, 0, 0x6c02, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

/* Original: SLES_004.74:DAT_800C7E94 */
MENU_ENTRY g_pause_menu[] = {
    {70, 6, MENU_ACTION, 0, 0, 0, 0, 0}, {65, 0, MENU_ACTION, 0, 0, 0, 0, 0}, {67, 2, MENU_ACTION, 0, 0, 0, 0, 0}, {66, 1, MENU_ACTION, 0, 0, 0, 0, 0}, {68, 3, MENU_SLIDER, 127, 0, 0, &g_sfx_volume_setting, 0}, {69, 4, MENU_SLIDER, 127, 0, 0, &g_music_volume_setting, 0}, {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

/* Original: SLES_004.74:DAT_800C7F04 */
MENU_ENTRY g_level_pause_menu[] = {
    {70, 6, MENU_ACTION, 0, 0, 0, 0, 0}, {65, 0, MENU_ACTION, 0, 0, 0, 0, 0}, {67, 2, MENU_ACTION, 0, 0, 0, 0, 0}, {66, 1, MENU_ACTION, 0, 0, 0, 0, 0}, {68, 3, MENU_SLIDER, 127, 0, 0, &g_sfx_volume_setting, 0}, {69, 4, MENU_SLIDER, 127, 0, 0, &g_music_volume_setting, 0}, {76, 5, MENU_ACTION, 0, 0, 0, 0, g_game_text.entries + 33}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

/* Original: SLES_004.74:DAT_800C7F94 */
MENU_ENTRY g_death_menu[] = {
    {0, 7, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {67, 8, MENU_ACTION, 0, 0, 0, 0, 0}, {66, 9, MENU_ACTION, 0, 0, 0, 0, 0}, {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

/* Original: SLES_004.74:DAT_800C7FE4 */
MENU_ENTRY g_result_menu[] = {
    {0, 10, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 11, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {75, 12, MENU_ACTION, 0, 0, 0, 0, 0}, {67, 13, MENU_ACTION, 0, 0, 0, 0, 0}, {66, 14, MENU_ACTION, 0, 0, 0, 0, 0}, {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

/* Original: HQ.BIN:DAT_800FE2F4 */
MENU_ENTRY g_hq_level_menu[] = {
    {68, 3, MENU_SLIDER, 127, 0, 0, &g_sfx_volume_setting, 0}, {69, 4, MENU_SLIDER, 127, 0, 0, &g_music_volume_setting, 0}, {76, 5, MENU_ACTION, 0, 0, 0, 0, g_game_text.entries + 33}, {0, 27, MENU_LABEL, 0, 0, 0, 0, 0}, {0, 17, MENU_ACTION, 0, 0, 0, 0, 0}, {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

/* Original: HQ.BIN:DAT_800FE354 */
MENU_ENTRY g_hq_menu[] = {
    {68, 3, MENU_SLIDER, 127, 0, 0, &g_sfx_volume_setting, 0},
    {69, 4, MENU_SLIDER, 127, 0, 0, &g_music_volume_setting, 0},
    {0, 17, MENU_ACTION, 0, 0, 0, 0, 0},
    {-1, 0, MENU_ACTION, 0, 0, 0, 0, 0},
};

static void backdrop(sint32 left, sint32 top, sint32 width, sint32 height)
{
    RENDER_PACKET_ARENA *arena = g_gpu_packets;
    POLY_G4 gradient;
    POLY_F4 black;
    POLY_G4 *poly0;
    POLY_F4 *poly1;
    sint32 y0 = top - g_render_state.height / 2 + 0x41;
    sint32 y1 = y0 + height - 2;
    SetPolyG4(&gradient);
    gradient.p[0].rgbc.r = 0x78;
    gradient.p[0].rgbc.g = 0;
    gradient.p[0].rgbc.b = 0;
    gradient.p[1].rgbc.r = 0x78;
    gradient.p[1].rgbc.g = 0x3c;
    gradient.p[1].rgbc.b = 0;
    gradient.p[2].rgbc.r = 0x78;
    gradient.p[2].rgbc.g = 0x3c;
    gradient.p[2].rgbc.b = 0;
    gradient.p[3].rgbc.r = 0xff;
    gradient.p[3].rgbc.g = 0xff;
    gradient.p[3].rgbc.b = 0;
    gradient.p[0].xy.x = gradient.p[2].xy.x = (left + 1);
    gradient.p[1].xy.x = gradient.p[3].xy.x = (left + width - 1);
    gradient.p[0].xy.y = gradient.p[1].xy.y = y0;
    gradient.p[2].xy.y = gradient.p[3].xy.y = y1;
    SetPolyF4(&black);
    black.rgbc.r = black.rgbc.g = black.rgbc.b = 0;
    black.p[0].x = black.p[2].x = left;
    black.p[1].x = black.p[3].x = (left + width - 1);
    black.p[0].y = black.p[1].y = (y0 - 1);
    black.p[2].y = black.p[3].y = (y1 + 1);
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(gradient) > RENDER_PRIM_CAPACITY - arena->used));
    poly0 = (POLY_G4 *)(arena->data.bytes + arena->used);
    memcpy(poly0, &gradient, sizeof(gradient));
    arena->used += sizeof(gradient);

    AddPrim(g_cur_render_frame->ot + 1, poly0);
    psx_add_draw_area_rect(g_cur_render_frame->ot + 1, left + 160, left + 160 + width, top, top + height);
    XASSERT(!(arena == 0 || arena->used > RENDER_PRIM_CAPACITY || sizeof(black) > RENDER_PRIM_CAPACITY - arena->used));
    poly1 = (POLY_F4 *)(arena->data.bytes + arena->used);
    memcpy(poly1, &black, sizeof(black));
    arena->used += sizeof(black);

    AddPrim(g_cur_render_frame->ot + 1, poly1);
}

/* Direct translation of 0x800894B4..0x8008A048.  The original clipped
 * DRAWENV packets surrounding the two backdrop polygons are represented by
 * the host ordering-table packets themselves; menu dimensions, row layout,
 * selection, sliders/lists/icons and all game-state writes follow the MIPS. */
/* Original: SLES_004.74:FUN_800894B4. */
GDB_CALL sint16 menu_run(MENU_ENTRY *entries, sint32 x, sint32 y)
{
    MISSION_HUD_BAR slider;
    sint32 count = 0, step = 1, frame = 0, slider_active = -1;
    sint32 width = 0, height, left, top, row;
    MENU_ENTRY *sel;
    PSX_RECT scr;
    sound_all_voices_stop();
    cd_audio_pause();
    scr.x = 0;
    scr.y = 0;
    scr.w = 0x140;
    scr.h = g_render_state.height;
    g_input.prev = g_input.held;
    g_disp_shake_ticks = 0;
    MoveImage(&scr, 0, g_render_state.height);
    DrawSync(0);
    g_menu_selection_idx = -1;
    while (entries[count].result != -1)
    {
        sint32 text_width = text_byte_len(g_game_text.entries[GAME_TEXT_MENU_FIRST + entries[count].text_idx]);
        if (width < text_width)
            width = text_width;
        ++count;
    }
    width = (width + 2) * g_text_renderer.glyph_width;
    height = (count + 2) * g_text_renderer.line_height;
    left = x != 0 ? x : -width / 2;
    top = y != 0 ? y : -height / 2 + g_render_state.height / 2;
    for (;;)
    {
        frame_begin();
        if (menu_heading != 0)
        {
            g_text_renderer.color_idx = 2;
            text_render(menu_heading, -15 * g_text_renderer.glyph_width / 2, 2 * g_text_renderer.line_height - g_render_state.height / 2);
            psx_add_draw_area_rect(g_cur_render_frame->ot + 1, 0, g_render_state.width, 0, g_render_state.height);
        }
        disp_shake_update();
        ++frame;
        --g_render_state.frame;
        if ((g_input.pressed & (PADLup | PADLdown)) != 0)
        {
            sound_play_nonpositional(g_session.scuba_active == 0 ? 3 : 0xa2, 0, g_session.scuba_active == 0 ? 0x7f : 100);
        }
        if ((g_input.pressed & PADLup) != 0)
            step = -1;
        if ((g_input.pressed & PADLdown) != 0)
            step = 1;
        do
        {
            g_menu_selection_idx += step;
            if (g_menu_selection_idx < 0)
                g_menu_selection_idx = count - 1;
            if (g_menu_selection_idx >= count)
                g_menu_selection_idx = 0;
        } while (entries[g_menu_selection_idx].kind == MENU_LABEL);
        step = 0;
        sel = &entries[g_menu_selection_idx];

        if ((g_input.pressed & (PADLup | PADLdown)) != 0 || slider_active == -1)
        {
            if (sel->kind == MENU_SLIDER)
            {
                slider_active = 1;
                hud_bar_initialize(&slider, left + (width - sel->limit) / 2, top + height - g_render_state.height / 2 - 0x0b, 0x7f, sel->limit);
            }
            else
                slider_active = 0;
        }
        if (slider_active != 0)
        {
            sint32 *val = sel->val;
            hud_bar_render(&slider, *val, 0, 0x7f);
            if ((g_input.held & PADLleft) != 0)
                --*val;
            if ((g_input.held & PADLright) != 0)
                ++*val;
            if (*val < 0)
                *val = 0;
            if (*val > sel->limit)
                *val = sel->limit;
            if (sel->result == 'D')
                g_sfx_playback_volume = g_sfx_volume_setting = *val;
            if (sel->result == 'E')
            {
                sint32 volume;
                uint8 scale = g_music_track_volumes[g_cur_music_track];
                g_music_volume_setting = *val;
                volume = scale * *val;
                if (volume < 0)
                    volume += 0x7f;
                volume >>= 7;
                SsSetSerialVol(SS_SERIAL_A, volume, volume);
            }
        }
        if (sel->result == 'L')
        {
            const char *const *list = sel->choices;
            sint32 list_count = 0;
            while (list[list_count] != 0)
                ++list_count;
            if ((g_input.pressed & PADLleft) != 0)
            {
                --sel->choice;
                if (list[sel->choice] == g_game_text.unavailable_stage)
                    --sel->choice;
            }
            if ((g_input.pressed & PADLright) != 0)
            {
                ++sel->choice;
                if (list[sel->choice] == g_game_text.unavailable_stage)
                    ++sel->choice;
            }
            if (sel->choice < 0)
                sel->choice = list_count - 1;
            if (sel->choice >= list_count)
                sel->choice = 0;
            g_text_renderer.color_idx = 5;
            text_render(list[sel->choice], left + g_text_renderer.glyph_width, top + g_text_renderer.line_height * (g_menu_selection_idx + 2) - g_render_state.height / 2);
        }

        row = top + g_text_renderer.line_height - g_render_state.height / 2;
        for (x = 0; x < count; x++)
        {
            MENU_ENTRY *entry = &entries[x];
            if (entry->kind == MENU_SPRITE)
            {
                SPRT *icon = render_scr_spr((left + g_text_renderer.glyph_width * 3), row, entry->resource, 0);
                row += 12;
                if (x == g_menu_selection_idx)
                    icon->rgbc.r = icon->rgbc.g = icon->rgbc.b = (frame * 8);
                continue;
            }
            if (x == g_menu_selection_idx && (frame & 8) != 0)
                text_render(">", left, row);
            if (entry->kind == MENU_LABEL)
                g_text_renderer.color_idx = 4;
            text_render(g_game_text.entries[GAME_TEXT_MENU_FIRST + entry->text_idx], left + g_text_renderer.glyph_width, row);
            row += g_text_renderer.line_height;
        }
        backdrop(left, top, width, height);
        end_frame_submit(1);
        g_sound_handles_invalidated = 1;
        if ((g_input.pressed & PADRdown) != 0 || xport_isquit())
            break;
    }
    cd_audio_play();
    sound_play_nonpositional(g_session.scuba_active == 0 ? 4 : 0xa3, 0, g_session.scuba_active == 0 ? 0x7f : 100);
    DrawSync(0);
    /* Boot language selection precedes player allocation */
    if (g_player != 0)
        g_player->menu_resume = 1;
    return sel->result;
}

sint16 language_menu_run(void)
{
    sint16 language;
    menu_heading = "SELECT LANGUAGE";
    language = menu_run(g_language_menu, 0, 42);
    menu_heading = 0;
    return language;
}
