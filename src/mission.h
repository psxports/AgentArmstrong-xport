#ifndef MODULE_API_MISSION_H
#define MODULE_API_MISSION_H

#include <stddef.h>

#include "animation.h"
#include "xport.h"
#include "psx.h"
#include "collision.h"
#include "effect_update.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "sprite.h"

/* Types. */
typedef struct MissionRecord
{
    uint16 connection;
    uint8 stage;
    uint8 region;
    sint16 x, y;
    sint16 neighbour[4];
    uint8 map_index;
    uint8 unlocked;
    uint8 next_connection;
    uint8 pad_13;
} MissionRecord;

typedef struct
{
    COLLISION collision;            /* +0x00 */
    uint16 field_78;                /* +0x78 */
    uint16 field_7a;                /* +0x7A */
    sint32 ladder_x;                /* +0x7C */
    sint32 horizontal_velocity;     /* +0x80 */
    sint32 field_84;                /* +0x84 */
    sint32 velocity_limit;          /* +0x88 */
    sint32 horizontal_acceleration; /* +0x8C */
    sint32 horizontal_direction;    /* +0x90 */
    sint16 player_frame;            /* +0x94 */
    sint16 player_captured;         /* +0x96 */
    sint16 capture_delay;           /* +0x98 */
    sint16 sound_handle;            /* +0x9A */
    sint16 phase_counter;           /* +0x9C */
    uint16 field_9e;                /* +0x9E */
} MISSION_EXTRACTION_LADDER;

#if defined(AP_32BIT)
typedef char MissionExtractionLadder_size_a0[sizeof(MISSION_EXTRACTION_LADDER) == 0xA0 ? 1 : -1];
    #define MISSION_LADDER_OFFSET_ASSERT(field, offset) typedef char MissionExtractionLadder_##field##_at_##offset[offsetof(MISSION_EXTRACTION_LADDER, field) == 0x##offset ? 1 : -1]
MISSION_LADDER_OFFSET_ASSERT(ladder_x, 7c);
MISSION_LADDER_OFFSET_ASSERT(horizontal_velocity, 80);
MISSION_LADDER_OFFSET_ASSERT(field_84, 84);
MISSION_LADDER_OFFSET_ASSERT(velocity_limit, 88);
MISSION_LADDER_OFFSET_ASSERT(horizontal_acceleration, 8c);
MISSION_LADDER_OFFSET_ASSERT(horizontal_direction, 90);
MISSION_LADDER_OFFSET_ASSERT(player_frame, 94);
MISSION_LADDER_OFFSET_ASSERT(player_captured, 96);
MISSION_LADDER_OFFSET_ASSERT(capture_delay, 98);
MISSION_LADDER_OFFSET_ASSERT(sound_handle, 9a);
MISSION_LADDER_OFFSET_ASSERT(phase_counter, 9c);
    #undef MISSION_LADDER_OFFSET_ASSERT
#endif
typedef struct
{
    COLLISION collision;   /* +0x00 */
    sint16 field_78;       /* +0x78 */
    sint16 field_7a;       /* +0x7A */
    sint32 cell_width;     /* +0x7C */
    sint32 cell_rows;      /* +0x80 */
    sint32 vertical_span;  /* +0x84 */
    sint32 map_x;          /* +0x88 */
    sint32 map_z;          /* +0x8C */
    sint32 age;            /* +0x90 */
    sint32 lifetime;       /* +0x94 */
    sint32 field_98;       /* +0x98 */
    sint32 upper_height;   /* +0x9C */
    sint32 lower_height;   /* +0xA0 */
    EFFECT *source_effect; /* +0xA4 */
    sint16 sound_handle;   /* +0xA8 */
    sint16 sound_delay;    /* +0xAA */
} BUILDING;

typedef struct
{
    COLLISION collision;   /* +0x00 */
    uint32 field_78;       /* +0x78 */
    EFFECT *source_effect; /* +0x7C */
} BUILDING_COLLAPSE;

#if defined(AP_32BIT)
typedef char DestructibleBuildingObject_size_ac[sizeof(BUILDING) == 0xac ? 1 : -1];
typedef char DestructibleBuildingObject_cells_at_7c[offsetof(BUILDING, cell_width) == 0x7c ? 1 : -1];
typedef char DestructibleBuildingObject_rows_at_80[offsetof(BUILDING, cell_rows) == 0x80 ? 1 : -1];
typedef char DestructibleBuildingObject_span_at_84[offsetof(BUILDING, vertical_span) == 0x84 ? 1 : -1];
typedef char DestructibleBuildingObject_map_x_at_88[offsetof(BUILDING, map_x) == 0x88 ? 1 : -1];
typedef char DestructibleBuildingObject_map_z_at_8c[offsetof(BUILDING, map_z) == 0x8c ? 1 : -1];
typedef char DestructibleBuildingObject_age_at_90[offsetof(BUILDING, age) == 0x90 ? 1 : -1];
typedef char DestructibleBuildingObject_lifetime_at_94[offsetof(BUILDING, lifetime) == 0x94 ? 1 : -1];
typedef char DestructibleBuildingObject_upper_at_9c[offsetof(BUILDING, upper_height) == 0x9c ? 1 : -1];
typedef char DestructibleBuildingObject_lower_at_a0[offsetof(BUILDING, lower_height) == 0xa0 ? 1 : -1];
typedef char DestructibleBuildingObject_effect_at_a4[offsetof(BUILDING, source_effect) == 0xa4 ? 1 : -1];
typedef char DestructibleBuildingObject_sound_at_a8[offsetof(BUILDING, sound_handle) == 0xa8 ? 1 : -1];
typedef char DestructibleBuildingObject_delay_at_aa[offsetof(BUILDING, sound_delay) == 0xaa ? 1 : -1];
typedef char BuildingDamageTrigger_size_80[sizeof(BUILDING_COLLAPSE) == 0x80 ? 1 : -1];
typedef char BuildingDamageTrigger_effect_at_7c[offsetof(BUILDING_COLLAPSE, source_effect) == 0x7c ? 1 : -1];
#endif

/* Types. */
/* Resident 0x1C0 route-following enemy allocated by FUN_800A2198.
 * Names are limited to roles established by the constructor/update and their
 * direct callbacks; unresolved bytes retain address-derived names. */
typedef struct
{
    COLLISION collision;                  /* +0x000 */
    sint16 health;                        /* +0x078 */
    sint16 flash_clut_ticks;              /* +0x07A */
    ANIM animation;                       /* +0x07C */
    sint32 horizontal_flip;               /* +0x098 */
    EFFECT *route_points[30];             /* +0x09C */
    uint8 descriptor[0x30];               /* +0x114 */
    EFFECT *source_effect;                /* +0x144 */
    sint32 movement_line[6];              /* +0x148 */
    sint16 route_index;                   /* +0x160 */
    sint16 route_count;                   /* +0x162 */
    sint16 movement_direction;            /* +0x164 */
    sint16 display_direction;             /* +0x166 */
    sint16 attack_active;                 /* +0x168 */
    sint16 secondary_active;              /* +0x16A */
    sint16 alternate_attack;              /* +0x16C */
    sint16 attack_countdown;              /* +0x16E */
    sint16 attack_delay;                  /* +0x170 */
    sint16 turn_countdown;                /* +0x172 */
    sint16 route_step;                    /* +0x174 */
    sint16 movement_slowed;               /* +0x176 */
    sint32 vertical_velocity;             /* +0x178 */
    sint32 target_x;                      /* +0x17C */
    sint32 target_y;                      /* +0x180 */
    sint32 target_z;                      /* +0x184 */
    sint16 scale_transition;              /* +0x188 */
    sint16 palette_offset;                /* +0x18A */
    uint8 special_attack_enabled;         /* +0x18C */
    uint8 turn_attack_enabled;            /* +0x18D */
    uint8 hit_reaction_enabled;           /* +0x18E */
    sint8 alternate_attack_chance;        /* +0x18F */
    uint8 muzzle_flash_enabled;           /* +0x190 */
    uint8 field_191[3];                   /* +0x191 */
    sint32 muzzle_y_offset;               /* +0x194 */
    const sint32 *walk_frames;            /* +0x198 */
    const sint32 *attack_frames;          /* +0x19C */
    const sint32 *special_attack_frames;  /* +0x1A0 */
    const sint32 *secondary_frames;       /* +0x1A4 */
    sint32 render_resource;               /* +0x1A8 */
    sint32 movement_speed;                /* +0x1AC */
    const sint32 *attack_animation;       /* +0x1B0 */
    const sint32 *patrol_animation;       /* +0x1B4 */
    void (*attack_callback)(void *enemy); /* +0x1B8 */
    uint32 field_1bc;                     /* +0x1BC */
} MISSION_ENEMY_ACTOR;

#if defined(AP_32BIT)
    #define MISSION_ENEMY_OFFSET_ASSERT(field, offset) typedef char MissionEnemyActor_##field##_at_##offset[offsetof(MISSION_ENEMY_ACTOR, field) == 0x##offset ? 1 : -1]
MISSION_ENEMY_OFFSET_ASSERT(health, 078);
MISSION_ENEMY_OFFSET_ASSERT(animation, 07c);
MISSION_ENEMY_OFFSET_ASSERT(route_points, 09c);
MISSION_ENEMY_OFFSET_ASSERT(descriptor, 114);
MISSION_ENEMY_OFFSET_ASSERT(source_effect, 144);
MISSION_ENEMY_OFFSET_ASSERT(movement_line, 148);
MISSION_ENEMY_OFFSET_ASSERT(route_index, 160);
MISSION_ENEMY_OFFSET_ASSERT(vertical_velocity, 178);
MISSION_ENEMY_OFFSET_ASSERT(target_x, 17c);
MISSION_ENEMY_OFFSET_ASSERT(scale_transition, 188);
MISSION_ENEMY_OFFSET_ASSERT(special_attack_enabled, 18c);
MISSION_ENEMY_OFFSET_ASSERT(muzzle_y_offset, 194);
MISSION_ENEMY_OFFSET_ASSERT(walk_frames, 198);
MISSION_ENEMY_OFFSET_ASSERT(render_resource, 1a8);
MISSION_ENEMY_OFFSET_ASSERT(attack_callback, 1b8);
typedef char MissionEnemyActor_size_1c0[sizeof(MISSION_ENEMY_ACTOR) == 0x1C0 ? 1 : -1];
    #undef MISSION_ENEMY_OFFSET_ASSERT
#endif

/* Types. */
typedef struct MISSION_HUD_BAR MISSION_HUD_BAR;

typedef struct
{
    uint8 field_00[0x0c];         /* +0x00 */
    void (*update)(void *object); /* +0x0C */
    uint8 field_10[0x10];         /* +0x10 */
    sint32 x;                     /* +0x20 */
    sint32 y;                     /* +0x24 */
    uint8 field_28[0x54];         /* +0x28 */
    sint32 field_7c;              /* +0x7C */
    sint32 z;                     /* +0x80 */
    char text[32];                /* +0x84 */
    sint16 elapsed_seconds;       /* +0xA4 */
    uint16 field_a6;              /* +0xA6 */
} MISSION_TIMER_DISPLAY;

typedef struct
{
    COLLISION collision;     /* +0x00 */
    uint32 field_78;         /* +0x78 */
    char text[32];           /* +0x7C */
    sint16 text_width;       /* +0x9C */
    sint16 lifetime;         /* +0x9E */
    const char *source_text; /* +0xA0 */
    uint8 steady;            /* +0xA4 */
    uint8 slot;              /* +0xA5 */
    uint8 field_a6[2];       /* +0xA6 */
} MISSION_TEXT_MESSAGE;

#if defined(AP_32BIT)
typedef char MissionTimerDisplay_size_a8[sizeof(MISSION_TIMER_DISPLAY) == 0xA8 ? 1 : -1];
typedef char MissionTimerDisplay_text_at_84[offsetof(MISSION_TIMER_DISPLAY, text) == 0x84 ? 1 : -1];
typedef char MissionTimerDisplay_elapsed_at_a4[offsetof(MISSION_TIMER_DISPLAY, elapsed_seconds) == 0xA4 ? 1 : -1];
typedef char MissionTextMessage_size_a8[sizeof(MISSION_TEXT_MESSAGE) == 0xA8 ? 1 : -1];
typedef char MissionTextMessage_text_at_7c[offsetof(MISSION_TEXT_MESSAGE, text) == 0x7C ? 1 : -1];
typedef char MissionTextMessage_source_at_a0[offsetof(MISSION_TEXT_MESSAGE, source_text) == 0xA0 ? 1 : -1];
typedef char MissionTextMessage_steady_at_a4[offsetof(MISSION_TEXT_MESSAGE, steady) == 0xA4 ? 1 : -1];
#endif

/* BEGIN GENERATED MODULE API */
void *mission_text_pointer(uint32 table, sint32 index);
MissionRecord *mission_record_find_by_stage(sint16 stage);
PROJECTILE *projectile_create(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1);
MISSION_ENEMY_ACTOR *mission_enemy_create(EFFECT *source);
MISSION_TEXT_MESSAGE *mission_text_message_create(const char *text);
MISSION_TIMER_DISPLAY *mission_timer_display_create(sint32 x, sint32 y, sint32 z);
sint32 angle_to_player(sint32 x, sint32 z, sint32 divisor);
sint32 mission_enemy_update_visibility(COLLISION *object, EFFECT *source, sint16 type, uint16 clut);
sint32 mission_objective_script_update(sint32 script_address);
sint32 route_waypoints_collect(sint16 route, sint16 index_bias, EFFECT **waypoints);
void airfield_enemy_configure(MISSION_ENEMY_ACTOR *object);
void airfield_enemy_fire(MISSION_ENEMY_ACTOR *object);
void ambient_camera_sprite_create(void);
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
void mission_enemy_fire(MISSION_ENEMY_ACTOR *object);
void mission_hud_render(void);
void mission_unlock_text_show(sint32 stage);
void mortar_create(EFFECT *source);
void tower_surface_trigger_update(EFFECT *effect);
/* END GENERATED MODULE API */

#endif
