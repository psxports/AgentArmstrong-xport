#include <stddef.h>
#include "app.h"
#include "global.h"
#include "stubs.h"

/* Functions. */
void input_record_append(sint32 input)
{
    sint32 *cursor = (sint32 *)g_input_record_cursor;
    if ((uint8 *)g_input_record_buffer + 0x1000 < (uint8 *)g_input_record_cursor)
        fatal_error("RECORD OVERFLOW");

    if (input == g_input_run_value)
    {
        g_input_run_length++;
        return;
    }
    if (g_input_run_length == -1)
    {
        g_input_run_value = input;
        g_input_run_length = 0;
        return;
    }
    if (g_input_run_length < 4)
    {
        while (g_input_run_length >= 0)
        {
            *cursor++ = g_input_run_value;
            g_input_record_cursor = cursor;
            g_input_run_length--;
        }
    }
    else
    {
        *cursor++ = 0xffff;
        *cursor++ = g_input_run_length;
        *cursor++ = g_input_run_value;
        g_input_record_cursor = cursor;
    }
    g_input_run_length = 0;
    g_input_run_value = input;
}

/* Original: FUN_800960FC. */
sint32 input_playback_next(void)
{
    sint32 result = g_input_run_value;
    sint32 *cursor = (sint32 *)g_input_record_cursor;
    sint32 remaining = g_input_run_length - 1;
    if (g_input_run_length == -1)
    {
        result = *cursor++;
        if (result == 0xffff)
        {
            remaining = cursor[0] - 1;
            g_input_run_value = cursor[1];
            result = g_input_run_value;
            cursor += 2;
        }
    }
    g_input_run_length = remaining;
    g_input_record_cursor = cursor;
    return result;
}
