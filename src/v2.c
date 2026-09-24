#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include "airship.h"
#include "game_sound.h"
#include "code_module.h"
#include "collision.h"
#include "effect_update.h"
#include "global.h"
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

/* Types. */
/* Semantic native-port module; original address comments are preserved. */
typedef struct
{
    COLLISION collision;           /* +0x000 */
    sint16 health;                 /* +0x078 */
    sint16 flash_clut_ticks;       /* +0x07A */
    MODEL_NODE nodes[6];           /* +0x07C */
    uint8 field_2bc[0xc0];         /* +0x2BC */
    MODEL_NODE *node_table[6];     /* +0x37C */
    uint8 field_394[8];            /* +0x394 */
    EFFECT *owner_effect;          /* +0x39C */
    EFFECT *markers[8];            /* +0x3A0 */
    sint32 velocity_x;             /* +0x3C0 */
    sint32 target_x;               /* +0x3C4 */
    sint32 velocity_y;             /* +0x3C8 */
    sint32 acceleration_y;         /* +0x3CC */
    sint32 lowest_marker_y;        /* +0x3D0 */
    sint16 launch_ticks;           /* +0x3D4 */
    sint16 flight_sound_handle;    /* +0x3D6 */
    sint16 jet_rotation;           /* +0x3D8 */
    sint16 jet_scale;              /* +0x3DA */
    sint16 target_jet_scale;       /* +0x3DC */
    sint16 departure_ticks;        /* +0x3DE */
    sint16 mode;                   /* +0x3E0 */
    uint16 marker_count;           /* +0x3E2 */
    sint16 markers_complete;       /* +0x3E4 */
    sint16 removed;                /* +0x3E6 */
    sint16 secondary_sound_handle; /* +0x3E8 */
    sint16 forced_launch;          /* +0x3EA */
} V2_ACTOR;

typedef struct
{
    COLLISION collision; /* +0x00 */
    uint16 ticks;        /* +0x78 */
    uint16 field_7a;     /* +0x7A */
} V2_COMPLETION_COUNTER;

#if defined(AP_32BIT)
    #define V2_OFFSET_ASSERT(field, offset) typedef char V2Actor_##field##_at_##offset[(offsetof(V2_ACTOR, field) == 0x##offset) ? 1 : -1]
V2_OFFSET_ASSERT(nodes, 07c);
V2_OFFSET_ASSERT(node_table, 37c);
V2_OFFSET_ASSERT(owner_effect, 39c);
V2_OFFSET_ASSERT(markers, 3a0);
V2_OFFSET_ASSERT(velocity_x, 3c0);
V2_OFFSET_ASSERT(lowest_marker_y, 3d0);
V2_OFFSET_ASSERT(mode, 3e0);
V2_OFFSET_ASSERT(forced_launch, 3ea);
typedef char V2Actor_size_3ec[(sizeof(V2_ACTOR) == 0x3ec) ? 1 : -1];
typedef char V2CompletionCounter_size_7c[sizeof(V2_COMPLETION_COUNTER) == 0x7c ? 1 : -1];
typedef char V2CompletionCounter_ticks_at_78[offsetof(V2_COMPLETION_COUNTER, ticks) == 0x78 ? 1 : -1];
    #undef V2_OFFSET_ASSERT
#endif

/* Exact PAL resident type-0x6A pair, 0x800A97EC..0x800A99C4. */

typedef struct
{
    COLLISION collision;  /* +0x00 */
    uint8 field_78[4];    /* +0x78 */
    EFFECT *owner_effect; /* +0x7C */
} V2_RADAR_MARKER;

#if defined(AP_32BIT)
typedef char V2RadarMarker_size_80[sizeof(V2_RADAR_MARKER) == 0x80 ? 1 : -1];
#endif

/* Functions. */
/* Original: V2_800FBBAC. */
static void v2_expl_stage_three_update(SPRITE *o)
{
    sint32 old = o->field_d0, volume;
    void *sound;
    o->collision.x = g_camera_world_x;
    o->collision.y = g_camera_world_y + 0x273a;
    o->collision.z = g_camera_world_z + 0x10000;
    o->field_d0 = old + 1;
    if ((uint16)(old - 64) < 8)
        o->field_a8 = (sint16)(random_range(64) + 96);
    if ((sint16)old == 72)
    {
        o->field_a8 = 128;
        o->field_aa = -2;
    }
    if (o->field_c8 == -1)
        o->field_c8 = (sint16)sound_play_nonpositional(0x2b, -16, 0);
    else
    {
        volume = o->field_a8;
        if (volume < 0)
            volume = 0;
        if (volume >= 128)
            volume = 127;
        {
            sint16 pair[2];
            pair[0] = pair[1] = (sint16)(volume >> 9);
            sound = pair;
            sound_voice_spatial_volume_update(sound, (sint16)o->field_c8);
        }
    }
}

/* Original: V2_800FB8F4. */
static void v2_expl_stage_two_update(SPRITE *o)
{
    SPRITE *p;
    o->collision.x = g_camera_world_x;
    o->collision.y = g_camera_world_y + 0x453a;
    o->collision.z = g_camera_world_z + 0x10000;
    if (o->scale_x >= 0x1d57)
        o->scale_step = 0x20;
    if (o->field_c8 == 0 && o->scale_x >= 0x1d57)
    {
        o->field_c8 = 1;
        p = expl_flash_create(g_camera_world_x, (((o->collision.z - g_camera_world_z) * 96) / 355) + g_camera_world_y - 0x1e00, g_camera_world_z + 0x10000);
        p->animation.next_address = p->animation.start_address = (uint32)win_code_module_address(0x800fbddc);
        p->scale_x = 0x7400;
        p->field_a8 = -128;
        p->field_aa = 4;
        p->frame_callback = v2_expl_stage_three_update;
        p->field_c8 = -1;
        p->field_9e = p->field_9c = p->field_ac = 1;
        p->scale_step = p->rotation_step_x = 0;
        p->lifetime = 0x140;
        --g_objective_counts[5];
    }
}

/* Original: V2_800FBA48. */
static void v2_expl_stage_one_update(SPRITE *o)
{
    SPRITE *p;
    o->collision.x = g_camera_world_x;
    o->collision.y = g_camera_world_y + 0x453a;
    o->collision.z = g_camera_world_z + 0x10000;
    if (o->field_a8 >= 25)
        o->field_aa = -2;
    g_scene_brightness_bias = -(o->field_a8 + 128);
    if (g_scene_brightness_bias > 0)
        g_scene_brightness_bias = 0;
    if (o->scale_x >= 0x2c01)
        o->scale_step = 0x20;
    if (o->field_a8 < -16 && o->field_aa < 0 && o->field_d4 == 0)
    {
        o->field_d4 = 1;
        p = expl_flash_create(g_camera_world_x, g_camera_world_y + 0x453a, g_camera_world_z + 0x10000);
        p->field_9e = p->field_9c = 1;
        p->animation.next_address = p->animation.start_address = (uint32)win_code_module_address(0x800fbdc8);
        p->field_ac = 0x47f;
        p->scale_x = 0x10;
        p->field_a8 = 0x80;
        p->field_aa = -2;
        p->scale_step = 0xaa;
        p->rotation_step_x = 0;
        p->frame_callback = v2_expl_stage_two_update;
        sound_play_nonpositional(0x30, 0, 0x7f);
    }
}

/* Original: V2_800FBD88. */
static void v2_completion_counter_update(V2_COMPLETION_COUNTER *counter)
{
    ++counter->ticks;
}

/* Original: V2_800FBCE0. */
void v2_expl_sequence_begin(void)
{
    SPRITE *p;
    object_create(sizeof(V2_COMPLETION_COUNTER), (FUNC_COLLISION_UPDATE)v2_completion_counter_update);
    p = expl_flash_create(g_camera_world_x, g_camera_world_y + 0x453a, g_camera_world_z + 0x10000);
    p->field_9c = p->field_9e = 1;
    p->rotation_step_x = 2;
    p->animation.next_address = p->animation.start_address = (uint32)win_code_module_address(0x800fbdb4);
    p->field_ac = 0x47f;
    p->field_a8 = -127;
    p->field_aa = 12;
    p->scale_step = 0x400;
    p->frame_callback = v2_expl_stage_one_update;
    speech_random_request(12);
}

/* Original: V2_800FAF94. */
static void v2_rocket_update(V2_ACTOR *o)
{
    MODEL_NODE *root = &o->nodes[0], *jet = &o->nodes[5];
    SOUND_VOLUME_PAIR *volume;
    SpuVoiceAttr *voice;
    SPRITE *p;
    sint32 step, q, n, mode = o->mode;
    if (o->velocity_y >= -255 && mode != 3)
        o->velocity_y += o->acceleration_y;
    o->collision.y += o->velocity_y;
    if (o->launch_ticks == 0 && player_inside_xz_box(o->collision.x, o->collision.z, 0x19000, 0x19000))
    {
        if (o->forced_launch)
            goto forced;
        o->launch_ticks = 1;
        o->velocity_x = -128;
        sound_play_positional(9, -40, 200, o->collision.x, o->collision.y, o->collision.z);
        speech_random_request(6);
        o->secondary_sound_handle = (sint16)sound_play_nonpositional(0x31, 2, 0x41);
    }
    if (o->forced_launch)
    {
    forced:
        if (o->launch_ticks == 0 && g_primary_objective_states[1] == 2)
        {
            o->launch_ticks = 1;
            o->velocity_x = -128;
            sound_play_positional(9, -40, 200, o->collision.x, o->collision.y, o->collision.z);
            o->secondary_sound_handle = (sint16)sound_play_nonpositional(0x31, 2, 0x41);
        }
    }
    if (o->launch_ticks)
    {
        step = 4;
        if (o->departure_ticks && o->velocity_y > 0)
        {
            step = 32;
            o->target_x = g_camera_world_x;
        }
        if (o->collision.x < o->target_x)
            o->velocity_x += step;
        if (o->target_x < o->collision.x)
            o->velocity_x -= step;
        o->collision.x += o->velocity_x;
        if (o->launch_ticks == 384)
            o->acceleration_y = -5;
        if (o->flight_sound_handle == -1)
            o->flight_sound_handle = sound_play_positional(0x2b, 0, 0x7f, o->collision.x, o->collision.y, o->collision.z);
        else if ((volume = sound_spatial_volume_calculate(0x7f, o->collision.x, o->collision.y, o->collision.z)) != 0)
        {
            voice = sound_voice_spatial_volume_update(volume, o->flight_sound_handle);
            if (voice != 0 && voice->pitch >= 0x101 && o->launch_ticks >= 0x101)
            {
                voice->mask = SPU_VOICE_PITCH;
                --voice->pitch;
                sound_voice_attributes_apply(voice);
            }
        }
    }
    mode = o->mode;
    root->translation_x = o->collision.x - g_camera_world_x;
    root->translation_y = o->collision.y - g_camera_world_y;
    root->translation_z = o->collision.z - g_camera_world_z;
    g_model_render_frame = g_current_render_frame;
    root->scale = (sint16)(0x1000 / mode);
    q = (g_render_row_world_z + 0x2000 - o->collision.z) / 0x100;
    g_render_depth_bucket = 0x480 - q;
    if (mode == 3)
        g_render_depth_bucket += 0xc0;
    g_current_model_world_z = o->collision.z;
    if (o->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --o->flash_clut_ticks;
    }
    model_render_node(root, (MATRIX *)player_assets_executable_address(0x800c8decu));
    g_model_clut_override = 0;
    hierarchy_collision_box_update(o, &o->nodes[1], 4);
    if (o->launch_ticks)
    {
        if ((g_frame_counter & 7) == 0)
        {
            p = world_sprite_create(jet->world_x, jet->world_y, jet->world_z, (const sint32 *)player_assets_executable_address(0x800c8bc8u));
            p->velocity_y = 0x600;
            p->scale_step = (sint16)(0x1000 / mode);
            p->velocity_x = random_range(0x200) - 0x100;
            p->field_a8 = 0x7f;
            p->field_aa = -4;
            p->field_ac = (sint16)g_render_depth_bucket;
        }
        if (o->launch_ticks == 1)
            expl_flash_create(jet->world_x, jet->world_y, jet->world_z);
        if (o->jet_scale < o->target_jet_scale)
            o->jet_scale += 0x80;
        if (o->target_jet_scale < o->jet_scale)
            o->jet_scale -= 0x80;
        q = o->jet_scale + ((g_frame_counter & 2) >> 1) * (o->jet_scale / 4);
        step = o->launch_ticks < 65 ? -8 : -24;
        if (o->launch_ticks >= 65)
            o->target_jet_scale = 0x2000;
        POLY_FT4 *prim = (POLY_FT4 *)(intptr)g_transient_prim_cursor;
        SetPolyFT4(prim);
        SetSemiTrans(prim, 1);
        g_next_frame_object->force_light = 1;
        g_next_frame_object->light_delta = (sint16)step;
        g_render_depth_bucket -= 0x20;
        if (o->launch_ticks >= 181)
            o->jet_rotation += (sint16)(g_frame_counter & 1);
        render_world_sprite(0x26400, jet->world_x, jet->world_y, jet->world_z, 0, q / mode, q / mode, prim, g_render_depth_bucket, 0, 0, o->jet_rotation);
        g_transient_prim_cursor += (sint32)sizeof(*prim);
        root->rotation_y += 8;
        ++o->launch_ticks;
        if (o->departure_ticks)
        {
            if (mode == 3)
                root->rotation_z += 16;
            else if (o->collision.y < o->lowest_marker_y)
            {
                root->rotation_y += 24;
                root->rotation_z += 3;
            }
            ++o->departure_ticks;
            if ((o->collision.z - g_camera_world_z) * 64 / 355 + g_camera_world_y < o->collision.y && mode == 3)
            {
                object_destroy(o);
                sound_voice_stop(&o->flight_sound_handle);
                sound_voice_stop(&o->secondary_sound_handle);
                v2_expl_sequence_begin();
            }
            if (o->collision.y < -250 * (o->collision.z - g_camera_world_z) / 355 + g_camera_world_y && mode != 3)
            {
                o->velocity_y = 0x300;
                o->mode = 3;
                o->collision.z += 0x8000;
            }
        }
        root->rotation_y &= 0xfff;
        root->rotation_z &= 0xfff;
    }
    if (o->departure_ticks == 0)
    {
        if (o->removed == 0 && o->collision.y < o->lowest_marker_y - 0x20000)
        {
            o->removed = 1;
            sound_voice_stop(&o->flight_sound_handle);
            sound_voice_stop(&o->secondary_sound_handle);
            object_destroy(o);
            if (!g_debug_cheats_enabled)
                player_death_begin();
        }
        if (o->removed == 0 && o->forced_launch == 0)
        {
            for (n = 0; n < o->marker_count; n++)
                if (o->markers[n]->values[0] != 999)
                    break;
            if (n == o->marker_count)
            {
                if (o->markers_complete == 0)
                {
                    o->markers_complete = 1;
                    o->velocity_y = -0x400;
                }
                o->acceleration_y = 0;
                o->departure_ticks = 1;
            }
        }
    }
}

/* Original: V2_800FADD4. */
void v2_rocket_create(EFFECT *e)
{
    static const sint32 models[6] = {0, -1, -1, -1, -1, -1};
    V2_ACTOR *o = (V2_ACTOR *)runtime_heap_allocate(0x3ec);
    EFFECT *m = 0;
    sint32 n;
    o->collision.update = (FUNC_COLLISION_UPDATE)v2_rocket_update;
    linked_list_append(g_object_list, o);
    o->owner_effect = e;
    for (n = 0; n < 6; n++)
    {
        MODEL_NODE *node = &o->nodes[n];
        o->node_table[n] = node;
        node->model_id = models[n] < 0 ? 0 : g_v2_rocket_model_base + models[n];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(g_v2_rocket_initial_pose, o->node_table, 1);
    o->target_x = o->collision.x = e->x;
    o->collision.y = e->y - 0x4a00;
    o->collision.z = e->z;
    e->type = 0;
    o->flight_sound_handle = -1;
    o->target_jet_scale = 0x5000;
    o->jet_scale = 0x4000;
    o->mode = 1;
    while ((m = effect_find_next(0x38, m)) != 0)
    {
        if (m->values[0] == e->values[0])
        {
            o->markers[o->marker_count] = m;
            if (m->y < o->lowest_marker_y)
                o->lowest_marker_y = m->y;
            ++o->marker_count;
        }
        ++m;
    }
    if (g_stage_index == 14)
        o->forced_launch = 1;
#ifdef XPORT_NATIVE
    if (g_stage_index == 25 && getenv("OA_STAGE25_TRACE"))
    {
        FILE *file = fopen("../status/v2-jungle-native.log", "w");
        if (file)
        {
            fprintf(file, "V2_JUNGLE_NATIVE_CTOR xyz=%d,%d,%d health=%d markers=%d mode=%d update=800faf94\n", o->collision.x, o->collision.y, o->collision.z, o->health, o->marker_count, o->mode);
            fclose(file);
        }
    }
#endif
}

/* Original: FUN_800A9884. */
static void v2_radar_marker_update(V2_RADAR_MARKER *o)
{
    sint32 dx, dz, q;
    if (!world_object_is_visible(o->collision.x, o->collision.y, o->collision.z, 0x8000))
    {
        o->owner_effect->type = 0x6a;
        object_destroy(o);
        return;
    }
    dx = o->collision.x - g_camera_world_x;
    dz = o->collision.z - g_camera_world_z;
    q = (dx * 320) / dz;
    if (q < 0)
        q = -q;
    g_next_frame_object->light_delta = (sint16)(-80 - q / 2);
    render_world_sprite(0x8800, o->collision.x, o->collision.y, o->collision.z, 0, 0x1000, 0x1000, (POLY_FT4 *)o->collision.prim, 0, 2, 0, ((dx < 0 ? dx + 0xff : dx) >> 8) & 0x3ff);
}

/* Original: FUN_800A97EC. */
void v2_radar_marker_create(EFFECT *e)
{
    V2_RADAR_MARKER *o = (V2_RADAR_MARKER *)object_create(sizeof(*o), (FUNC_COLLISION_UPDATE)v2_radar_marker_update);
    e->type = 0;
    setPolyFT4(o->collision.prim);
    setShadeTex(o->collision.prim, 0);
    o->collision.prim[7] |= 2;
    o->owner_effect = e;
    o->collision.x = e->x;
    o->collision.y = e->y;
    o->collision.z = e->z;
}
