#include <string.h>

#include "video.h"

sint32 video_play_str(const char *path, sint32 expected_height, sint32 last_frame)
{
    char data_path[260];
    const char *src;
    char *dst = data_path;
    uint32 remaining = sizeof(data_path);
    XportFmvPresentation presentation = {0};
    if (!path || !*path)
        return -1;
    if (expected_height <= 0 || expected_height > 240)
        return -1;
    src = path;
    if (strncmp(path, "DATA\\", 5) && strncmp(path, "DATA/", 5))
    {
        memcpy(dst, "DATA/", 5);
        dst += 5;
        remaining -= 5;
    }
    while (*src && *src != ';' && remaining > 1)
    {
        *dst++ = *src == '\\' ? '/' : *src;
        ++src;
        --remaining;
    }
    *dst = 0;
    if (*src && *src != ';')
        return -1;
    presentation.width = 320;
    presentation.height = 240;
    presentation.video_y = (240 - expected_height) / 2;
    presentation.controllers = 1;
    presentation.skip_buttons = PADstart | PADRdown;
    return xport_fmv_play_presented(data_path, last_frame > 0 ? last_frame : 0, 0, &presentation);
}
