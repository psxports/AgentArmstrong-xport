#ifndef GAME_H
#define GAME_H

#include "xport.h"

extern sint32 g_cd_file_io_enabled;

/* Resident stage and attract-mode publications retain original widths */
typedef struct GAME_SESSION
{
    sint32 stage, next_stage, language, play_fmv;
    sint32 scuba_active;
    sint16 attract_mode, next_attract_mode, attract_demo_variant, attract_demo_idx;
} GAME_SESSION;

extern GAME_SESSION g_session;
sint32 frame_render_work_enabled(void);

GDB_CALL sint32 boot_language_text_load(void);

typedef struct
{
    sint32 stage;
    uint32 render_start_frame;
} ATTRACT_DEMO;

enum
{
    ATTRACT_DEMO_COUNT = 7
};

extern const ATTRACT_DEMO g_attract_demos[ATTRACT_DEMO_COUNT];

#endif
