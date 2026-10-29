#include "random.h"

/* Variables. */
/* Original: SLES_004.74:DAT_800D4188; primary RNG word (gp+0x500). */
static uint32 primary_state;

/* Original: SLES_004.74:DAT_800D41E8; independent RNG word (gp+0x560).
 * FUN_80088FA8 temporarily swaps it through the primary word, then restores it. */
static uint32 secondary_state;

/* Functions. */
/* Original: SLES_004.74:FUN_80088F4C. */
GDB_CALL uint16 random_range(sint32 maximum)
{
    uint32 mixed;

    if (maximum == 0)
    {
        primary_state = 0;
        return 0;
    }
    mixed = primary_state * 13 + 7;
    primary_state = mixed;
    mixed ^= mixed >> 16;
    return ((maximum + 1u) * (mixed & 0xffff)) >> 16;
}

/* SLES-004.74 0x80088FA8..0x80088FE4. */
/* Original: SLES_004.74:FUN_80088FA8. */
uint16 secondary_random_range(sint32 maximum)
{
    uint32 secondary = secondary_state;
    uint32 saved_primary = primary_state;
    uint16 result;
    primary_state = secondary;
    result = random_range(maximum);
    secondary = primary_state;
    primary_state = saved_primary;
    secondary_state = secondary;
    return result;
}
