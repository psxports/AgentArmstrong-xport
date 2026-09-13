#ifndef MODULE_API_TEXT_RENDERER_H
#define MODULE_API_TEXT_RENDERER_H

#include "app.h"

/* Types. */
typedef struct
{
    uint8 unknown_00[0x14];
    uint16 u0;
    uint16 u1;
    uint16 v0;
    uint16 v1;
    uint16 tpage;
} FONT;

/* Original: DAT_800D4178. */

/* BEGIN GENERATED MODULE API */
GDB_CALL void text_render(const char *source, sint32 initial_x, sint32 initial_y);
sint32 text_byte_length(const char *text);
/* END GENERATED MODULE API */

#endif
