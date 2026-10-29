#ifndef GAME_RUNTIME_H
#define GAME_RUNTIME_H

#include "spatial.h"
#include "level_data.h"
#include "map.h"
#include "model.h"
#include "render.h"
#include "sprite.h"

/* BEGIN GENERATED MODULE API */
GDB_CALL sint32 tim_image_upload(const void *raw, uint32 size);
GDB_CALL void scr_overlay_draw(void);
GDB_CALL void str_video_play(const char *str_path, sint32 height, sint32 frames, sint32 mode);
GDB_CALL void vram_clear(void);
void psx_add_draw_area_rect(void *ot, sint32 x0, sint32 x1, sint32 y0, sint32 y1);
void psx_begin_frame(void);
void psx_end_frame(void);
void psx_set_disp_offset_y(sint32 offset);
void psx_set_prim_depth(sint32 world_z);
void psx_set_prim_ot_bucket(sint32 bucket);
void psx_submit_prepared_model_ft3(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, const MODEL_PACKET *src, sint32 shade, uint16 clut_override, RENDER_FRAME *target_frame);
void psx_submit_prepared_model_ft4(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, const MODEL_PACKET *src, sint32 shade, uint16 clut_override, RENDER_FRAME *target_frame);
void frame_model_submit(FRAME_OBJECT *object);
/* END GENERATED MODULE API */

/* Native stage-heap lifetime boundary */
void game_runtime_heap_reset(void);

#endif
