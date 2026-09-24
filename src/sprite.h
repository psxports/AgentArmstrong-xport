#ifndef MODULE_API_SPRITE_H
#define MODULE_API_SPRITE_H

#include <stddef.h>

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "model.h"

/* Types. */
/* Shared 0x100-byte sprite/projectile object created by FUN_80090758.
 * Semantic names are limited to fields whose role is established by the
 * constructor/update or by multiple direct consumers. */
typedef struct SPRITE
{
    COLLISION collision;                      /* +0x00 */
    sint16 damage;                            /* +0x78 */
    sint16 flash_clut_ticks;                  /* +0x7A */
    sint32 velocity_x;                        /* +0x7C */
    sint32 velocity_y;                        /* +0x80 */
    sint32 velocity_z;                        /* +0x84 */
    sint32 acceleration_x;                    /* +0x88 */
    sint32 acceleration_y;                    /* +0x8C */
    sint32 acceleration_z;                    /* +0x90 */
    sint16 lifetime;                          /* +0x94 */
    uint16 scale_x;                           /* +0x96 */
    uint16 scale_y;                           /* +0x98 */
    sint16 scale_step;                        /* +0x9A */
    sint16 field_9c;                          /* +0x9C */
    sint16 field_9e;                          /* +0x9E */
    sint16 render_kind;                       /* +0xA0 */
    sint16 owns_resource;                     /* +0xA2 */
    sint32 cull_distance;                     /* +0xA4 */
    sint16 field_a8;                          /* +0xA8 */
    sint16 field_aa;                          /* +0xAA */
    sint16 field_ac;                          /* +0xAC */
    uint8 field_ae;                           /* +0xAE */
    uint8 field_af;                           /* +0xAF */
    sint16 rotation_x;                        /* +0xB0 */
    sint16 rotation_step_x;                   /* +0xB2 */
    sint16 rotation_y;                        /* +0xB4 */
    sint16 rotation_step_y;                   /* +0xB6 */
    sint16 rotation_z;                        /* +0xB8 */
    sint16 rotation_step_z;                   /* +0xBA */
    FUNC_COLLISION_UPDATE frame_callback;     /* +0xBC */
    FUNC_COLLISION_UPDATE collision_callback; /* +0xC0 */
    FUNC_COLLISION_UPDATE impact_callback;    /* +0xC4 */
    sint32 field_c8;                          /* +0xC8 */
    sint32 field_cc;                          /* +0xCC */
    sint32 field_d0;                          /* +0xD0 */
    sint32 field_d4;                          /* +0xD4 */
    ANIM animation;                           /* +0xD8 */
    sint32 field_f4;                          /* +0xF4 */
    MODEL_NODE *model;                        /* +0xF8 */
    uint16 clut_override;                     /* +0xFC */
    uint16 field_fe;                          /* +0xFE */
} SPRITE;

#if defined(AP_32BIT)
    #define SPRITE_OBJECT_OFFSET_ASSERT(field, offset) typedef char RuntimeSpriteObject_##field##_at_##offset[(offsetof(SPRITE, field) == 0x##offset) ? 1 : -1]
SPRITE_OBJECT_OFFSET_ASSERT(collision, 00);
SPRITE_OBJECT_OFFSET_ASSERT(damage, 78);
SPRITE_OBJECT_OFFSET_ASSERT(velocity_x, 7c);
SPRITE_OBJECT_OFFSET_ASSERT(acceleration_x, 88);
SPRITE_OBJECT_OFFSET_ASSERT(lifetime, 94);
SPRITE_OBJECT_OFFSET_ASSERT(scale_x, 96);
SPRITE_OBJECT_OFFSET_ASSERT(scale_y, 98);
SPRITE_OBJECT_OFFSET_ASSERT(scale_step, 9a);
SPRITE_OBJECT_OFFSET_ASSERT(render_kind, a0);
SPRITE_OBJECT_OFFSET_ASSERT(owns_resource, a2);
SPRITE_OBJECT_OFFSET_ASSERT(cull_distance, a4);
SPRITE_OBJECT_OFFSET_ASSERT(rotation_x, b0);
SPRITE_OBJECT_OFFSET_ASSERT(frame_callback, bc);
SPRITE_OBJECT_OFFSET_ASSERT(collision_callback, c0);
SPRITE_OBJECT_OFFSET_ASSERT(impact_callback, c4);
SPRITE_OBJECT_OFFSET_ASSERT(field_c8, c8);
SPRITE_OBJECT_OFFSET_ASSERT(animation, d8);
SPRITE_OBJECT_OFFSET_ASSERT(model, f8);
SPRITE_OBJECT_OFFSET_ASSERT(clut_override, fc);
typedef char RuntimeSpriteObject_size_100[(sizeof(SPRITE) == 0x100) ? 1 : -1];
    #undef SPRITE_OBJECT_OFFSET_ASSERT
#endif

/* Types. */
typedef struct
{
    uint8 field_00[0x10];   /* +0x00 */
    uint32 region_token;    /* +0x10 */
    uint16 u0;              /* +0x14 */
    uint16 u1;              /* +0x16 */
    uint16 v0;              /* +0x18 */
    uint16 v1;              /* +0x1A */
    uint16 tpage;           /* +0x1C */
    uint16 page_x;          /* +0x1E */
    uint16 page_y;          /* +0x20 */
    uint16 pixel_mode;      /* +0x22 */
    uint16 clut;            /* +0x24 */
    uint16 field_26;        /* +0x26 */
    uint32 cached_resource; /* +0x28 */
    sint8 pivot_x;          /* +0x2C */
    sint8 pivot_y;          /* +0x2D */
    uint8 width_minus_one;  /* +0x2E */
    uint8 height;           /* +0x2F */
} VRAM_SPRITE;

typedef char SpriteVramDescriptor_size_30[sizeof(VRAM_SPRITE) == 0x30 ? 1 : -1];
typedef char SpriteVramDescriptor_region_at_10[offsetof(VRAM_SPRITE, region_token) == 0x10 ? 1 : -1];
typedef char SpriteVramDescriptor_uv_at_14[offsetof(VRAM_SPRITE, u0) == 0x14 ? 1 : -1];
typedef char SpriteVramDescriptor_tpage_at_1c[offsetof(VRAM_SPRITE, tpage) == 0x1c ? 1 : -1];
typedef char SpriteVramDescriptor_mode_at_22[offsetof(VRAM_SPRITE, pixel_mode) == 0x22 ? 1 : -1];
typedef char SpriteVramDescriptor_cached_at_28[offsetof(VRAM_SPRITE, cached_resource) == 0x28 ? 1 : -1];
typedef char SpriteVramDescriptor_dimensions_at_2e[offsetof(VRAM_SPRITE, width_minus_one) == 0x2e ? 1 : -1];

/* BEGIN GENERATED MODULE API */
GDB_CALL SPRT *render_screen_sprite(sint16 x, sint16 y, uint32 resource, sint16 ot_bucket);
GDB_CALL uint32 sprite_clut_upload(uint32 resource_id, sint32 unused_2, sint32 unused_3, sint32 semi_transparent);
GDB_CALL void *sprite_create_vram_descriptors(uint32 resource_id, sint32 frame_count, uint16 clut, sint16 abr);
GDB_CALL void font_sprite_initialize(void);
GDB_CALL void render_world_sprite(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *prim, sint16 ot_bucket, sint32 split_mode, VRAM_SPRITE *descriptor, sint32 rotation);
sint32 sprite_resource_dump_audit(const char *path);
sint32 frame_render_work_enabled(void);
void *sprite_vram_descriptor_for_resource(uint32 resource_id);
void sprite_resource_reset(void);
void render_stage_background(void);
void render_world_sprite_immediate(uint32 resource, sint32 x, sint32 y, sint32 z, sint32 flip_or_velocity, sint32 scale_x, sint32 scale_y, POLY_FT4 *prim, sint32 ot_bucket, sint32 unused_10, sint32 raw_descriptor, sint32 rotation);
void sprite_assign_clut_range(uint32 resource_id, sint32 frame_count, uint16 clut);
void sprite_upload_pixels(void *raw_descriptor, uint8 *pixels, sint32 mode);
void sprite_archive_resource_load(const char *name, uint32 resource, sint32 frames, sint32 mode, void *archive);
uint16 sprite_clut_entry_count(uint32 resource_id);
void sprite_vram_defer_release(void *resource);
void sprite_vram_release_deferred(void);
void sprite_vram_reset_deferred(void);
GDB_CALL void sprite_vram_reset(uint16 *env);
GDB_CALL VRAM_SPRITE *sprite_vram_allocate(VRAM_SPRITE *d, sint32 w, sint32 h, sint32 mode);
/* END GENERATED MODULE API */

#endif
