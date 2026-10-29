#ifndef MODULE_API_SPRITE_H
#define MODULE_API_SPRITE_H

#include <stddef.h>

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "model.h"

extern sint32 g_vram_error_reporting_enabled;

enum
{
    SPRITE_CLUT_SLOT_COUNT = 256,
    CLUT_LANGUAGE_FLAG = 0x1b,
    CLUT_TANYA = 0x5f,
    CLUT_SIGNAL_FLARE = 0xa1,
    CLUT_HQ_MAP = 0xab,
    CLUT_SCUBA_DIVER = 0xb5
};

typedef struct SPRITE_PALETTE_STATE
{
    uint16 cluts[SPRITE_CLUT_SLOT_COUNT];
    sint16 cursor_x, cursor_y;
    sint16 hit_flash;
} SPRITE_PALETTE_STATE;

extern SPRITE_PALETTE_STATE g_spr_pal;
extern const sint32 g_default_spr_scale;

/* Types. */
/* Sprite/projectile runtime owner created by original FUN_80090758
 * Semantic names are limited to fields whose role is established by the
 * constructor/update or by multiple direct consumers. */
typedef enum
{
    SPRITE_CONTACT_NOTIFY,
    SPRITE_CONTACT_OBJECT
} SPRITE_CONTACT_KIND;

typedef struct SPRITE
{
    COLLISION col;
    sint32 velocity_x;
    sint32 velocity_y;
    sint32 velocity_z;
    sint32 acceleration_x;
    sint32 acceleration_y;
    sint32 acceleration_z;
    sint16 lifetime;
    uint16 scale_x;
    uint16 scale_y;
    sint16 scale_step;
    sint16 retain_offscreen;
    sint16 split_mode;
    sint16 render_kind;
    sint16 owns_resource;
    sint32 cull_distance;
    sint16 light_delta;
    sint16 light_step;
    sint16 ot_bucket;
    uint8 force_light;
    sint16 rotation_x;
    sint16 rotation_step_x;
    sint16 rotation_y;
    sint16 rotation_step_y;
    sint16 rotation_z;
    sint16 rotation_step_z;
    FUNC_OBJECT_UPDATE frame_callback;
    SPRITE_CONTACT_KIND col_callback_kind;

    union
    {
        FUNC_OBJECT_UPDATE col_callback;
        FUNC_OBJECT_RESULT_UPDATE col_callback_object;
    };

    FUNC_OBJECT_UPDATE impact_callback;

    /* The installed behavior callback owns its variant */
    union
    {
        struct
        {
            sint32 special;
            sint32 target_x, target_y, target_z;
        } projectile;

        sint32 pickup_y;
        sint32 parachute_step;
        sint32 water_light_step;

        struct
        {
            sint32 next_spawned;
            sint32 ticks;
            sint16 sound_handle;
        } explosion;
    } behavior;

    ANIM animation;
    MODEL_NODE *model;
    uint16 clut_override;
} SPRITE;

/* Types. */
typedef struct VRAM_REGION VRAM_REGION;

typedef struct VRAM_SPRITE
{
    struct VRAM_SPRITE *prev;
    struct VRAM_SPRITE *next;
    VRAM_REGION *region;
    uint16 u0, u1, v0, v1;
    uint16 tpage;
    uint16 page_x, page_y;
    uint16 pixel_mode;
    uint16 clut;
    uint32 cached_resource;
    sint8 pivot_x, pivot_y;
    uint8 width_minus_one, height;
} VRAM_SPRITE;

typedef struct
{
    uint16 x, y, width, height, mode;
} VRAM_REGION_CFG;

typedef struct
{
    const VRAM_REGION_CFG *regions;
    uint32 count;
} VRAM_LAYOUT;

extern const VRAM_LAYOUT g_game_vram_layout;
extern const VRAM_LAYOUT g_hq_vram_layout;

extern const ANIM_COMMAND g_particle_animation[14];

/* BEGIN GENERATED MODULE API */
GDB_CALL SPRT *render_scr_spr(sint16 x, sint16 y, uint32 resource, sint16 ot_bucket);
GDB_CALL uint32 spr_clut_upload(uint32 resource_id, sint32 semi_transparent);
GDB_CALL VRAM_SPRITE *spr_create_vram_descs(uint32 resource_id, sint32 frame_count, uint16 clut, sint16 abr);
GDB_CALL void render_world_spr(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *poly0, sint16 ot_bucket, sint32 split_mode, VRAM_SPRITE *desc, sint32 rotation);
VRAM_SPRITE *spr_vram_desc_for_resource(uint32 resource_id);
void render_stage_background(void);
void render_world_spr_immediate(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *poly0, sint32 ot_bucket, VRAM_SPRITE *desc, sint32 rotation);
void spr_assign_clut_range(uint32 resource_id, sint16 frame_count, uint16 clut);
void spr_upload_pix(VRAM_SPRITE *desc, uint32 resource_id, sint32 mode);
void spr_archive_resource_load(const char *name, uint32 resource, sint32 frames, sint32 mode, const void *archive, sint32 archive_size);
void spr_vram_defer_release(VRAM_SPRITE *resource);
void spr_vram_release_deferred(void);
void spr_vram_reset_deferred(void);
GDB_CALL void spr_vram_reset(const VRAM_LAYOUT *layout);
GDB_CALL sint32 spr_vram_allocate(VRAM_SPRITE *d, uint16 w, uint16 h, sint32 mode);
/* END GENERATED MODULE API */

#endif
