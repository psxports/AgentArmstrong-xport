#ifndef MODULE_API_TEXT_RENDERER_H
#define MODULE_API_TEXT_RENDERER_H

#include "xport.h"
#include "psx.h"
#include "sprite.h"

enum
{
    GAME_TEXT_COUNT = 142,
    GAME_TEXT_MENU_FIRST = 105
};

typedef struct
{
    uint8 *data;
    uint32 size;
    const char *unavailable_stage;
    const char *entries[GAME_TEXT_COUNT];
} GAME_TEXT;

extern GAME_TEXT g_game_text;

sint32 game_text_load(const void *data, uint32 size, sint32 language);

typedef struct TEXT_RENDER_STATE
{
    /* Originals: DAT_800D4178, DAT_800D4060, DAT_800D40D4, DAT_800D3F20 */
    const VRAM_SPRITE *font;
    sint32 glyph_width, glyph_height, columns;
    /* Originals: DAT_800D3ED0, DAT_800D3FE4, DAT_800D3CF0, DAT_800D3F6C */
    sint16 line_height, clut, color_idx;
    sint16 pen_y;
} TEXT_RENDER_STATE;

extern TEXT_RENDER_STATE g_text_renderer;

/* Native descriptor/resource lifetime boundary */
void text_renderer_resource_reset(void);
void text_renderer_release(void);
GDB_CALL void text_font_init(void);

/* BEGIN GENERATED MODULE API */
GDB_CALL char *integer_to_text(sint32 val);
GDB_CALL void text_render(const char *src, sint16 initial_x, sint16 initial_y);
sint32 text_byte_len(const char *text);
/* END GENERATED MODULE API */

#endif
