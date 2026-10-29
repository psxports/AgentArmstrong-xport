#ifndef SPRITE_RENDERER_H
#define SPRITE_RENDERER_H

#include "xport.h"
#include "psx.h"
#include "sprite.h"

#if defined(AP_32BIT)
typedef char SPRITE_RENDERER_POLY_FT4_SIZE_28[sizeof(POLY_FT4) == 0x28 ? 1 : -1];
typedef char SPRITE_RENDERER_POLY_FT4_CLUT_0E[(offsetof(POLY_FT4, p) + offsetof(POINT_T, attr)) == 0x0e ? 1 : -1];
typedef char SPRITE_RENDERER_POLY_FT4_TPAGE_16[(offsetof(POLY_FT4, p) + sizeof(POINT_T) + offsetof(POINT_T, attr)) == 0x16 ? 1 : -1];
typedef char SPRITE_RENDERER_POLY_FT4_X3_20[(offsetof(POLY_FT4, p) + 3 * sizeof(POINT_T) + offsetof(POINT_T, xy)) == 0x20 ? 1 : -1];
#endif

#endif
