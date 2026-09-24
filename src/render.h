#ifndef MODULE_API_RENDER_H
#define MODULE_API_RENDER_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

/* Types. */
enum
{
    RENDER_OT_LENGTH = 0x481,
    RENDER_PRIM_AREA_OFFSET = 0x1274,
    RENDER_FRAME_USED_SIZE = 0x6864
};

typedef struct
{
    uint32 ot[RENDER_OT_LENGTH];                                       /* +0x0000..+0x1203 */
    DRAWENV drawenv;                                                   /* +0x1204 */
    DISPENV display_env;                                               /* +0x1260 */
    uint8 prim_area[RENDER_FRAME_USED_SIZE - RENDER_PRIM_AREA_OFFSET]; /* +0x1274 */
} RENDER_FRAME;

typedef struct
{
    uint32 magic;            /* +0x00 */
    uint32 flags;            /* +0x04 */
    uint32 image_block_size; /* +0x08 */
    PSX_RECT rectangle;      /* +0x0C */
    uint8 pixels[1];         /* +0x14 */
} SCREENSHOT_TIM_IMAGE;

#if defined(AP_32BIT)
typedef char RenderDrawEnv_size_5c[sizeof(DRAWENV) == 0x5c ? 1 : -1];
typedef char RenderDrawEnv_commands_at_1c[offsetof(DRAWENV, dr_env) == 0x1c ? 1 : -1];
typedef char RenderDispEnv_size_14[sizeof(DISPENV) == 0x14 ? 1 : -1];
typedef char RenderDrawCommand_size_40[sizeof(DR_ENV) == 0x40 ? 1 : -1];
typedef char RenderRect_size_08[sizeof(PSX_RECT) == 8 ? 1 : -1];
typedef char RenderFrame_draw_at_1204[offsetof(RENDER_FRAME, drawenv) == 0x1204 ? 1 : -1];
typedef char RenderFrame_display_at_1260[offsetof(RENDER_FRAME, display_env) == 0x1260 ? 1 : -1];
typedef char RenderFrame_prim_at_1274[offsetof(RENDER_FRAME, prim_area) == 0x1274 ? 1 : -1];
typedef char RenderFrame_used_size_6864[sizeof(RENDER_FRAME) == 0x6864 ? 1 : -1];
typedef char ScreenshotTim_rectangle_at_0c[offsetof(SCREENSHOT_TIM_IMAGE, rectangle) == 0x0c ? 1 : -1];
typedef char ScreenshotTim_pixels_at_14[offsetof(SCREENSHOT_TIM_IMAGE, pixels) == 0x14 ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
GDB_CALL void display_buffers_initialize(void);
GDB_CALL void drawenv_apply(void *env);
GDB_CALL void frame_begin(void);
GDB_CALL void gpu_draw_sync(sint32 mode);
GDB_CALL void ot_submit(uint32 *ot);
sint32 ot_depth_inherit_row_bucket(sint32 stored_bucket, sint32 row_bucket);
sint32 ot_depth_row_bucket(sint32 visible_row);
sint32 ot_depth_run_self_tests(void);
sint32 ot_depth_should_swap_cell_objects(sint32 current_world_z, sint32 next_world_z);
sint32 pause_menu_update(void);
void cd_audio_pause(void);
void cd_audio_play(void);
void debug_input_update(void);
void display_shake_update(void);
void sound_all_voices_stop(void);
/* END GENERATED MODULE API */

#endif
