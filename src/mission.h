#ifndef MODULE_API_MISSION_H
#define MODULE_API_MISSION_H

#include <stddef.h>

#include "animation.h"
#include "game_math.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "effect_update.h"
#include "hq.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "sprite.h"

typedef enum MISSION_OBJECTIVE_OP
{
    MISSION_OBJECTIVE_PRIMARY = -2,
    MISSION_OBJECTIVE_SECONDARY = -3,
    MISSION_OBJECTIVE_TARGETS = -4,
    MISSION_OBJECTIVE_ACTION = -5,
    MISSION_OBJECTIVE_END = -6,
    MISSION_OBJECTIVE_TIME_LIMIT = -7,
    MISSION_OBJECTIVE_NO_TIME_LIMIT = -8,
    MISSION_OBJECTIVE_COUNT_NONZERO = -10,
    MISSION_OBJECTIVE_COUNT_ZERO = -12,
    MISSION_OBJECTIVE_COUNT_ABOVE = -13,
    MISSION_OBJECTIVE_TIMER_CHECK = -14
} MISSION_OBJECTIVE_OP;

typedef struct MISSION_OBJECTIVE_COMMAND
{
    MISSION_OBJECTIVE_OP op;
    sint16 arg, threshold;
} MISSION_OBJECTIVE_COMMAND;

typedef struct MISSION_OBJECTIVE_SCRIPT
{
    const MISSION_OBJECTIVE_COMMAND *commands;
    uint32 count;
} MISSION_OBJECTIVE_SCRIPT;

typedef enum MISSION_COUNT
{
    MISSION_COUNT_REWARD_OBJECTS = 14,
    MISSION_COUNT_FLAME_TARGETS = 18,
    MISSION_COUNT_SPECIAL_TARGETS = 20,
    MISSION_COUNT_AIRSHIP_DONE = 21,
    MISSION_COUNT_MECHANOID_DONE = 22
} MISSION_COUNT;

extern sint16 g_target_indicator_enabled;

/* Types. */
typedef struct
{
    COLLISION col;
    sint32 ladder_x;
    sint32 horizontal_velocity;
    sint32 velocity_limit;
    sint32 horizontal_acceleration;
    sint32 horizontal_dir;
    sint16 player_frame;
    sint16 player_captured;
    sint16 capture_delay;
    sint16 sound_handle;
    sint16 phase_counter;
} MISSION_EXTRACTION_LADDER;

typedef struct
{
    COLLISION col;
    sint32 cell_width;
    sint32 cell_rows;
    sint32 vertical_span;
    sint32 map_x;
    sint32 map_z;
    sint32 age;
    sint32 lifetime;
    sint32 upper_height;
    sint32 lower_height;
    EFFECT *src_effect;
    sint16 sound_handle;
    sint16 sound_delay;
} BUILDING;

typedef struct
{
    COLLISION col;
    EFFECT *src_effect;
} BUILDING_COLLAPSE;

/* Types. */
/* Native route-following enemy owned by FUN_800A2198 */
typedef struct
{
    COLLISION col;
    ANIM animation;
    sint32 horizontal_flip;
    EFFECT *route_points[30];
    VRAM_SPRITE desc;
    EFFECT *src_effect;
    FIXED_DDA_LINE movement_line;
    sint16 route_idx;
    sint16 route_count;
    sint16 movement_dir;
    sint16 disp_dir;
    sint16 attack_active;
    sint16 secondary_active;
    sint16 alternate_attack;
    sint16 attack_countdown;
    sint16 attack_delay;
    sint16 turn_countdown;
    sint16 route_step;
    sint16 movement_slowed;
    sint32 vertical_velocity;
    sint32 target_x;
    sint32 target_y;
    sint32 target_z;
    sint16 scale_transition;
    sint16 pal_offset;
    uint8 special_attack_enabled;
    uint8 turn_attack_enabled;
    uint8 hit_reaction_enabled;
    sint8 alternate_attack_chance;
    uint8 muzzle_flash_enabled;
    sint32 muzzle_y_offset;
    const sint32 *walk_frames;
    const sint32 *attack_frames;
    const sint32 *special_attack_frames;
    const sint32 *secondary_frames;
    uint32 render_resource;
    sint32 movement_speed;
    const ANIM_COMMAND *attack_animation;
    const ANIM_COMMAND *patrol_animation;
    void (*attack_callback)(void *enemy);
    uint32 alternate_barrel;
} MISSION_ENEMY_ACTOR;

/* Types. */
typedef struct
{
    sint32 x, y;
    sint32 width;
    sint32 maximum;
    POLY_G4 fill;
    POLY_F4 border;
} MISSION_HUD_BAR;

extern MISSION_HUD_BAR g_boss_health_bar;
extern MISSION_HUD_BAR g_player_health_bar;

typedef struct MISSION_TIMER_DISPLAY
{
    COLLISION col;
    sint32 x, y, z;
    char text[32];
    sint16 elapsed_seconds;
} MISSION_TIMER_DISPLAY;

typedef struct
{
    COLLISION col;
    sint16 text_width;
    sint16 lifetime;
    const char *src_text;
    uint8 steady;
    uint8 slot;
    char text[];
} MISSION_TEXT_MESSAGE;

/* Native mission state; original level-entry resets remain at their call sites */
typedef struct MISSION_STATE
{
    /* Originals: 800E7928, 800EBEF8, 800EA170, 800E7F40, 800EA658 */
    uint8 primary[16], secondary[16], actions[32], counts[32], message_slots[16];
    sint32 primary_count, secondary_count;
    sint32 primary_targets_done, secondary_targets_done;
    sint32 primary_targets_total, secondary_targets_total;
    sint32 primary_action_present, timer_visible, ending, enemy_updates, required_items;
    MISSION_TIMER_DISPLAY *timer;
} MISSION_STATE;

extern MISSION_STATE g_mission;

/* BEGIN GENERATED MODULE API */
const char *mission_text_pointer(const char *const *table, sint32 idx);
MISSION_ENEMY_ACTOR *mission_enemy_create(EFFECT *src);
VRAM_SPRITE *sentry_create(EFFECT *effect);
MISSION_TEXT_MESSAGE *mission_text_message_create(const char *text);
MISSION_TIMER_DISPLAY *mission_timer_disp_create(sint32 x, sint32 y, sint32 z);
sint32 mission_enemy_update_visibility(COLLISION *object, EFFECT *src, sint16 type, uint16 clut);
sint32 mission_objective_counts(void);
void mission_required_items_check(void);
uint32 mission_objective_script_update(const MISSION_OBJECTIVE_SCRIPT *script);
sint16 route_waypoints_collect(sint16 route, sint16 idx_bias, EFFECT **waypoints, sint32 capacity);
void airfield_enemy_configure(MISSION_ENEMY_ACTOR *object);
void ambient_camera_spr_create(void);
void ambient_speech_timer_update(void);
void building_collapse_create(EFFECT *effect);
void building_create(EFFECT *effect);
void building_map_deform(sint32 world_x, sint32 world_z, sint32 width, sint32 rows, sint32 delta, sint32 upper, sint32 lower);
void drop_range_create(EFFECT *first);
void falling_hazard_create(EFFECT *effect);
void flare_area_create(EFFECT *first);
void hud_bar_initialize(MISSION_HUD_BAR *hud, sint32 x, sint32 y, sint32 width, sint32 maximum);
void hud_bar_render(MISSION_HUD_BAR *hud, sint32 amount, sint32 unused, sint32 zero_width);
void mission_complete_begin(void);
void mission_hud_render(void);
void mission_unlock_text_show(sint16 stage);
void mortar_create(EFFECT *src);
void tower_surface_trigger_update(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
