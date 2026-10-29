#ifndef MODULE_API_PLAYER_H
#define MODULE_API_PLAYER_H

#include <stddef.h>

#include "animation.h"
#include "game_math.h"
#include "xport.h"
#include "psx.h"
#include "cc_archive.h"
#include "collision.h"
#include "effect_update.h"
#include "sprite.h"
#include "map.h"

extern const ANIM_COMMAND g_player_run_animation0[3];
extern const ANIM_COMMAND g_player_run_animation4[3];

/* Publications retain their original update timing across object replacement */
typedef struct PLAYER_WORLD_STATE
{
    sint32 x, y, z;
    sint32 saved_x, saved_y, saved_z;
    sint32 delta_x, delta_y, delta_z;
} PLAYER_WORLD_STATE;

extern PLAYER_WORLD_STATE g_player_world;

/* Player-owned render-frame light request and retained direction */
typedef struct PLAYER_MUZZLE_LIGHT
{
    sint32 pending;
    sint32 dir_x, dir_z;
} PLAYER_MUZZLE_LIGHT;

extern PLAYER_MUZZLE_LIGHT g_player_muzzle_light;
extern const MAP_STAMP g_player_muzzle_light_stamp;
extern sint16 g_white_signal_flare_clut;
extern sint16 g_player_spr_clut;

/* Types. */
extern sint32 g_sel_weapon_slot;
extern uint8 g_weapon_cheat_enabled;
extern uint8 g_invulnerability_cheat_enabled;
extern uint8 g_level_select_cheat_enabled;

typedef struct
{
    uint8 *overbins;
    uint32 overbins_size;
    uint8 *diver;
    uint32 diver_size;
} PLAYER_ASSET_FILES;

extern PLAYER_ASSET_FILES g_player_asset_files;
GDB_CALL void overbins_load(void);

/* Generic mission item created by FUN_8009F274 */
typedef struct PLAYER_ACTION
{
    COLLISION col;
    ANIM animation;
    sint32 item_type;
    sint32 velocity_y;
    sint32 velocity_x;
    sint32 velocity_z;
    sint32 gravity_y;
    sint32 spin_ticks;
    sint32 lifetime;
    sint16 rotation_step;
    sint16 rotation;
    void (*impact_callback)(struct PLAYER_ACTION *projectile);
    void (*tail_callback)(struct PLAYER_ACTION *projectile);
    EFFECT *owner_effect;

    union
    {
        uint32 age;
        EFFECT *explosion_waypoint;
    } extra;
} PLAYER_ACTION;

/* Types. */
/* Native inventory fields are encoded separately by the save codec */
typedef struct
{
    uint16 count;
    uint16 type;
    uint32 frame_id;
} INVENTORY_SLOT;

/* Original player owner allocated by FUN_80099C10 */
typedef struct PLAYER
{
    COLLISION col;
    sint32 velocity_x;
    sint32 velocity_y;
    sint32 facing_x;
    sint32 facing_z;
    ANIM animation;
    VRAM_SPRITE vram_desc;
    uint32 prev_buttons;
    sint16 weapon_animation_active;
    sint32 dir_x;
    sint32 dir_z;
    sint32 throw_dir_x;
    sint32 throw_dir_z;
    sint32 unchanged_dir_frames;
    sint16 action_lock;
    sint16 effect_latch;
    INVENTORY_SLOT inventory[16];
    sint16 dir;
    sint16 prev_dir;
    sint16 airborne;
    sint16 special_fire_latch;
    sint16 ammunition;
    sint16 menu_resume;
    sint16 special_fire_ticks;
    sint16 spatial_sound_handle;
    sint16 spatial_sound_id;
    sint16 weapon_sound_handle;
    uint32 sampled_buttons;
    sint32 health;
    sint16 damage_cooldown;
    sint16 idle_animation_ticks;
    sint32 recovery_x;
    sint32 recovery_y;
    sint32 recovery_z;
    sint16 sel_weapon;
    sint16 powered_rounds;
    sint16 death_delay;
    FIXED_DDA_LINE recovery_path;
    sint16 ledge_frame;
    sint16 jump_override;
    sint16 jump_button_latch;
    sint16 moving_frames;
    sint16 idle_frames;
    sint32 move_speed_boost;
    sint32 accumulated_fall;
    sint16 recovery_delay;
    sint16 grounded;
    sint32 input_mask;
    void *checkpoint;
    sint16 fire_counter;
    sint16 fire_mask;
    sint16 light_delta;
    sint16 env_counter;
    sint16 continuous_fire_ticks;
    sint16 special_weapon_ticks;
    sint16 thrown_item_type;
    sint16 surface_state;
    sint16 preserve_checkpoint;
    sint16 air;
} PLAYER;

extern PLAYER *g_player;

/* Types. */
typedef struct
{
    COLLISION col;
    sint16 lifetime;
    void *(*callback)(void *object);
    uint16 interval;
    sint16 countdown;
    sint32 callback_val_88;
    sint32 callback_val_8c;
} TIMED_CALLBACK;

/* BEGIN GENERATED MODULE API */
GDB_CALL TIMED_CALLBACK *timed_callback_create(void *(*callback)(void *object), sint32 x, sint32 y, sint32 z, uint16 lifetime, uint16 interval);
GDB_CALL void player_particle_ring_create(sint32 x, sint32 y, sint32 z, sint32 rings, sint32 frames_per_ring);
PLAYER_ACTION *player_action_object_create(sint32 x, sint32 y, sint32 z, sint32 type);
SPRITE *damage_expl_create(COLLISION *src, sint32 kind);
sint32 player_bob_offset_update(sint32 *counter);
uint16 player_animation_start(uint16 dir_buttons);
void *continuous_weapon_particle_create(sint32 x, sint32 y, sint32 z, sint16 clut);
void *expl_debris_particle_create(void *raw_emitter);
void *expl_trail_particle_create(void *raw_emitter);
INVENTORY_SLOT *player_inventory_add(sint32 type, sint32 count, PLAYER *player);
INVENTORY_SLOT *player_inventory_find(sint32 type, PLAYER *player);
void level_select_unlock_all(void);
void player_apply_damage(void *raw_attacker, void *raw_player);
void player_assets_runtime_initialize(void);
void player_cheats_update(void);
void player_checkpoint_recovery_begin(PLAYER *player);
void player_checkpoint_recovery_update(void *raw_player);
void player_control_update(void *raw_player);
void player_death_begin(void);
void player_debug_movement_update(void);
void player_detect_ledge(sint32 x, sint32 y, sint32 z, sint32 width, sint32 height);
void player_ledge_control_update(void *raw_player);
void player_prone_control_update(void *raw_player);
void player_spawn_at_start(void);
void weapon_selection_update(void);
__declspec(noinline) sint32 player_inside_xz_box(sint32 x, sint32 z, sint32 width, sint32 depth);
__declspec(noinline) void player_push_out_of_box(sint32 x, sint32 y, sint32 z, sint32 half_x, sint32 height, sint32 half_z);
void player_action_effect_create(EFFECT *effect);
GDB_CALL COLLISION *player_special_projectile_create(sint16 wanted_angle);
void player_mission_complete_update(void *player_val);
sint32 player_angle_from_point(sint32 world_x, sint32 world_z, sint32 divisor);
/* END GENERATED MODULE API */

/* Native stage-heap lifetime boundary */
void player_heap_reset(void);
const ANIM_COMMAND *player_continuous_weapon_script(void);

#endif
