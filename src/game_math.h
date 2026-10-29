#ifndef GAME_MATH_H
#define GAME_MATH_H

#include "xport.h"

#define div_trunc(val, divisor) ((sint32)(val) / (sint32)(divisor))
/* Low-word multiplication retains modulo32 before signed division */
#define mul_lo(left, right) ((sint32)((uint32)(left) * (uint32)(right)))
#define mips_mult_lo_div(left, right, divisor) div_trunc(mul_lo((left), (right)), (divisor))
#define div_trunc_16(val) div_trunc((val), 0x10)
#define div_trunc_256(val) div_trunc((val), 0x100)
#define div_trunc_4096(val) div_trunc((val), 0x1000)
#define div_pow2_trunc(val, shift) mips_div_pow2_trunc((val), (shift))
#define div_64_trunc(val) div_pow2_trunc((val), 6)
#define div_256_trunc(val) div_pow2_trunc((val), 8)
#define div_1024_trunc(val) div_pow2_trunc((val), 10)
#define div_2048_trunc(val) div_pow2_trunc((val), 11)
#define div_4096_trunc(val) div_pow2_trunc((val), 12)
#define div_16384_trunc(val) div_pow2_trunc((val), 14)

sint32 mips_div_pow2_trunc(sint32 val, uint32 shift);
sint32 abs_s32(sint32 val);
sint16 angle_approach_wrapped(sint16 target, sint32 step, sint16 cur);

/* Shared immutable angle entries also supply original sprite overflow reads */
extern const sint32 g_vector_angles[64][64];

sint32 fixed_angle_from_vector(sint32 x, sint32 z);

/* Native coordinates retain the fixed-point units of their owning operation */
typedef struct POINT_2D
{
    sint32 x, y;
} POINT_2D;

/* Native DDA positions and increments retain the original fixed-point units */
typedef struct FIXED_DDA_LINE
{
    struct
    {
        sint32 x, y, z;
    } pos, step;
} FIXED_DDA_LINE;

sint32 fixed_dda_initialize(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, FIXED_DDA_LINE *line);
void fixed_dda_advance(FIXED_DDA_LINE *line);

#endif
