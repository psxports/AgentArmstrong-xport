#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include "app.h"
#include "sprite.h"

#if defined(AP_32BIT)
typedef char POLY_FT4_size_28[sizeof(POLY_FT4) == 0x28 ? 1 : -1];
typedef char POLY_FT4_clut_0e[offsetof(POLY_FT4, clut) == 0x0e ? 1 : -1];
typedef char POLY_FT4_tpage_16[offsetof(POLY_FT4, tpage) == 0x16 ? 1 : -1];
typedef char POLY_FT4_x3_20[offsetof(POLY_FT4, x3) == 0x20 ? 1 : -1];
#endif

#endif
