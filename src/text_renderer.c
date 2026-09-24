#include <string.h>
#include "global.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "resource_table.h"
#include "sprite.h"
#include "stubs.h"
#include "text_renderer.h"

/* Functions. */
static sint32 div2_trunc(sint32 value)
{
    return value < 0 ? (value + 1) / 2 : value / 2;
}

static sint32 div4_trunc(sint32 value)
{
    return value < 0 ? (value + 3) / 4 : value / 4;
}

/* Original: FUN_8008EDEC. */
sint32 text_byte_length(const char *text)
{
    return (sint32)strlen(text);
}

/* Original: FUN_800AFF18. */
uint32 text_decode_single_byte_glyph(uint32 code)
{
    const uint16 *special = (const uint16 *)player_assets_executable_address(0x800cda64u);
    sint32 index;
    for (index = 0; index < 0x21; index++)
        if ((code & 0xffffu) == special[index])
            return (uint32)(index + 0x20);
    if (((code + 0x7db1u) & 0xffffu) < 10u)
        return (code + 0xe1u) & 0xffu;
    if (((code + 0x7da0u) & 0xffffu) < 0x1au)
        return (code + 0xe1u) & 0xffu;
    if (((code + 0x7d7fu) & 0xffffu) < 0x1au)
        return (code + 0xe0u) & 0xffu;
    return 0;
}

/* Original: FUN_8008EE0C. */
uint32 text_decode_sjis_glyph(uint16 code)
{
    const sint16 *entry = (const sint16 *)player_assets_executable_address(0x800c8450u);
    uint32 converted = text_decode_single_byte_glyph(code) & 0xffu;
    if (converted != 0)
        return converted;
    while ((uint16)entry[0] != 0xffffu)
    {
        if ((uint16)entry[0] == code)
            return (uint32)(sint16)(entry[1] + g_font_glyph_columns * 4 + 0x21);
        entry += 2;
    }
    fatal_error_with_value("SJIS NOT FOUND", code);
    return 0;
}

static void set_text_color(SPRT *packet, uint32 index)
{
    const uint8 *rgb = (const uint8 *)player_assets_executable_address(0x800c8438u + index * 3u);
    packet->r0 = rgb[0];
    packet->g0 = rgb[1];
    packet->b0 = rgb[2];
}

static void submit_font_mode(const FONT *font)
{
    DR_MODE mode;
    DR_MODE *copy;
    SetDrawMode(&mode, 0, 0, font->tpage, 0);
    copy = (DR_MODE *)g_gpu_packet_cursor;
    memcpy(copy, &mode, sizeof(mode));
    AddPrim(g_current_render_frame->ot + 1, copy);
    g_gpu_packet_cursor += sizeof(mode);
}

static sint32 centered_line_width(const uint8 *text)
{
    uint32 consumed = 0;
    sint32 width = 0;
    while (text[consumed] != 0 && text[consumed] != '\n')
    {
        uint8 c = text[consumed];
        if ((uint8)(c + 0x7f) < 0x20u || (c & 0xf8u) == 0x98u)
        {
            consumed += 2;
            width += g_font_glyph_width;
        }
        else
        {
            if (c == ' ')
                width += div2_trunc(g_font_glyph_width) + div4_trunc(g_font_glyph_width);
            else
                width += g_font_glyph_width;
            consumed += 1;
        }
    }
    return (sint16)width;
}

/* Original: FUN_8008EECC. */
GDB_CALL void text_render(const char *source, sint32 initial_x, sint32 initial_y)
{
    uint8 *text = (uint8 *)source;
    FONT *font = g_font;
    SPRT packet;
    sint32 pen_x = (sint16)initial_x;
    sint32 pen_y = (sint16)initial_y + 0x40;
    sint32 initial_color = g_text_color_index;
    SetSprt(&packet);
    set_text_color(&packet, (uint32)initial_color);
    g_text_color_index = (sint16)(g_text_color_index * 3);
    packet.w = (sint16)g_font_glyph_width;
    packet.h = (sint16)g_font_glyph_height;
    packet.x0 = (sint16)pen_x;
    packet.y0 = (sint16)pen_y;
    packet.u0 = (uint8)font->u0;
    packet.v0 = (uint8)font->v1;
    packet.clut = (uint16)g_font_clut;

    for (;;)
    {
        uint32 code = *text++;
        sint32 sjis = 0;
        if (code == 0)
            break;
        if ((code - 0x81u) < 0x20u || (code & 0xf8u) == 0x98u)
        {
            uint8 first = (uint8)code;
            --text;
            if ((first & 0xf8u) == 0x98u)
                code = ((uint32)first << 8 | text[1]) & 0x7ffu;
            else
            {
                code = text_decode_sjis_glyph((uint16)((uint32)first << 8 | text[1]));
                text[0] = (uint8)(((sint16)code + 0x9800) >> 8);
                text[1] = (uint8)code;
            }
            sjis = 1;
            text += 2;
        }
        if (!sjis && ((code - 0xa1u) & 0xffffu) < 0x3cu)
        {
            const uint8 *map = (const uint8 *)player_assets_executable_address(0x800c835bu + (sint16)code);
            code = *map;
        }
        code = (uint32)(sint16)code;
        if (code == ' ')
        {
            pen_x += div2_trunc(g_font_glyph_width) + div4_trunc(g_font_glyph_width);
            continue;
        }
        if (code == 0x0c)
        {
            set_text_color(&packet, *text++);
            continue;
        }
        if (code == '\n')
        {
            pen_y += g_font_line_height;
            pen_x = (sint16)initial_x;
            continue;
        }
        if (code == '\r')
            continue;
        if (code == 0x10)
        {
            uint32 resource = (uint32)text[0] | (uint32)text[1] << 8 | (uint32)text[2] << 16 | (uint32)text[3] << 24;
            uint8 *frame;
            text += 4;
            submit_font_mode(font);
            frame = animation_frame_resource(resource);
            pen_x += (uint32)frame[0] >> 1;
            render_screen_sprite((sint16)pen_x, (sint16)(pen_y + g_font_glyph_height), resource, 1);
            pen_x += (uint32)frame[0] >> 1;
            continue;
        }
        if (code == 0x11)
        {
            pen_y += *text++;
            continue;
        }
        if (code == 0x13)
        {
            if ((uint16)(pen_y + 0xd5) < 0x1abu)
                pen_x = -div2_trunc(centered_line_width(text));
            continue;
        }
        if ((sint16)code > 0x1f)
        {
            if ((uint16)(pen_y + 0xd5) < 0x1abu)
            {
                sint32 glyph = (sint16)code - 0x21;
                SPRT *copy;
                packet.v0 = (uint8)(font->v0 + (glyph / g_font_glyph_columns) * g_font_glyph_height);
                packet.u0 = (uint8)(font->u0 + (glyph % g_font_glyph_columns) * g_font_glyph_width);
                packet.x0 = (sint16)pen_x;
                packet.y0 = (sint16)pen_y;
                copy = (SPRT *)g_gpu_packet_cursor;
                memcpy(copy, &packet, sizeof(packet));
                psx_set_prim_screen_offset(160, 64);
                AddPrim(g_current_render_frame->ot + 1, copy);
                g_gpu_packet_cursor += sizeof(packet);
            }
            pen_x += g_font_glyph_width;
        }
    }
    submit_font_mode(font);
    g_text_color_index = 0;
    g_text_pen_y = (sint16)pen_y;
}
