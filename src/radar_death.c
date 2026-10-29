#include <stdlib.h>
#include <stddef.h>

#include "spatial.h"
#include "game_math.h"
#include "game.h"
#include "airship.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "render.h"
#include "mission.h"
#include "hq.h"
#include "map.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "radar_death.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "scuba_player.h"

/* Original: DAT_800D40F0. */
MODEL_POSE_RESOURCE g_radar_initial_pose;

/* Original: DAT_800D40E4. */
sint16 g_radar_vehicle_model_base;

/* Types. */
typedef struct
{
    DESTRUCTIBLE_HIERARCHY hierarchy;
    MODEL_NODE nodes[9];
    MODEL_NODE *node_table[9];
    sint32 saved_node3_translation_z;
} RADAR_VEHICLE;

/* Functions. */

/* Original: SLES_004.74:FUN_800AD218. */
static void radar_vehicle_update(void *raw_object)
{
    RADAR_VEHICLE *object = raw_object;
    MODEL_NODE *root = &object->nodes[0], *aim = &object->nodes[4];
    EFFECT *effect = object->hierarchy.owner_effect;
    FIXED_DDA_LINE aim_motion;
    sint32 horizontal_span, step_x_abs, angle;
    if (!world_object_is_visible(object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z, 0x8000))
    {
        object_destroy(object);
        effect->type = 0x38;
        return;
    }
    player_push_out_of_box(object->hierarchy.col.x, object->hierarchy.col.y, object->hierarchy.col.z, 0x3800, 0xaa00, 0x3800);
    root->translation_x = (uint32)object->hierarchy.col.x - g_camera.world_x;
    root->translation_y = (uint32)object->hierarchy.col.y - g_camera.world_y;
    root->translation_z = (uint32)object->hierarchy.col.z - g_camera.world_z;
    object->nodes[1].rotation_y = (object->nodes[1].rotation_y + 0x30) & 0xfff;
    fixed_dda_initialize(aim->world_x, aim->world_y, aim->world_z, g_player_world.x, g_player_world.y, g_player_world.z, &aim_motion);
    horizontal_span = abs_s32(aim_motion.step.z);
    step_x_abs = abs_s32(aim_motion.step.x);
    if (step_x_abs < horizontal_span)
        horizontal_span = horizontal_span + (uint32)div_pow2_trunc(step_x_abs, 1);
    else
        horizontal_span = step_x_abs + (uint32)div_pow2_trunc(horizontal_span, 1);
    angle = fixed_angle_from_vector(div_pow2_trunc(aim_motion.step.y, 11), -abs_s32(div_pow2_trunc(horizontal_span, 11)));
    aim->rotation_x = angle_approach_wrapped(((angle << 2) - 0x500) & 0xffc, 12, aim->rotation_x);
    g_model_draw.target_frame = g_cur_render_frame;
    g_model_draw.depth_bucket = 0x480 - div_pow2_trunc((g_map_render.depth_anchor_z + 0x2000u - object->hierarchy.col.z), 8);
    g_model_draw.world_z = object->hierarchy.col.z;
    if (object->hierarchy.col.flash_clut_ticks != 0)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --object->hierarchy.col.flash_clut_ticks;
    }
    model_render_node(root, &g_model_identity);
    object->nodes[3].translation_z = object->saved_node3_translation_z;
    g_model_draw.clut_override = 0;
    hierarchy_col_box_update(&object->hierarchy.col, &object->nodes[5], 4);
}

/* 0x800AD9E0..0x800ADA10: destroyed map instance flicker. */
/* Original: FUN_800AD9E0. */
static void destroyed_map_instance_flicker_update(void *raw_object)
{
    DESTRUCTIBLE_HIERARCHY *object = (DESTRUCTIBLE_HIERARCHY *)raw_object;
    MAP_MODEL_INSTANCE *instance = object->map_rec;
    instance->light = random_range(0xff);
}

/* Complete resident destruction operation, including loaded SCUBA dispatch */
/* Original: FUN_800ADA10. */
void large_object_destruction_effect_create(COLLISION *object)
{
    SPRITE *effect;
    player_particle_ring_create(object->x, object->y, object->z, 0x0c, 3);
    disp_shake_start();
    speech_random_request(4);
    if (g_session.scuba_active != 0)
    {
        timed_callback_create(scuba_death_particle_emit, object->x, object->y, object->z, 0x28, 2);
        scuba_blast_effect_create(object);
        sound_play_positional(0xa4, 0, 0xc8, object->x, object->y, object->z);
        return;
    }
    expl_flash_create(object->x, (object->y - 0x4000u), (object->z - 0x100u));
    timed_callback_create(expl_debris_particle_create, object->x, object->y, object->z, 0x40, 8);
    timed_callback_create(expl_trail_particle_create, object->x, object->y, object->z, 0x30, 0x10);
    effect = world_spr_create(object->x, object->y, (object->z - 0x2800u), g_expl_impact_animation);
    effect->scale_x = 0x2000;
    sound_play_positional(10, 0, 0x7f, object->x, object->y, object->z);
}

/* Exact 0x800ACB84..0x800ACDA0 shared destructible-object callback. */
/* Original: FUN_800ACB84. */
void destructible_hierarchy_damage(void *raw_object, void *src)
{
    DESTRUCTIBLE_HIERARCHY *object = (DESTRUCTIBLE_HIERARCHY *)raw_object;
    EFFECT *effect;
    MAP_MODEL_GROUP *grp;
    MAP_MODEL_INSTANCE *instance;
    sint32 count;
    object->col.flash_clut_ticks = 1;
    if (object_damage_apply(object, src) == 0)
        return;
    if (object->col.object_type == 0x35)
        --g_mission.counts[3];
    if (object->col.object_type == 0x38)
        --g_mission.counts[4];
    if (object->col.object_type == 0x62)
        --g_mission.counts[8];
    if (object->col.object_type == 0x75)
        --g_mission.counts[11];
    if (object->col.object_type == 0x74)
        --g_mission.counts[23];
    object->col.receives_mask = 0;
    object->col.object.update = destroyed_map_instance_flicker_update;
    large_object_destruction_effect_create(&object->col);
    effect = object->owner_effect;
    effect->vals[0] = 999;
    grp = object->map_rec;
    if (grp == g_map.grps)
        return;
    count = grp->active_count;
    instance = grp + 1;
    while (count > 0)
    {
        sint32 delta = abs_s32((((uint32)instance->y << 8) - object->col.y));
        if (delta < 0x4000)
        {
            XASSERT(instance->model_id < g_model_catalog.count);
            XASSERT(g_model_selections.radar_target < g_model_catalog.count);
            sint32 replacement_y = instance->y + g_model_catalog.descs[instance->model_id].half_y;
            instance->y = replacement_y;
            replacement_y = replacement_y + g_model_catalog.descs[g_model_selections.radar_target].half_y + 2;
            instance->y = replacement_y;
            instance->model_id = g_model_selections.radar_target;
            object->map_rec = instance;
            return;
        }
        instance++;
        --count;
    }
}

/* Original: FUN_800AD038. */
void radar_vehicle_create(EFFECT *effect)
{
    static const sint8 model_offsets[9] = {3, 2, 1, 0, -1, -1, -1, -1, -1};
    RADAR_VEHICLE *object = (RADAR_VEHICLE *)runtime_heap_allocate(sizeof(*object));
    sint32 idx;
    object->hierarchy.col.object.update = radar_vehicle_update;
    linked_list_append(&g_object_list, object);
    object->hierarchy.col.object_type = effect->type;
    effect->type = 0;
    for (idx = 0; idx < 9; idx++)
    {
        MODEL_NODE *node = &object->nodes[idx];
        object->node_table[idx] = node;
        node->model_id = model_offsets[idx] < 0 ? 0 : g_radar_vehicle_model_base + model_offsets[idx];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_radar_initial_pose, object->node_table, 9, 1);
    object->hierarchy.owner_effect = effect;
    object->hierarchy.col.z = effect->z;
    object->hierarchy.col.x = effect->x;
    object->hierarchy.col.y = effect->y + 0x1000u;
    object->hierarchy.col.receives_mask = 1;
    object->hierarchy.col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    object->hierarchy.col.hit_callback = destructible_hierarchy_damage;
    object->hierarchy.col.hit.health = 10;
    object->nodes[0].scale = 0x1000;
    object->saved_node3_translation_z = object->nodes[3].translation_z;
    object->hierarchy.map_rec = map_floor_cell_at(object->hierarchy.col.x, object->hierarchy.col.z)->grp;
}
