#ifndef PLATFORM_WIN_GAME_PLATFORM_H
#define PLATFORM_WIN_GAME_PLATFORM_H

#include "global.h"
#include "level_data.h"
#include "map.h"
#include "model.h"
#include "render.h"
#include "sprite.h"

typedef struct PSXMapTextureInfo
{
    uint16 tpage;
    uint16 clut;
    uint8 u;
    uint8 v;
    uint8 valid;
} PSXMapTextureInfo;

/* BEGIN GENERATED MODULE API */
GDB_CALL
sint32 psx_load_stage_textures(const uint8 *texinfo, uint32 texinfo_size, uint32 script_address);
GDB_CALL sint32 tim_image_upload(void *raw);
GDB_CALL void display_mask_set(sint32 mode);
GDB_CALL void end_frame_submit(sint32 mode);
GDB_CALL void screen_overlay_draw(void);
GDB_CALL void str_video_play(char *str_path, sint32 width, sint32 frames, sint32 mode);
GDB_CALL void vram_clear(void);
sint32 psx_cd_configure_driver(void);
sint32 psx_cd_read_track_table(sint32 mode, void *workspace);
sint32 psx_cd_start_driver(void);
sint32 psx_dump_vram(const char *path);
sint32 psx_finalize_stage_resources(void);
sint32 psx_get_cell_frame_objects(sint32 cell_x, sint32 cell_z, FrameObjectPartial **out);
sint32 psx_get_map_texture_info(sint32 texture, PSXMapTextureInfo *out);
sint32 psx_get_object_shade(void);
void game_runtime_configure(void);
void psx_add_draw_area_rect(void *ot, sint32 x0, sint32 x1, sint32 y0, sint32 y1);
void psx_begin_frame(void);
void psx_cd_stop_driver(void);
void psx_end_frame(void);
void psx_menu_background_end(void);
void psx_ot_trace_model(sint32 bucket, sint32 id, sint32 x, sint32 y, sint32 z);
void psx_set_display_offset_y(sint32 offset);
void psx_set_hierarchy_packet_trace(sint32 active);
void psx_set_model_clut_override(uint16 clut);
void psx_set_model_packet_context(sint32 id, sint32 x, sint32 y, sint32 z, sint32 ry, sint32 rx, sint32 rz);
void psx_set_object_shade(sint32 shade);
void psx_set_prim_depth(sint32 world_z);
void psx_set_prim_ot_bucket(sint32 bucket);
void psx_set_prim_screen_offset(sint32 x, sint32 y);
void psx_smoke_mission_start(void);
void psx_submit_prepared_model_ft3(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, const uint8 *source, sint32 shade);
void psx_submit_prepared_model_ft4(sint32 x0, sint32 y0, sint32 x1, sint32 y1, sint32 x2, sint32 y2, sint32 x3, sint32 y3, const uint8 *source, sint32 shade);
void frame_model_submit(FrameObjectPartial *object);
void render_map_scene(void);
/* END GENERATED MODULE API */

#endif
