#include <stdlib.h>

#include "render.h"
#include "input_record.h"

INPUT_RECORD_STATE g_input_rec;
/* Original: SLES_004.74:DAT_800D3CE4 */
const char g_input_playback_label[5] = "DEMO";

/* The current stage owns the stream allocation */
void input_rec_heap_reset(void)
{
    g_input_rec.data = 0;
    g_input_rec.size = g_input_rec.cursor = 0;
}

/* Original: FUN_80095FE4. */
void input_rec_append(sint32 input)
{
    uint32 limit = g_input_rec.size < 0x1000 ? g_input_rec.size : 0x1000;
    uint32 words;
    XASSERT(!(g_input_rec.data == 0 || g_input_rec.cursor > limit));
    if (input == g_input_rec.run_val)
    {
        g_input_rec.run_len = (g_input_rec.run_len + 1u);
        return;
    }
    if (g_input_rec.run_len == -1)
    {
        g_input_rec.run_val = input;
        g_input_rec.run_len = 0;
        return;
    }
    if (g_input_rec.run_len < 4)
        words = g_input_rec.run_len >= 0 ? g_input_rec.run_len + 1u : 0;
    else
        words = 3;
    XASSERT(words <= (limit - g_input_rec.cursor) / 4);
    if (g_input_rec.run_len < 4)
    {
        while (g_input_rec.run_len >= 0)
        {
            xport_store_le32(g_input_rec.data + g_input_rec.cursor, g_input_rec.run_val);
            g_input_rec.cursor += 4;
            --g_input_rec.run_len;
        }
    }
    else
    {
        xport_store_le32(g_input_rec.data + g_input_rec.cursor, 0xffff);
        xport_store_le32(g_input_rec.data + g_input_rec.cursor + 4, g_input_rec.run_len);
        xport_store_le32(g_input_rec.data + g_input_rec.cursor + 8, g_input_rec.run_val);
        g_input_rec.cursor += 12;
    }
    g_input_rec.run_len = 0;
    g_input_rec.run_val = input;
}

/* Original: FUN_800960FC. */
sint32 input_playback_next(void)
{
    sint32 result;
    if (g_input_rec.run_len == -1)
    {
        XASSERT(!(g_input_rec.data == 0 || g_input_rec.cursor > g_input_rec.size || g_input_rec.size - g_input_rec.cursor < 4));
        result = xport_load_le32(g_input_rec.data + g_input_rec.cursor);
        g_input_rec.cursor += 4;
        if (result == 0xffff)
        {
            XASSERT(g_input_rec.size - g_input_rec.cursor >= 8);
            g_input_rec.run_len = (xport_load_le32(g_input_rec.data + g_input_rec.cursor) - 1);
            g_input_rec.run_val = xport_load_le32(g_input_rec.data + g_input_rec.cursor + 4);
            g_input_rec.cursor += 8;
            result = g_input_rec.run_val;
        }
        /* Ordinary words leave the original -1 run sentinel unchanged */
        return result;
    }
    g_input_rec.run_len = (g_input_rec.run_len - 1u);
    return g_input_rec.run_val;
}
