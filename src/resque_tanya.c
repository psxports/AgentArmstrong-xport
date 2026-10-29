#include <stddef.h>

#include "spatial.h"
#include "game_math.h"
#include "resque_tanya.h"
#include "render.h"
#include "airship.h"
#include "animation.h"
#include "game_sound.h"
#include "collision.h"
#include "effect_update.h"
#include "game.h"
#include "map_render.h"
#include "camera.h"
#include "text_renderer.h"
#include "hq.h"
#include "map.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "scuba_player.h"

/* Original: DAT_800D41E4. */
MODEL_POSE_RESOURCE g_gun_emplacement_initial_pose;

/* Original: DAT_800D41E0. */
sint16 g_gun_emplacement_model_base;

/* Types. */
typedef struct
{
    COLLISION col;
    VRAM_SPRITE desc;
    ANIM animation;
    sint16 state;
    sint32 velocity_x;
    sint32 velocity_z;
    sint32 render_scale;
    sint16 departure_started;
    sint16 player_near_frames;
    EFFECT *owner_effect;
} TANYA_ACTOR;

typedef struct
{
    DESTRUCTIBLE_HIERARCHY hierarchy;
    sint16 fire_countdown;
    MODEL_NODE nodes[11];
    MODEL_NODE *node_table[11];
    sint32 saved_node3_translation_z;
    sint32 aim_jitter_x;
} TANYA_GUN_EMPLACEMENT;

/* Original data: SLES_004.74:0x800CACE0..0x800CAD33 */
static const ANIM_COMMAND tanya_idle_animation[11] = {
    {ANIM_STEP_FRAME, {12, 97288, 0}, 0}, {ANIM_STEP_FRAME, {12, 97289, 0}, 0}, {ANIM_STEP_FRAME, {12, 97288, 0}, 0}, {ANIM_STEP_FRAME, {60, 97289, 0}, 0}, {ANIM_STEP_FRAME, {6, 97288, 0}, 0}, {ANIM_STEP_FRAME, {6, 97289, 0}, 0}, {ANIM_STEP_FRAME, {6, 97288, 0}, 0}, {ANIM_STEP_FRAME, {6, 97289, 0}, 0}, {ANIM_STEP_FRAME, {30, 97288, 0}, 0}, {ANIM_STEP_FRAME, {12, 97289, 0}, 0}, {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800CAD34..0x800CAD3F */
static const ANIM_COMMAND tanya_turn_animation[2] = {
    {ANIM_STEP_FRAME, {100, 97290, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800CAD40..0x800CAD83 */
static const ANIM_COMMAND tanya_release_animation[9] = {
    {ANIM_STEP_FRAME, {4, 97280, 0}, 0}, {ANIM_STEP_FRAME, {4, 97281, 0}, 0}, {ANIM_STEP_FRAME, {4, 97282, 0}, 0}, {ANIM_STEP_FRAME, {4, 97283, 0}, 0}, {ANIM_STEP_FRAME, {4, 97284, 0}, 0}, {ANIM_STEP_FRAME, {4, 97285, 0}, 0}, {ANIM_STEP_FRAME, {4, 97286, 0}, 0}, {ANIM_STEP_FRAME, {4, 97287, 0}, 0}, {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800CD99C..0x800CD9A6 */
static const sint8 gun_emplacement_models[11] = {
    3, 1, 2, 0, -1, -1, -1, -1, -1, -1, -1,
};

/* Original data: SLES_004.74:0x800CD9A8..0x800CD9B1 */
static const sint8 underwater_gun_models[10] = {
    2, 1, 0, -1, -1, -1, -1, -1, -1, -1,
};

/* Functions. */
/* Original: FUN_800A48A8. */
static void tanya_update(void *raw_object)
{
    TANYA_ACTOR *object = (TANYA_ACTOR *)raw_object;
    EFFECT *effect = object->owner_effect;
    sint32 old_x = object->col.x, old_y = object->col.y;
    sint32 old_z = object->col.z;
    animation_update(&object->animation);
    if (g_mission.primary[1] == 2)
    {
        if (object->desc.region != 0)
            spr_vram_defer_release(&object->desc);
        object_destroy(object);
        return;
    }
    if (!world_object_is_visible(old_x, old_y, old_z, 0xb400))
    {
        if (object->desc.region != 0)
            spr_vram_defer_release(&object->desc);
        if (object->departure_started == 0)
            effect->type = 0x2c;
        object_destroy(object);
        return;
    }
    if (object->desc.region == 0)
        spr_vram_allocate(&object->desc, 0x50, 0x56, 1);
    object->col.y = object->col.y + 0x400u;
    if (object->state == 2)
    {
        object->col.x = (uint32)object->col.x + object->velocity_x;
        object->col.z = (uint32)object->col.z + object->velocity_z;
    }
    map_motion_resolve(old_x, old_y, old_z, &object->col.x, &object->col.y, &object->col.z, 0, 0, 0x20, 0x50, 5);
    if (abs_s32(((uint32)object->col.x - g_player_world.x)) <= 0xffff && abs_s32(((uint32)object->col.y - g_player_world.y)) <= 0x7fff)
    {
        if (object->state == 0)
        {
            g_mission.counts[2]++;
            if (object->player_near_frames >= 0x33)
            {
                animation_start(&object->animation, tanya_turn_animation);
                object->departure_started = 1;
                if (object_find_next_by_type(0, 0x5c) == 0 && object->state == 0)
                {
                    mission_text_message_create(g_game_text.entries[100]);
                    object->state = 1;
                }
            }
            else
                ++object->player_near_frames;
        }
    }
    if (object->state != 0 && object->animation.next == 0)
    {
        animation_start(&object->animation, tanya_release_animation);
        object->state = 2;
        object->velocity_x = 0x100;
        object->velocity_z = -0x20;
        object->render_scale = g_default_spr_scale;
    }
    render_world_spr(object->animation.frame, object->col.x, object->col.y, object->col.z, 1, object->render_scale, object->render_scale, &object->col.prim, 0, 2, &object->desc, 0);
}

/* Original: FUN_800A479C. */
void tanya_create(EFFECT *effect)
{
    TANYA_ACTOR *object = (TANYA_ACTOR *)runtime_heap_allocate(sizeof(*object));
    object->col.object.update = tanya_update;
    linked_list_append(&g_object_list, object);
    setPolyFT4(&object->col.prim);
    setShadeTex(&object->col.prim, 0);
    object->col.x = effect->x;
    object->col.y = effect->y + 0x2000u;
    object->col.z = effect->z - 0x1000u;
    if (g_session.stage == 0x0e)
        object->col.x = object->col.x + 0x6000u;
    object->owner_effect = effect;
    effect->type = 0;
    col_box_set(object, 0x40, 0x40, 0x10);
    object->animation.owner = object;
    object->desc.clut = g_spr_pal.cluts[CLUT_TANYA];
    animation_start(&object->animation, tanya_idle_animation);
    object->render_scale = (g_default_spr_scale + 0x200u);
}

/* Original: FUN_800AC610. */
static void tanya_gun_emplacement_update(void *raw_object)
{
    TANYA_GUN_EMPLACEMENT *object = raw_object;
    MODEL_NODE *root = &object->nodes[0], *aim, *muzzle;
    FIXED_DDA_LINE aim_motion;
    sint32 horizontal_span, step_x_abs, angle;
    if (!world_object_is_visible(object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z, 0x8000))
        return;
    root->translation_x = (uint32)object->hierarchy.col.x - g_camera.world_x;
    root->translation_y = (uint32)object->hierarchy.col.y - g_camera.world_y;
    root->translation_z = (uint32)object->hierarchy.col.z - g_camera.world_z;
    player_push_out_of_box(object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z, 0x3800, 0xaa00, 0x3800);
    if ((g_render_state.frame & 0x3f) == 0)
        object->aim_jitter_x = (random_range(0x40) - 0x20) * 256;
    fixed_dda_initialize(object->nodes[1].world_x, object->nodes[1].world_y, object->nodes[1].world_z, (uint32)g_player_world.x + object->aim_jitter_x, g_player_world.y - 0x3000u, g_player_world.z, &aim_motion);
    angle = fixed_angle_from_vector(div_pow2_trunc(aim_motion.step.x, 11), div_pow2_trunc(aim_motion.step.z, 11));
    object->nodes[1].rotation_y = angle_approach_wrapped(angle << 2, 0x0e, object->nodes[1].rotation_y);
    aim = &object->nodes[g_session.scuba_active ? 3 : 4];
    fixed_dda_initialize(aim->world_x, aim->world_y, aim->world_z, (uint32)g_player_world.x + object->aim_jitter_x, g_player_world.y, g_player_world.z, &aim_motion);
    horizontal_span = abs_s32(aim_motion.step.z);
    step_x_abs = abs_s32(aim_motion.step.x);
    if (step_x_abs < horizontal_span)
        horizontal_span = horizontal_span + (uint32)div_pow2_trunc(step_x_abs, 1);
    else
        horizontal_span = step_x_abs + (uint32)div_pow2_trunc(horizontal_span, 1);
    angle = ((fixed_angle_from_vector(div_pow2_trunc(aim_motion.step.y, 11), -abs_s32(div_pow2_trunc(horizontal_span, 11))) << 2) - 0x400 - (g_session.scuba_active ? 0x400 : 0)) & 0xfff;
    if (angle < 0x190)
        angle = 0x190;
    aim->rotation_x = angle_approach_wrapped(angle, 0x0c, aim->rotation_x);
    g_model_draw.target_frame = g_cur_render_frame;
    g_model_draw.depth_bucket = 0x480 - div_pow2_trunc((g_map_render.depth_anchor_z + 0x2000u - object->hierarchy.col.z), 8);
    g_model_draw.world_z = object->hierarchy.col.z;
    if (object->hierarchy.col.flash_clut_ticks != 0)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --object->hierarchy.col.flash_clut_ticks;
    }
    model_render_node(root, &g_model_identity);
    g_model_draw.clut_override = 0;
    object->nodes[3].translation_z = object->saved_node3_translation_z;
    if (object->fire_countdown < 0x14 && !g_session.scuba_active)
    {
        object->nodes[3].translation_z = object->saved_node3_translation_z - ((g_render_state.frame << 9) & 0xe00);
        if (object->fire_countdown == 0x13)
            sound_play_positional(0x12, 0, 0x7f, object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z);
        muzzle = &object->nodes[5];
        /* Even countdown emits the original muzzle sprite */
        if ((object->fire_countdown & 1) == 0)
        {
            g_model_draw.depth_bucket = 0x480 - div_pow2_trunc(((uint32)g_map_render.depth_anchor_z - muzzle->world_z), 8);
            render_world_spr(0x1e000 | ((object->fire_countdown >> 2) & 1), muzzle->world_x, muzzle->world_y, muzzle->world_z, 0, 0x2200, 0x2200, 0, g_model_draw.depth_bucket, 0, 0, 0);
        }
        /* Projectile cadence is independent of the muzzle sprite */
        if ((g_render_state.frame & 3) == 0)
        {
            PROJECTILE *shot = projectile_create(muzzle->world_x, muzzle->world_y, muzzle->world_z, object->nodes[6].world_x, object->nodes[6].world_y, object->nodes[6].world_z);
            shot->col.hit.damage = 0x80;
            shot->col.object_type = 0;
            shot->col.sends_mask |= 1;
            shot->col.receives_mask |= 4;
        }
    }
    if (object->fire_countdown == 0 && g_session.scuba_active)
    {
        scuba_projectile_create(object->nodes[4].world_x, object->nodes[4].world_y, object->nodes[4].world_z, object->nodes[5].world_x, object->nodes[5].world_y, object->nodes[5].world_z, 0xa0);
        sound_play_positional(0xa9, 0, 0x7f, object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z);
    }
    if (object->fire_countdown == 0)
        object->fire_countdown = random_range(0x64) + 0x78;
    --object->fire_countdown;
    hierarchy_col_box_update(&object->hierarchy.col, &object->nodes[g_session.scuba_active ? 6 : 7], 4);
}

/* Original: FUN_800AC3B8. */
void tanya_gun_emplacement_create(EFFECT *effect)
{
    TANYA_GUN_EMPLACEMENT *object = (TANYA_GUN_EMPLACEMENT *)runtime_heap_allocate(sizeof(*object));
    const sint8 *models = g_session.scuba_active ? underwater_gun_models : gun_emplacement_models;
    sint32 count = g_session.scuba_active ? 10 : 11, idx;
    object->hierarchy.col.object.update = tanya_gun_emplacement_update;
    linked_list_append(&g_object_list, object);
    object->hierarchy.col.object_type = effect->type;
    effect->type = 0;
    for (idx = 0; idx < count; idx++)
    {
        MODEL_NODE *node = &object->nodes[idx];
        object->node_table[idx] = node;
        node->model_id = models[idx] < 0 ? 0 : g_gun_emplacement_model_base + models[idx];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_gun_emplacement_initial_pose, object->node_table, count, 1);
    object->hierarchy.owner_effect = effect;
    object->hierarchy.col.z = effect->z;
    object->hierarchy.col.x = effect->x;
    object->hierarchy.col.y = effect->y + 0x1000u;
    object->hierarchy.col.receives_mask = 1;
    object->hierarchy.col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->hierarchy.col.hit_callback = destructible_hierarchy_damage;
    object->hierarchy.col.hit.health = (g_session.scuba_active ? 5 : 0x28);
    object->nodes[0].scale = 0x1000;
    object->fire_countdown = 0x3c;
    object->saved_node3_translation_z = object->nodes[3].translation_z;
    object->hierarchy.map_rec = map_floor_cell_at(object->hierarchy.col.x, object->hierarchy.col.z)->grp;
}
