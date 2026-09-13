#include "app.h"

/* Variables. */
/* Original: SLES_004.74:DAT_800D4188; primary RNG word (gp+0x500). */
static uint32 g_random_state;

/* Original: SLES_004.74:DAT_800D41E8; independent RNG word (gp+0x560).
 * FUN_80088FA8 temporarily swaps it through the primary word, then restores it. */
static uint32 g_secondary_random_state;

/* Functions. */
/* Original: FUN_80088F4C. */
GDB_CALL sint32 random_range(sint32 maximum)
{
    uint32 mixed;

    if (maximum == 0)
    {
        g_random_state = 0;
        return 0;
    }
    mixed = g_random_state * 13 + 7;
    g_random_state = mixed;
    mixed ^= mixed >> 16;
    return (sint32)(((uint32)(maximum + 1) * (mixed & 0xffff)) >> 16);
}

/* SLES-004.74 0x80088FA8..0x80088FE4. */
/* Original: FUN_80088FA8. */
sint32 secondary_random_range(sint32 maximum)
{
    uint32 primary_state = g_random_state;
    sint32 result;
    g_random_state = g_secondary_random_state;
    result = random_range(maximum);
    g_secondary_random_state = g_random_state;
    g_random_state = primary_state;
    return result;
}
