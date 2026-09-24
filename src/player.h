#ifndef MODULE_API_PLAYER_H
#define MODULE_API_PLAYER_H

#include <stddef.h>

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "cc_archive.h"
#include "collision.h"
#include "effect_update.h"
#include "sprite.h"

/* Types. */
/* Generic mission item created by FUN_8009F274.  The allocation size and
 * every named field below are pinned by SLES_004.74 0x8009F0B0..0x8009F870.
 * tail_state is deliberately neutral: different item callbacks consume the
 * same slot as either an age counter or an address. */
typedef struct
{
    COLLISION collision;                       /* +0x00 */
    sint16 damage;                             /* +0x78 */
    uint16 field_7a;                           /* +0x7A */
    uint8 field_7c[0x30];                      /* +0x7C */
    ANIM animation;                            /* +0xAC */
    sint32 item_type;                          /* +0xC8 */
    sint32 velocity_y;                         /* +0xCC */
    sint32 velocity_x;                         /* +0xD0 */
    sint32 velocity_z;                         /* +0xD4 */
    sint32 gravity_y;                          /* +0xD8 */
    sint32 field_dc;                           /* +0xDC */
    sint32 lifetime;                           /* +0xE0 */
    sint16 rotation_step;                      /* +0xE4 */
    sint16 rotation;                           /* +0xE6 */
    void (*impact_callback)(void *projectile); /* +0xE8 */
    void (*tail_callback)(void *projectile);   /* +0xEC */
    EFFECT *owner_effect;                      /* +0xF0 */
    uint32 tail_state;                         /* +0xF4 */
} PLAYER_ACTION;

#if defined(AP_32BIT)
    #define PLAYER_ACTION_OFFSET_ASSERT(field, offset) typedef char PlayerActionObject_##field##_at_##offset[(offsetof(PLAYER_ACTION, field) == 0x##offset) ? 1 : -1]
PLAYER_ACTION_OFFSET_ASSERT(collision, 00);
PLAYER_ACTION_OFFSET_ASSERT(damage, 78);
PLAYER_ACTION_OFFSET_ASSERT(animation, ac);
PLAYER_ACTION_OFFSET_ASSERT(item_type, c8);
PLAYER_ACTION_OFFSET_ASSERT(velocity_y, cc);
PLAYER_ACTION_OFFSET_ASSERT(velocity_x, d0);
PLAYER_ACTION_OFFSET_ASSERT(velocity_z, d4);
PLAYER_ACTION_OFFSET_ASSERT(gravity_y, d8);
PLAYER_ACTION_OFFSET_ASSERT(field_dc, dc);
PLAYER_ACTION_OFFSET_ASSERT(lifetime, e0);
PLAYER_ACTION_OFFSET_ASSERT(rotation_step, e4);
PLAYER_ACTION_OFFSET_ASSERT(rotation, e6);
PLAYER_ACTION_OFFSET_ASSERT(impact_callback, e8);
PLAYER_ACTION_OFFSET_ASSERT(tail_callback, ec);
PLAYER_ACTION_OFFSET_ASSERT(owner_effect, f0);
PLAYER_ACTION_OFFSET_ASSERT(tail_state, f4);
typedef char PlayerActionObject_size_f8[(sizeof(PLAYER_ACTION) == 0xf8) ? 1 : -1];
    #undef PLAYER_ACTION_OFFSET_ASSERT
#endif

/* Types. */
/* Main-executable player object allocated by FUN_80099C10.
 *
 * Names are used only where the MIPS consumers establish a role.  Unknown
 * storage deliberately keeps its address name; this is preferable to
 * inventing semantics while still making every recovered offset checkable. */
typedef struct
{
    uint16 count;    /* +0x00 */
    uint16 type;     /* +0x02 */
    uint32 frame_id; /* +0x04 */
} INVENTORY_SLOT;

typedef struct PLAYER
{
    void *previous;                    /* +0x000 */
    void *next;                        /* +0x004 */
    uint32 field_008;                  /* +0x008 */
    void (*update)(void *player);      /* +0x00C */
    void *overlap_10;                  /* +0x010 */
    void *overlap_14;                  /* +0x014 */
    void *overlap_18;                  /* +0x018 */
    sint16 object_type;                /* +0x01C */
    uint16 field_01e;                  /* +0x01E */
    sint32 x, y, z;                    /* +0x020..+0x028 */
    sint32 box_x, box_y, box_z;        /* +0x02C..+0x034 */
    sint32 width, height, depth;       /* +0x038..+0x040 */
    POLY_FT4 prim;                     /* +0x044 */
    void *frame_descriptor;            /* +0x06C */
    sint32 render_flags;               /* +0x070 */
    sint32 draw_mode;                  /* +0x074 */
    sint16 field_078;                  /* +0x078 */
    sint16 flash_clut_ticks;           /* +0x07A */
    sint32 velocity_x;                 /* +0x07C */
    sint32 velocity_y;                 /* +0x080 */
    sint32 facing_x;                   /* +0x084 */
    sint32 facing_z;                   /* +0x088 */
    ANIM animation;                    /* +0x08C */
    VRAM_SPRITE vram_descriptor;       /* +0x0A8 */
    uint8 field_0d8[0x30];             /* +0x0D8 */
    uint32 previous_buttons;           /* +0x108 */
    sint16 weapon_animation_active;    /* +0x10C */
    sint16 field_10e;                  /* +0x10E */
    sint32 direction_x;                /* +0x110 */
    sint32 direction_z;                /* +0x114 */
    sint32 field_118;                  /* +0x118 */
    sint32 field_11c;                  /* +0x11C */
    sint32 unchanged_direction_frames; /* +0x120 */
    sint16 action_lock;                /* +0x124 */
    sint16 effect_latch;               /* +0x126 */
    INVENTORY_SLOT inventory[16];      /* +0x128 */
    sint16 direction;                  /* +0x1A8 */
    sint16 previous_direction;         /* +0x1AA */
    sint16 airborne;                   /* +0x1AC */
    sint16 field_1ae;                  /* +0x1AE */
    sint16 field_1b0;                  /* +0x1B0 */
    sint16 ammunition;                 /* +0x1B2 */
    sint16 menu_resume;                /* +0x1B4 */
    sint16 field_1b6;                  /* +0x1B6 */
    sint16 spatial_sound_handle;       /* +0x1B8 */
    sint16 field_1ba;                  /* +0x1BA */
    sint16 weapon_sound_handle;        /* +0x1BC */
    sint16 field_1be;                  /* +0x1BE */
    uint32 sampled_buttons;            /* +0x1C0 */
    sint32 health;                     /* +0x1C4 */
    sint16 damage_cooldown;            /* +0x1C8 */
    sint16 field_1ca;                  /* +0x1CA */
    sint32 recovery_x;                 /* +0x1CC */
    sint32 recovery_y;                 /* +0x1D0 */
    sint32 recovery_z;                 /* +0x1D4 */
    sint16 field_1d8;                  /* +0x1D8 */
    sint16 selected_weapon;            /* +0x1DA */
    sint16 field_1dc;                  /* +0x1DC */
    sint16 death_delay;                /* +0x1DE */
    sint32 recovery_path[6];           /* +0x1E0 */
    sint16 field_1f8;                  /* +0x1F8 */
    sint16 jump_override;              /* +0x1FA */
    sint16 jump_button_latch;          /* +0x1FC */
    sint16 moving_frames;              /* +0x1FE */
    sint16 idle_frames;                /* +0x200 */
    sint16 field_202;                  /* +0x202 */
    sint32 field_204;                  /* +0x204 */
    sint32 accumulated_fall;           /* +0x208 */
    sint16 recovery_delay;             /* +0x20C */
    sint16 field_20e;                  /* +0x20E */
    sint16 field_210;                  /* +0x210 */
    sint16 grounded;                   /* +0x212 */
    uint8 field_214[0x18];             /* +0x214 */
    sint32 field_22c;                  /* +0x22C */
    void *checkpoint;                  /* +0x230 */
    sint16 fire_counter;               /* +0x234 */
    sint16 fire_mask;                  /* +0x236 */
    sint16 light_delta;                /* +0x238 */
    sint16 env_counter;                /* +0x23A */
    sint16 field_23c;                  /* +0x23C */
    sint16 special_weapon_ticks;       /* +0x23E */
    sint16 field_240;                  /* +0x240 */
    sint16 surface_state;              /* +0x242 */
    sint16 preserve_checkpoint;        /* +0x244 */
    sint16 field_246;                  /* +0x246 */
} PLAYER;

#if defined(AP_32BIT)
    #define PLAYER_OFFSET_ASSERT(field, offset) typedef char PlayerObject_##field##_at_##offset[(offsetof(PLAYER, field) == 0x##offset) ? 1 : -1]

PLAYER_OFFSET_ASSERT(x, 020);
PLAYER_OFFSET_ASSERT(box_x, 02c);
PLAYER_OFFSET_ASSERT(width, 038);
PLAYER_OFFSET_ASSERT(prim, 044);
PLAYER_OFFSET_ASSERT(frame_descriptor, 06c);
PLAYER_OFFSET_ASSERT(render_flags, 070);
PLAYER_OFFSET_ASSERT(draw_mode, 074);
PLAYER_OFFSET_ASSERT(flash_clut_ticks, 07a);
PLAYER_OFFSET_ASSERT(velocity_x, 07c);
PLAYER_OFFSET_ASSERT(velocity_y, 080);
PLAYER_OFFSET_ASSERT(facing_x, 084);
PLAYER_OFFSET_ASSERT(facing_z, 088);
PLAYER_OFFSET_ASSERT(animation, 08c);
PLAYER_OFFSET_ASSERT(vram_descriptor, 0a8);
PLAYER_OFFSET_ASSERT(previous_buttons, 108);
PLAYER_OFFSET_ASSERT(direction_x, 110);
PLAYER_OFFSET_ASSERT(direction_z, 114);
PLAYER_OFFSET_ASSERT(unchanged_direction_frames, 120);
PLAYER_OFFSET_ASSERT(action_lock, 124);
PLAYER_OFFSET_ASSERT(inventory, 128);
PLAYER_OFFSET_ASSERT(direction, 1a8);
PLAYER_OFFSET_ASSERT(airborne, 1ac);
PLAYER_OFFSET_ASSERT(ammunition, 1b2);
PLAYER_OFFSET_ASSERT(menu_resume, 1b4);
PLAYER_OFFSET_ASSERT(spatial_sound_handle, 1b8);
PLAYER_OFFSET_ASSERT(weapon_sound_handle, 1bc);
PLAYER_OFFSET_ASSERT(sampled_buttons, 1c0);
PLAYER_OFFSET_ASSERT(health, 1c4);
PLAYER_OFFSET_ASSERT(damage_cooldown, 1c8);
PLAYER_OFFSET_ASSERT(recovery_x, 1cc);
PLAYER_OFFSET_ASSERT(death_delay, 1de);
PLAYER_OFFSET_ASSERT(recovery_path, 1e0);
PLAYER_OFFSET_ASSERT(jump_override, 1fa);
PLAYER_OFFSET_ASSERT(jump_button_latch, 1fc);
PLAYER_OFFSET_ASSERT(moving_frames, 1fe);
PLAYER_OFFSET_ASSERT(idle_frames, 200);
PLAYER_OFFSET_ASSERT(accumulated_fall, 208);
PLAYER_OFFSET_ASSERT(recovery_delay, 20c);
PLAYER_OFFSET_ASSERT(grounded, 212);
PLAYER_OFFSET_ASSERT(checkpoint, 230);
PLAYER_OFFSET_ASSERT(fire_counter, 234);
PLAYER_OFFSET_ASSERT(light_delta, 238);
PLAYER_OFFSET_ASSERT(env_counter, 23a);
PLAYER_OFFSET_ASSERT(special_weapon_ticks, 23e);
PLAYER_OFFSET_ASSERT(surface_state, 242);
PLAYER_OFFSET_ASSERT(preserve_checkpoint, 244);
typedef char PlayerObject_size_248[(sizeof(PLAYER) == 0x248) ? 1 : -1];

    #undef PLAYER_OFFSET_ASSERT
#endif

/* Types. */
typedef struct
{
    COLLISION collision;             /* +0x00 */
    uint32 field_78;                 /* +0x78 */
    sint16 lifetime;                 /* +0x7C */
    uint16 field_7e;                 /* +0x7E */
    void *(*callback)(void *object); /* +0x80 */
    uint16 interval;                 /* +0x84 */
    sint16 countdown;                /* +0x86 */
    sint32 callback_value_88;        /* +0x88 */
    sint32 callback_value_8c;        /* +0x8C */
} TIMED_CALLBACK;

#if defined(AP_32BIT)
typedef char TimedCallbackObject_size_90[sizeof(TIMED_CALLBACK) == 0x90 ? 1 : -1];
typedef char TimedCallbackObject_lifetime_at_7c[offsetof(TIMED_CALLBACK, lifetime) == 0x7c ? 1 : -1];
typedef char TimedCallbackObject_callback_at_80[offsetof(TIMED_CALLBACK, callback) == 0x80 ? 1 : -1];
typedef char TimedCallbackObject_interval_at_84[offsetof(TIMED_CALLBACK, interval) == 0x84 ? 1 : -1];
typedef char TimedCallbackObject_countdown_at_86[offsetof(TIMED_CALLBACK, countdown) == 0x86 ? 1 : -1];
typedef char TimedCallbackObject_value_88_at_88[offsetof(TIMED_CALLBACK, callback_value_88) == 0x88 ? 1 : -1];
typedef char TimedCallbackObject_value_8c_at_8c[offsetof(TIMED_CALLBACK, callback_value_8c) == 0x8c ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
COLLISION_RESULT *runtime_object_collision_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 new_x, sint32 new_y, sint32 new_z, sint32 width, sint32 height);
COLLISION_RESULT *static_map_collision_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 new_x, sint32 new_y, sint32 new_z, sint32 width, sint32 height, sint32 depth);
GDB_CALL
void *archive_member_find(const char *name, const CC_ARCHIVE_HEADER *archive);
GDB_CALL TIMED_CALLBACK *timed_callback_create(void *(*callback)(void *object), sint32 x, sint32 y, sint32 z, uint16 lifetime, uint16 interval);
GDB_CALL sint16 menu_run(void *raw_entries, sint32 x, sint32 y);
GDB_CALL void player_particle_ring_create(sint32 x, sint32 y, sint32 z, sint32 rings, sint32 frames_per_ring);
PLAYER_ACTION *player_action_object_create(sint32 x, sint32 y, sint32 z, sint32 type);
SPRITE *damage_expl_create(COLLISION *source, sint32 kind);
const void *player_assets_executable_address(uint32 address);
const void *player_assets_executable_pointer(uint32 address);
const char *player_assets_executable_text_pointer(uint32 address);
const void *player_assets_resolve_address(uint32 address);
sint32 player_bob_offset_update(sint32 *counter);
sint32 player_resolve_landing_hazard(PLAYER *player, sint32 old_x, sint32 old_y, sint32 old_z);
uint16 player_animation_start(sint16 direction_buttons);
uint32 player_collision_resolve(sint32 old_x, sint32 old_y, sint32 old_z, sint32 *x, sint32 *y, sint32 *z, sint32 *velocity_x, sint32 *velocity_y, sint32 width, sint32 height, sint32 depth);
void *continuous_weapon_particle_create(sint32 x, sint32 y, sint32 z, sint16 clut);
void *expl_debris_particle_create(TIMED_CALLBACK *emitter);
void *expl_trail_particle_create(TIMED_CALLBACK *emitter);
void *player_inventory_add(sint32 type, sint32 count, void *raw_player);
void *player_inventory_find(sint32 type, void *raw_player);
void level_select_unlock_all(void);
void explosive_pickup_update(PLAYER_ACTION *object);
void player_apply_damage(void *raw_attacker, PLAYER *player);
void player_assets_runtime_initialize(void);
void player_cheats_update(void);
void player_checkpoint_recovery_begin(PLAYER *player);
void player_checkpoint_recovery_update(PLAYER *player);
void player_control_update(void *raw_player);
void player_death_begin(void);
void player_debug_movement_update(PLAYER *player);
void player_detect_ledge(sint32 x, sint32 y, sint32 z, sint32 width, sint32 height);
void player_env_damage_update(PLAYER *player);
void player_hit_flash_update(PLAYER *player);
void player_inventory_remove_empty(void *raw_slot);
void player_ledge_control_update(void *raw_player);
void player_prone_control_update(void *raw_player);
void player_spawn_at_start(void);
void selected_weapon_sync(void);
void weapon_selection_update(void);
__declspec(noinline) sint32 player_inside_xz_box(sint32 x, sint32 z, sint32 width, sint32 depth);
__declspec(noinline) void player_push_out_of_box(sint32 x, sint32 y, sint32 z, sint32 half_x, sint32 height, sint32 half_z);
void player_action_effect_create(EFFECT *effect);
void player_projectile_impact_update(void *raw_object, sint32 old_x, sint32 old_y, sint32 old_z);
void player_throw_explosive(PLAYER *player);
GDB_CALL COLLISION *player_special_projectile_create(sint32 wanted_angle);
SPRITE *player_projectile_create(PLAYER *player, sint16 fire_mode);
void player_mission_complete_update(PLAYER *player);
void player_ammunition_regenerate(void);
/* END GENERATED MODULE API */

#endif
