#include <stddef.h>
#include <stdlib.h>

#include "render.h"
#include "effect_update.h"
#include "game.h"
#include "mechanism.h"
#include "animation.h"
#include "collision.h"
#include "game_sound.h"
#include "input.h"
#include "map.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "runtime_heap.h"
#include "spatial.h"
#include "sprite.h"

/* Original: DAT_800D74D8. */
uint8 g_mechanism_switch_states[128];

typedef struct
{
    COLLISION col;
    ANIM animation;
    sint16 state;
    sint16 input_latch;
    sint16 col_latch;
    sint16 cooldown;
    EFFECT *effect;
} MECHANISM_SWITCH;

typedef struct
{
    COLLISION col;
    sint32 delta_y;
    sint32 cell_width;
    sint32 cell_height;
    sint32 cell_depth;
    EFFECT *waypoints[10];
    sint16 waypoint_count;
    sint16 waypoint_step;
    sint16 waypoint_idx;
    sint16 wait_timer;
    sint16 sound_handle;
    uint8 enabled;
    uint8 completed_trips;
    uint8 sound_disabled;
    uint8 prev_gate_state;
} LIFT;

/* Original data: SLES_004.74:0x800CAEC8..0x800CAEE3 */
static const ANIM_COMMAND switch_animation0[4] = {
    {ANIM_STEP_FRAME, {6, 155650, 0}, 0},
    {ANIM_STEP_FRAME, {6, 155649, 0}, 0},
    {ANIM_STEP_FRAME, {6, 155648, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800CAEE4..0x800CAEFF */
static const ANIM_COMMAND switch_animation1[4] = {
    {ANIM_STEP_FRAME, {6, 155648, 0}, 0},
    {ANIM_STEP_FRAME, {6, 155649, 0}, 0},
    {ANIM_STEP_FRAME, {6, 155650, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original: FUN_800A5DF4. */
static void mechanism_switch_activate(void *raw_object, void *src)
{
    MECHANISM_SWITCH *object = (MECHANISM_SWITCH *)raw_object;
    /* Keep collision and input latches as independent signed halfwords */
    if (object->cooldown == 0)
    {
        object->cooldown = 0x3c;
        object->col_latch = 1;
        object->input_latch = 0;
    }
}

/* Original: FUN_800A5B90. */
static void mechanism_switch_update(void *raw_object)
{
    MECHANISM_SWITCH *object = (MECHANISM_SWITCH *)raw_object;
    EFFECT *effect = object->effect;
    sint16 switch_idx = effect->vals[0];
    XASSERT(!(switch_idx < 0 || switch_idx >= sizeof(g_mechanism_switch_states)));
    if (object->col_latch == 0 && !world_object_is_visible(object->col.x, object->col.y, object->col.z, 0x8000))
        return;
    if (object->cooldown != 0)
        object->cooldown--;
    if (!player_inside_xz_box(object->col.x, object->col.z, 0x4000, 0x2000) || !(g_input.held & PADRright) || g_player->menu_resume != 0 || object->cooldown != 0)
    {
        if (object->col_latch == 0)
        {
            if (object->state != g_mechanism_switch_states[switch_idx])
                goto toggle;
            object->input_latch = 0;
        }
        else
            goto perform;
    }
    else
    {
    toggle:
        if (object->col_latch == 0 && object->state == g_mechanism_switch_states[switch_idx])
        {
            /* Reset grenade timer separately from the input latch */
            g_player->special_fire_ticks = 0;
            g_player->menu_resume = 1;
        }
    perform:
        object->col_latch = 0;
        if (object->input_latch == 0)
        {
            sound_play_positional(0x17, 0, 0x7f, object->col.x, object->col.y, object->col.z);
            object->state ^= 1;
            effect->vals[1] = object->state;
            animation_start(&object->animation, object->state ? switch_animation1 : switch_animation0);
        }
        object->input_latch = 1;
        object->cooldown = 0x3c;
    }
    animation_update(&object->animation);
    XASSERT(!(g_next_frame_object >= g_frame_objects + 80));
    g_next_frame_object->light_delta = 0;
    if (g_session.stage != 0x0b)
        render_world_spr(object->animation.frame, object->col.x, object->col.y, object->col.z, 0, 0x1000, 0x1000, &object->col.prim, 0, 0, 0, 0);
    g_mechanism_switch_states[switch_idx] = object->state;
}

/* Original: FUN_800A5A88. */
void mechanism_switch_create(EFFECT *effect)
{
    MECHANISM_SWITCH *object = (MECHANISM_SWITCH *)runtime_heap_allocate(sizeof(*object));
    const ANIM_COMMAND *animation;
    sint16 switch_idx = effect->vals[0];
    XASSERT(!(switch_idx < 0 || switch_idx >= sizeof(g_mechanism_switch_states)));
    effect->type = 0;
    object->col.object.update = mechanism_switch_update;
    linked_list_append(&g_object_list, object);
    setPolyFT4(&object->col.prim);
    setShadeTex(&object->col.prim, 0);
    object->col.x = effect->x;
    object->col.y = effect->y;
    object->col.z = effect->z + 0x2000;
    object->col.object_type = 0x3e;
    object->col.receives_mask = 1;
    object->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->col.hit_callback = mechanism_switch_activate;
    col_box_set(object, 0x40, 0x40, 0x10);
    object->effect = effect;
    object->state = effect->vals[1];
    animation = effect->vals[1] ? switch_animation1 : switch_animation0;
    g_mechanism_switch_states[switch_idx] = effect->vals[1];
    animation_start(&object->animation, animation);
}

/* Exact 0x8009FEB0..0x8009FFCC: shift cell-model vertices whose Y lies in
 * the moving lift slab. */
/* Original: FUN_8009FEB0. */
static void lift_deform_map_models(sint32 x, sint32 y, sint32 z, sint32 width, sint32 height, sint32 depth, sint32 delta)
{
    const MAP_FLOOR_CELL *start = map_floor_cell_at(x, z);
    uint32 row_idx = start - g_map.floor;
    sint32 min_y = y / 0x100;
    sint32 max_y = min_y + height;
    sint32 r, c, n;
    for (r = 0; r < depth; ++r)
    {
        for (c = 0; c < width; ++c)
        {
            sint32 cell_idx = row_idx + c;
            MAP_MODEL_GROUP *grp;
            sint32 grp_idx;
            XASSERT(cell_idx < g_map.floor_count);
            grp = g_map.floor[cell_idx].grp;
            grp_idx = (grp - g_map.grps);
            XASSERT(grp_idx < g_map.rec_capacity);
            n = grp_idx != 0 ? grp->active_count : 0;
            XASSERT(!(n > 0 && n > g_map.rec_capacity - grp_idx - 1));
            for (sint32 i = 0; i < n; ++i)
            {
                MAP_MODEL_INSTANCE *instance = grp + 1 + i;
                if (instance->y >= min_y && instance->y < max_y)
                    instance->y += delta;
            }
        }
        row_idx += g_map.width;
    }
}

/* Original: FUN_800A0128. */
static void lift_update(void *raw_lift)
{
    LIFT *lift = (LIFT *)raw_lift;
    PLAYER *player = g_player;
    EFFECT *src = lift->waypoints[0];
    const SOUND_VOLUME_PAIR *sound;
    sint32 shifted_x, shifted_z, far_x, near_z, bottom_y, target_y;
    sint16 gate;
    if (player->preserve_checkpoint != 0 && player->checkpoint == lift && player->col.object.update == player_checkpoint_recovery_update)
        return;
    shifted_z = (lift->col.z + 0x2000u);
    lift->col.x = lift->col.x - 0x2000u;
    shifted_x = lift->col.x;
    lift->col.z = shifted_z;
    far_x = (shifted_x + ((uint32)lift->cell_width << 14));
    near_z = (shifted_z - ((uint32)lift->cell_depth << 14));
    bottom_y = (lift->col.y + ((uint32)lift->cell_height << 8));
    if ((g_player_world.x + 0x1300) >= shifted_x && (g_player_world.x - 0x1300) < far_x && shifted_z >= (g_player_world.z - 0x400) && near_z < (g_player_world.z + 0x400) && g_player_world.y > (lift->col.y - 0x800) && g_player_world.y < bottom_y && player->grounded != 0)
    {
        player->checkpoint = lift;
        player->preserve_checkpoint = 1;
    }
    lift->col.x = lift->col.x + 0x2000u;
    lift->col.z = lift->col.z - 0x2000u;
    gate = src->vals[6];
    if (gate != 0)
    {
        uint8 state;
        XASSERT(!(gate < 0 || gate >= sizeof(g_mechanism_switch_states)));
        state = g_mechanism_switch_states[gate];
        if (lift->col.object_type != 0x96)
            lift->enabled = state;
        else if (lift->prev_gate_state != state)
        {
            lift->prev_gate_state = state;
            lift->enabled = 1;
        }
    }
    sound = sound_spatial_volume_calculate(0x40, lift->col.x, lift->col.y, lift->col.z);
    if (lift->enabled == 0 && lift->wait_timer != 0x270f)
    {
        lift->wait_timer = 1;
        sound = 0;
    }
    if (lift->sound_disabled == 0)
    {
        if (lift->sound_handle == -1)
        {
            if (sound)
                lift->sound_handle = sound_play_positional(0x16, 0, 0x40, g_player_world.x, g_player_world.y, g_player_world.z);
        }
        else if (!sound)
        {
            sound_voice_stop(&lift->sound_handle);
            lift->sound_handle = -1;
        }
        else
            sound_voice_spatial_volume_update(sound, lift->sound_handle);
    }
    if (lift->wait_timer == 0 && lift->enabled != 0)
    {
        lift_deform_map_models(lift->col.x, lift->col.y, lift->col.z, lift->cell_width, lift->cell_height, lift->cell_depth, lift->delta_y / 0x100);
        lift->col.y = (uint32)lift->col.y + lift->delta_y;
        XASSERT(!(lift->waypoint_idx < 0 || lift->waypoint_idx >= 10 || lift->waypoints[lift->waypoint_idx] == 0));
        target_y = (lift->waypoints[lift->waypoint_idx]->y + 0x1000u);
        if (lift->col.y == target_y)
        {
            lift->waypoint_idx = (lift->waypoint_idx + lift->waypoint_step);
            if (lift->waypoint_idx == lift->waypoint_count - 1)
                lift->waypoint_step = -1;
            if (lift->waypoint_idx == 0)
                lift->waypoint_step = 1;
            XASSERT(!(lift->waypoint_idx < 0 || lift->waypoint_idx >= 10 || lift->waypoints[lift->waypoint_idx] == 0));
            target_y = lift->waypoints[lift->waypoint_idx]->y;
            lift->delta_y = lift->col.y >= target_y ? -0x200 : 0x200;
            lift->wait_timer = 0x18;
            if (lift->col.object_type == 0x96)
                lift->delta_y *= 8;
            if (src->vals[6] != 0)
            {
                if (lift->col.object_type == 0x5a)
                {
                    lift->enabled = 0;
                    g_mechanism_switch_states[src->vals[6]] = 0;
                    lift->wait_timer = 0;
                }
                if (lift->col.object_type == 0x96)
                    lift->enabled = 0;
            }
            if (++lift->completed_trips >= 0x80)
                lift->completed_trips = 0x7f;
            if (lift->completed_trips == 2 && lift->col.object_type == 0x5b)
            {
                lift->wait_timer = 0x270f;
                sound_voice_stop(&lift->sound_handle);
                lift->sound_disabled = 1;
            }
        }
    }
    else if (lift->wait_timer != 0x270f)
        lift->wait_timer = (lift->wait_timer - 1);
}

/* Original: FUN_8009FFD0. */
void lift_create(EFFECT *effect)
{
    LIFT *lift = (LIFT *)runtime_heap_allocate(sizeof(*lift));
    EFFECT *scan = 0;
    sint16 route = effect->vals[0], count = 1;
    lift->col.object.update = lift_update;
    linked_list_append(&g_object_list, lift);
    lift->col.object_type = effect->type;
    effect->type = 0;
    lift->col.x = effect->x;
    lift->col.y = effect->y + 0x1000u;
    lift->col.z = effect->z;
    lift->cell_width = (sint16)effect->vals[2];
    lift->cell_height = (sint16)effect->vals[3];
    lift->cell_depth = (sint16)effect->vals[4];
    lift->waypoints[0] = effect;
    for (;;)
    {
        scan = effect_find_next(0x1e, scan);
        if (!scan)
            break;
        if (scan->signed_vals[0] == route)
        {
            sint16 waypoint_idx = scan->vals[1];
            XASSERT(!(waypoint_idx < 0 || waypoint_idx >= 10 || count == 10));
            ++count;
            lift->waypoints[waypoint_idx] = scan;
        }
        ++scan;
    }
    lift->waypoint_step = 1;
    lift->sound_handle = -1;
    lift->waypoint_count = count;
    lift->enabled = 1;
    if (effect->vals[6] != 0)
    {
        sint16 switch_idx = effect->vals[6];
        XASSERT(!(switch_idx < 0 || switch_idx >= sizeof(g_mechanism_switch_states)));
        lift->enabled = g_mechanism_switch_states[switch_idx];
        if (lift->enabled == 0)
            lift->prev_gate_state = 1;
    }
}
