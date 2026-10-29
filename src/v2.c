#include <stdio.h>
#include <stdlib.h>

#include "game_math.h"
#include "input.h"
#include "game.h"
#include "airship.h"
#include "render.h"
#include "spatial.h"
#include "game_sound.h"
#include "collision.h"
#include "effect_update.h"
#include "map_render.h"
#include "camera.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "psx.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "sprite_renderer.h"
#include "v2.h"

/* Original: SLES_004.74:DAT_800D3E60. */
MODEL_POSE_RESOURCE g_v2_rocket_init_pose;

/* Original: SLES_004.74:DAT_800D3E54. */
sint16 g_v2_rocket_model_base;

/* Original data: COMMON0/V2.BIN:0x800FBDB4..0x800FBDC7 */
static const ANIM_COMMAND expl_animation0[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {3840, 166912, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/V2.BIN:0x800FBDC8..0x800FBDDB */
static const ANIM_COMMAND expl_animation1[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {128, 166913, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/V2.BIN:0x800FBDDC..0x800FBE07 */
static const ANIM_COMMAND expl_animation2[6] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {3, 166914, 0}, 0}, {ANIM_STEP_FRAME, {3, 166915, 0}, 0}, {ANIM_STEP_FRAME, {3, 166916, 0}, 0}, {ANIM_STEP_FRAME, {3, 166917, 0}, 0}, {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Types. */
/* Semantic native-port module; original address comments are preserved. */
typedef struct
{
    COLLISION col;
    MODEL_NODE nodes[6];
    MODEL_NODE *node_table[6];
    EFFECT *owner_effect;
    EFFECT *markers[8];
    sint32 velocity_x;
    sint32 target_x;
    sint32 velocity_y;
    sint32 acceleration_y;
    sint32 lowest_marker_y;
    sint16 launch_ticks;
    sint16 flight_sound_handle;
    sint16 jet_rotation;
    sint16 jet_scale;
    sint16 target_jet_scale;
    sint16 departure_ticks;
    sint16 mode;
    sint16 marker_count;
    sint16 markers_complete;
    sint16 removed;
    sint16 launch_sound_handle;
    sint16 forced_launch;
} V2_ACTOR;

typedef struct
{
    COLLISION col;
    uint16 ticks;
} V2_COMPLETION_COUNTER;

/* Exact PAL resident type-0x6A pair, 0x800A97EC..0x800A99C4. */

typedef struct
{
    COLLISION col;
    EFFECT *owner_effect;
} V2_RADAR_MARKER;

/* Functions. */
/* Original: V2:V2_800FBBAC. */
static void v2_expl_phase2_update(void *raw_spr)
{
    SPRITE *spr = raw_spr;
    sint32 prev_ticks = spr->behavior.explosion.ticks, volume;
    sint16 phase_ticks = prev_ticks;
    sint32 randomizing = phase_ticks >= 64 && phase_ticks < 72;
    spr->col.x = g_camera.world_x;
    spr->col.y = g_camera.world_y + 0x273au;
    spr->col.z = g_camera.world_z + 0x10000u;
    spr->behavior.explosion.ticks = prev_ticks + 1u;
    if (randomizing)
        spr->light_delta = random_range(64) + 96;
    if (phase_ticks == 72)
    {
        spr->light_delta = 128;
        spr->light_step = -2;
    }
    if (spr->behavior.explosion.sound_handle == -1)
        spr->behavior.explosion.sound_handle = sound_play_nonpositional(0x2b, -16, 0);
    else
    {
        volume = randomizing ? 127 : spr->light_delta;
        if (volume < 0)
            volume = 0;
        if (volume >= 128)
            volume = 127;
        SOUND_VOLUME_PAIR volume_pair;
        volume_pair.volume_left = volume_pair.volume_right = volume << 7;
        sound_voice_spatial_volume_update(&volume_pair, spr->behavior.explosion.sound_handle);
    }
}

/* Original: V2:V2_800FB8F4. */
static void v2_expl_phase1_update(void *raw_spr)
{
    SPRITE *spr = raw_spr;
    SPRITE *next_spr;
    spr->col.x = g_camera.world_x;
    spr->col.y = g_camera.world_y + 0x453au;
    spr->col.z = g_camera.world_z + 0x10000u;
    if (spr->scale_x >= 0x1d57)
        spr->scale_step = 0x20;
    if (spr->behavior.explosion.next_spawned == 0 && spr->scale_x >= 0x1d57)
    {
        spr->behavior.explosion.next_spawned = 1;
        next_spr = expl_flash_create(g_camera.world_x, g_camera.world_y + (0x10000u * 96 / 355) - 0x1e00, g_camera.world_z + 0x10000u);
        next_spr->animation.next = next_spr->animation.start = expl_animation2;
        next_spr->scale_x = 0x7400;
        next_spr->light_delta = -128;
        next_spr->light_step = 4;
        next_spr->frame_callback = v2_expl_phase2_update;
        next_spr->behavior.explosion.sound_handle = -1;
        next_spr->split_mode = next_spr->retain_offscreen = next_spr->ot_bucket = 1;
        next_spr->scale_step = next_spr->rotation_step_x = 0;
        next_spr->lifetime = 0x140;
        --g_mission.counts[5];
    }
}

/* Original: V2:V2_800FBA48. */
static void v2_expl_phase0_update(void *raw_spr)
{
    SPRITE *spr = raw_spr;
    SPRITE *next_spr;
    spr->col.x = g_camera.world_x;
    spr->col.y = g_camera.world_y + 0x453au;
    spr->col.z = g_camera.world_z + 0x10000u;
    if (spr->light_delta >= 25)
        spr->light_step = -2;
    g_map_render.brightness = -(spr->light_delta + 128);
    if (g_map_render.brightness > 0)
        g_map_render.brightness = 0;
    if (spr->scale_x >= 0x2c01)
        spr->scale_step = 0x20;
    if (spr->light_delta < -16 && spr->light_step < 0 && spr->behavior.explosion.next_spawned == 0)
    {
        spr->behavior.explosion.next_spawned = 1;
        next_spr = expl_flash_create(g_camera.world_x, (g_camera.world_y + 0x453au), (g_camera.world_z + 0x10000u));
        next_spr->split_mode = next_spr->retain_offscreen = 1;
        next_spr->animation.next = next_spr->animation.start = expl_animation1;
        next_spr->ot_bucket = 0x47f;
        next_spr->scale_x = 0x10;
        next_spr->light_delta = 0x80;
        next_spr->light_step = -2;
        next_spr->scale_step = 0xaa;
        next_spr->rotation_step_x = 0;
        next_spr->frame_callback = v2_expl_phase1_update;
        sound_play_nonpositional(0x30, 0, 0x7f);
    }
}

/* Original: V2:V2_800FBD88. */
static void v2_completion_counter_update(void *raw_counter)
{
    V2_COMPLETION_COUNTER *counter = raw_counter;
    ++counter->ticks;
}

/* Original: V2:V2_800FBCE0. */
void v2_expl_sequence_begin(void)
{
    SPRITE *spr;
    object_create(sizeof(V2_COMPLETION_COUNTER), v2_completion_counter_update);
    spr = expl_flash_create(g_camera.world_x, (g_camera.world_y + 0x453au), (g_camera.world_z + 0x10000u));
    spr->retain_offscreen = spr->split_mode = 1;
    spr->rotation_step_x = 2;
    spr->animation.next = spr->animation.start = expl_animation0;
    spr->ot_bucket = 0x47f;
    spr->light_delta = -127;
    spr->light_step = 12;
    spr->scale_step = 0x400;
    spr->frame_callback = v2_expl_phase0_update;
    speech_random_request(12);
}

/* Original: V2:V2_800FAF94. */
static void v2_rocket_update(void *raw_actor)
{
    V2_ACTOR *actor = raw_actor;
    MODEL_NODE *root = &actor->nodes[0], *jet = &actor->nodes[5];
    const SOUND_VOLUME_PAIR *volume;
    SpuVoiceAttr *voice;
    SPRITE *particle;
    sint32 depth_offset, marker_idx, mode = actor->mode;
    if (actor->velocity_y >= -255 && mode != 3)
        actor->velocity_y += actor->acceleration_y;
    actor->col.y = (uint32)actor->col.y + actor->velocity_y;
    if (actor->launch_ticks == 0 && player_inside_xz_box(actor->col.x, actor->col.z, 0x19000, 0x19000))
    {
        if (actor->forced_launch)
            goto forced;
        actor->launch_ticks = 1;
        actor->velocity_x = -128;
        sound_play_positional(9, -40, 200, actor->col.x, actor->col.y, actor->col.z);
        speech_random_request(6);
        actor->launch_sound_handle = sound_play_nonpositional(0x31, 2, 0x41);
    }
    if (actor->forced_launch)
    {
    forced:
        if (actor->launch_ticks == 0 && g_mission.primary[1] == 2)
        {
            actor->launch_ticks = 1;
            actor->velocity_x = -128;
            sound_play_positional(9, -40, 200, actor->col.x, actor->col.y, actor->col.z);
            actor->launch_sound_handle = sound_play_nonpositional(0x31, 2, 0x41);
        }
    }
    if (actor->launch_ticks)
    {
        sint32 horizontal_step = 4;
        if (actor->departure_ticks && actor->velocity_y > 0)
        {
            horizontal_step = 32;
            actor->target_x = g_camera.world_x;
        }
        if (actor->col.x < actor->target_x)
            actor->velocity_x += horizontal_step;
        if (actor->target_x < actor->col.x)
            actor->velocity_x -= horizontal_step;
        actor->col.x = (uint32)actor->col.x + actor->velocity_x;
        if (actor->launch_ticks == 384)
            actor->acceleration_y = -5;
        if (actor->flight_sound_handle == -1)
            actor->flight_sound_handle = sound_play_positional(0x2b, 0, 0x7f, actor->col.x, actor->col.y, actor->col.z);
        else if ((volume = sound_spatial_volume_calculate(0x7f, actor->col.x, actor->col.y, actor->col.z)) != 0)
        {
            voice = sound_voice_spatial_volume_update(volume, actor->flight_sound_handle);
            if (voice != 0 && voice->pitch >= 0x101 && actor->launch_ticks >= 0x101)
            {
                voice->mask = SPU_VOICE_PITCH;
                --voice->pitch;
                sound_voice_attributes_apply(voice);
            }
        }
    }
    mode = actor->mode;
    root->translation_x = (uint32)actor->col.x - g_camera.world_x;
    root->translation_y = (uint32)actor->col.y - g_camera.world_y;
    root->translation_z = (uint32)actor->col.z - g_camera.world_z;
    g_model_draw.target_frame = g_cur_render_frame;
    root->scale = 0x1000 / mode;
    depth_offset = div_256_trunc((g_map_render.depth_anchor_z + 0x2000u - actor->col.z));
    g_model_draw.depth_bucket = 0x480 - depth_offset;
    if (mode == 3)
        g_model_draw.depth_bucket += 0xc0;
    g_model_draw.world_z = actor->col.z;
    if (actor->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --actor->col.flash_clut_ticks;
    }
    model_render_node(root, &g_model_identity);
    g_model_draw.clut_override = 0;
    hierarchy_col_box_update(&actor->col, &actor->nodes[1], 4);
    if (actor->launch_ticks)
    {
        if ((g_render_state.frame & 7) == 0)
        {
            particle = world_spr_create(jet->world_x, jet->world_y, jet->world_z, g_particle_animation);
            particle->velocity_y = 0x600;
            particle->scale_step = 0x100 / mode;
            particle->velocity_x = random_range(0x200) - 0x100;
            particle->light_delta = 0x7f;
            particle->light_step = -4;
            particle->ot_bucket = g_model_draw.depth_bucket;
        }
        if (actor->launch_ticks == 1)
            expl_flash_create(jet->world_x, jet->world_y, jet->world_z);
        if (actor->jet_scale < actor->target_jet_scale)
            actor->jet_scale += 0x80;
        if (actor->target_jet_scale < actor->jet_scale)
            actor->jet_scale -= 0x80;
        sint32 jet_scale = actor->jet_scale + ((g_render_state.frame & 2) >> 1) * (actor->jet_scale / 4);
        sint32 light_delta = actor->launch_ticks < 65 ? -8 : -24;
        if (actor->launch_ticks >= 65)
            actor->target_jet_scale = 0x2000;
        POLY_FT4 *poly = g_next_transient_prim;
        XASSERT(!(g_next_transient_prim >= g_transient_prims + TRANSIENT_PRIM_LIMIT));
        SetPolyFT4(poly);
        SetSemiTrans(poly, 1);
        g_next_frame_object->force_light = 1;
        g_next_frame_object->light_delta = light_delta;
        g_model_draw.depth_bucket -= 0x20;
        if (actor->launch_ticks >= 181)
            actor->jet_rotation += (g_render_state.frame & 1);
        render_world_spr(0x26400, jet->world_x, jet->world_y, jet->world_z, 0, jet_scale / mode, jet_scale / mode, poly, g_model_draw.depth_bucket, 0, 0, actor->jet_rotation & 0x3ff);
        ++g_next_transient_prim;
        root->rotation_y += 8;
        ++actor->launch_ticks;
        if (actor->departure_ticks)
        {
            if (mode == 3)
                root->rotation_z += 16;
            else if (actor->col.y < actor->lowest_marker_y)
            {
                root->rotation_y += 24;
                root->rotation_z += 3;
            }
            ++actor->departure_ticks;
            if (((mul_lo(((uint32)actor->col.z - g_camera.world_z), 64) / 355) + g_camera.world_y) < actor->col.y && mode == 3)
            {
                object_destroy(actor);
                sound_voice_stop(&actor->flight_sound_handle);
                sound_voice_stop(&actor->launch_sound_handle);
                v2_expl_sequence_begin();
            }
            if (actor->col.y < ((mul_lo(-250, ((uint32)actor->col.z - g_camera.world_z)) / 355) + g_camera.world_y) && mode != 3)
            {
                actor->velocity_y = 0x300;
                actor->mode = 3;
                actor->col.z = actor->col.z + 0x8000u;
            }
        }
        root->rotation_y &= 0xfff;
        root->rotation_z &= 0xfff;
    }
    if (actor->departure_ticks == 0)
    {
        if (actor->removed == 0 && actor->col.y < (actor->lowest_marker_y - 0x20000))
        {
            actor->removed = 1;
            sound_voice_stop(&actor->flight_sound_handle);
            sound_voice_stop(&actor->launch_sound_handle);
            object_destroy(actor);
            if (!g_debug_cheats_enabled)
                player_death_begin();
        }
        if (actor->removed == 0 && actor->forced_launch == 0)
        {
            for (marker_idx = 0; marker_idx < actor->marker_count; marker_idx++)
                if (actor->markers[marker_idx]->vals[0] != 999)
                    break;
            if (marker_idx == actor->marker_count)
            {
                if (actor->markers_complete == 0)
                {
                    actor->markers_complete = 1;
                    actor->velocity_y = -0x400;
                }
                actor->acceleration_y = 0;
                actor->departure_ticks = 1;
            }
        }
    }
}

/* Original: V2:V2_800FADD4. */
void v2_rocket_create(EFFECT *effect)
{
    static const sint32 models[6] = {0, -1, -1, -1, -1, -1};
    V2_ACTOR *actor = runtime_heap_allocate(sizeof(*actor));
    EFFECT *marker = 0;
    sint32 idx;
    actor->col.object.update = v2_rocket_update;
    linked_list_append(&g_object_list, actor);
    actor->owner_effect = effect;
    for (idx = 0; idx < 6; idx++)
    {
        MODEL_NODE *node = &actor->nodes[idx];
        actor->node_table[idx] = node;
        node->model_id = models[idx] < 0 ? 0 : g_v2_rocket_model_base + models[idx];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_v2_rocket_init_pose, actor->node_table, 6, 1);
    actor->target_x = actor->col.x = effect->x;
    actor->col.y = effect->y - 0x4a00u;
    actor->col.z = effect->z;
    effect->type = 0;
    actor->flight_sound_handle = -1;
    actor->target_jet_scale = 0x5000;
    actor->jet_scale = 0x4000;
    actor->mode = 1;
    while ((marker = effect_find_next(0x38, marker)) != 0)
    {
        if (marker->vals[0] == effect->vals[0])
        {
            XASSERT(!(actor->marker_count >= sizeof(actor->markers) / sizeof(actor->markers[0])));
            actor->markers[actor->marker_count] = marker;
            if (marker->y < actor->lowest_marker_y)
                actor->lowest_marker_y = marker->y;
            ++actor->marker_count;
        }
        ++marker;
    }
    if (g_session.stage == 14)
        actor->forced_launch = 1;
}

/* Original: SLES_004.74:FUN_800A9884. */
static void v2_radar_marker_update(void *raw_marker)
{
    V2_RADAR_MARKER *marker = raw_marker;
    sint32 dx, dz, numerator, proj_x_abs;
    if (!world_object_is_visible(marker->col.x, marker->col.y, marker->col.z, 0x8000))
    {
        marker->owner_effect->type = 0x6a;
        object_destroy(marker);
        return;
    }
    dx = (uint32)marker->col.x - g_camera.world_x;
    dz = (uint32)marker->col.z - g_camera.world_z;
    numerator = dx * 320u;
    if (dz == 0)
    {
#if defined(_DEBUG)
        fputs("RADAR PROJECTION BREAK 7\n", stderr);
#endif
        abort();
    }
    if (dz == -1 && numerator == (-2147483647 - 1))
    {
#if defined(_DEBUG)
        fputs("RADAR PROJECTION BREAK 6\n", stderr);
#endif
        abort();
    }
    proj_x_abs = abs_s32(numerator / dz);
    g_next_frame_object->light_delta = -80 - proj_x_abs / 2;
    render_world_spr(0x8800, marker->col.x, marker->col.y, marker->col.z, 0, 0x1000, 0x1000, &marker->col.prim, 0, 2, 0, div_256_trunc(dx) & 0x3ff);
}

/* Original: SLES_004.74:FUN_800A97EC. */
void v2_radar_marker_create(EFFECT *effect)
{
    V2_RADAR_MARKER *marker;
    effect->type = 0;
    marker = object_create(sizeof(*marker), v2_radar_marker_update);
    setPolyFT4(&marker->col.prim);
    setShadeTex(&marker->col.prim, 0);
    marker->col.prim.rgbc.code |= 2;
    marker->owner_effect = effect;
    marker->col.x = effect->x;
    marker->col.y = effect->y;
    marker->col.z = effect->z;
}
