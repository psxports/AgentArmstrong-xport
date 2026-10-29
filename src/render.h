#ifndef MODULE_API_RENDER_H
#define MODULE_API_RENDER_H

#include <stddef.h>

#include "xport.h"
#include "psx.h"

/* Screen geometry and counters retain their original publication/reset sites */
typedef struct RENDER_STATE
{
    sint32 width, height, half_width;
    sint16 disp_y;
    uint32 frame;
    uint32 vsyncs_this_frame, vsyncs_total;
} RENDER_STATE;

extern RENDER_STATE g_render_state;
void render_vblank_update(void);
GDB_CALL void end_frame_submit(sint32 mode);

/* Types. */
enum
{
    TRANSIENT_PRIM_LIMIT = 64,
    RENDER_OT_LENGTH = 0x481,
    RENDER_PRIM_CAPACITY = 0x55f0
};

/* Variable-format GPU packet bytes use shared fixed-width packet layouts */
typedef struct
{
    union
    {
        uint32 alignment;
        uint8 bytes[RENDER_PRIM_CAPACITY];
    } data;

    uint32 used;
} RENDER_PACKET_ARENA;

typedef struct
{
    uint32 ot[RENDER_OT_LENGTH];
    DRAWENV drawenv;
    DISPENV disp_env;
    RENDER_PACKET_ARENA packets;
} RENDER_FRAME;

typedef char RENDER_DRAW_ENV_SIZE_5C[sizeof(DRAWENV) == 0x5c ? 1 : -1];
typedef char RENDER_DRAW_ENV_COMMANDS_AT_1C[offsetof(DRAWENV, dr_env) == 0x1c ? 1 : -1];
typedef char RENDER_DISP_ENV_SIZE_14[sizeof(DISPENV) == 0x14 ? 1 : -1];
typedef char RENDER_DRAW_COMMAND_SIZE_40[sizeof(DR_ENV) == 0x40 ? 1 : -1];
typedef char RENDER_RECT_SIZE_08[sizeof(PSX_RECT) == 8 ? 1 : -1];

extern __declspec(dllexport) RENDER_FRAME g_render_frames[2];
extern RENDER_FRAME *g_cur_render_frame;
extern RENDER_PACKET_ARENA *g_gpu_packets;
extern uint16 g_render_frame_idx;

extern POLY_FT4 g_transient_prims[TRANSIENT_PRIM_LIMIT];
extern POLY_FT4 *g_next_transient_prim;

extern sint16 g_disp_shake_ticks;
void disp_shake_start(void);

/* BEGIN GENERATED MODULE API */
GDB_CALL void fatal_error(const char *message);
void fatal_error_with_val(const char *prefix, sint32 val);
GDB_CALL void disp_buffers_initialize(void);
GDB_CALL void frame_begin(void);
GDB_CALL void ot_submit(uint32 *ot);
sint32 ot_depth_inherit_row_bucket(sint32 stored_bucket, sint32 row_bucket);
sint32 ot_depth_row_bucket(sint32 visible_row);
sint32 ot_depth_run_self_tests(void);
sint32 ot_depth_should_swap_cell_objects(sint32 cur_world_z, sint32 next_world_z);
sint32 pause_menu_update(void);
void cd_audio_pause(void);
void cd_audio_play(void);
void debug_input_update(void);
void disp_shake_update(void);
/* END GENERATED MODULE API */

GDB_CALL void vertical_sync_wait(sint32 mode);

#endif
