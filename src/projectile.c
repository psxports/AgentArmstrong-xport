#include <inttypes.h>
#include <stddef.h>
#include <stdlib.h>

#include "game_math.h"
#include "animation.h"
#include "spatial.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "map_render.h"
#include "render.h"
#include "input.h"
#include "level_data.h"
#include "map.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "psx.h"
#include "random.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "sprite_renderer.h"

/* Original: DAT_800D40BE, GP + 0x436 */
sint16 g_surface_impact_cooldown;

/* Original: DAT_800D3FEC, persistent projectile wind publication */
static sint32 projectile_wind_x;

/* Original data: SLES_004.74:0x800CAF10 */
static const ANIM_COMMAND enemy_bullet_animation[4] = {
    {ANIM_STEP_FRAME, {200, 157696, 0}, 0},
    {ANIM_LOOP, {0, 0, 0}, 0},
    {ANIM_STEP_FRAME, {200, 157696, 0}, 0},
    {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8B20..0x800C8B5B */
const ANIM_COMMAND g_bullet_impact_animation[8] = {
    {ANIM_STEP_FRAME, {3, 17420, 0}, 0}, {ANIM_STEP_FRAME, {3, 17421, 0}, 0}, {ANIM_STEP_FRAME, {3, 17422, 0}, 0}, {ANIM_STEP_FRAME, {3, 17423, 0}, 0}, {ANIM_STEP_FRAME, {3, 17424, 0}, 0}, {ANIM_STEP_FRAME, {3, 17425, 0}, 0}, {ANIM_STEP_FRAME, {3, 17426, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8B5C..0x800C8BC7 */
const ANIM_COMMAND g_expl_impact_animation[14] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {6, 17408, 0}, 0}, {ANIM_STEP_FRAME, {5, 17409, 0}, 0}, {ANIM_STEP_FRAME, {4, 17410, 0}, 0}, {ANIM_STEP_FRAME, {3, 17411, 0}, 0}, {ANIM_STEP_FRAME, {3, 17412, 0}, 0}, {ANIM_STEP_FRAME, {3, 17413, 0}, 0}, {ANIM_STEP_FRAME, {3, 17414, 0}, 0}, {ANIM_STEP_FRAME, {3, 17415, 0}, 0}, {ANIM_STEP_FRAME, {3, 17416, 0}, 0}, {ANIM_STEP_FRAME, {4, 17417, 0}, 0}, {ANIM_STEP_FRAME, {4, 17418, 0}, 0}, {ANIM_STEP_FRAME, {4, 17419, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8CC0..0x800C8D3B */
const ANIM_COMMAND g_surface_impact_animation[16] = {
    {ANIM_STEP_FRAME, {4, 129024, 0}, 0}, {ANIM_STEP_FRAME, {4, 129025, 0}, 0}, {ANIM_STEP_FRAME, {4, 129026, 0}, 0}, {ANIM_STEP_FRAME, {4, 129027, 0}, 0}, {ANIM_STEP_FRAME, {4, 129028, 0}, 0}, {ANIM_STEP_FRAME, {4, 129029, 0}, 0}, {ANIM_STEP_FRAME, {4, 129030, 0}, 0}, {ANIM_STEP_FRAME, {4, 129031, 0}, 0}, {ANIM_STEP_FRAME, {4, 129032, 0}, 0}, {ANIM_STEP_FRAME, {4, 129033, 0}, 0}, {ANIM_STEP_FRAME, {4, 129034, 0}, 0}, {ANIM_STEP_FRAME, {4, 129035, 0}, 0}, {ANIM_STEP_FRAME, {4, 129036, 0}, 0}, {ANIM_STEP_FRAME, {4, 129037, 0}, 0}, {ANIM_STEP_FRAME, {4, 129038, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8D3C..0x800C8DB7 */
const ANIM_COMMAND g_surface_impact_slow_animation[16] = {
    {ANIM_STEP_FRAME, {5, 129024, 0}, 0}, {ANIM_STEP_FRAME, {5, 129025, 0}, 0}, {ANIM_STEP_FRAME, {5, 129026, 0}, 0}, {ANIM_STEP_FRAME, {5, 129027, 0}, 0}, {ANIM_STEP_FRAME, {5, 129028, 0}, 0}, {ANIM_STEP_FRAME, {5, 129029, 0}, 0}, {ANIM_STEP_FRAME, {5, 129030, 0}, 0}, {ANIM_STEP_FRAME, {5, 129031, 0}, 0}, {ANIM_STEP_FRAME, {5, 129032, 0}, 0}, {ANIM_STEP_FRAME, {5, 129033, 0}, 0}, {ANIM_STEP_FRAME, {5, 129034, 0}, 0}, {ANIM_STEP_FRAME, {5, 129035, 0}, 0}, {ANIM_STEP_FRAME, {5, 129036, 0}, 0}, {ANIM_STEP_FRAME, {5, 129037, 0}, 0}, {ANIM_STEP_FRAME, {5, 129038, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8DB8..0x800C8DEB */
const ANIM_COMMAND g_small_surface_impact_animation[7] = {
    {ANIM_STEP_FRAME, {4, 129039, 0}, 0}, {ANIM_STEP_FRAME, {4, 129040, 0}, 0}, {ANIM_STEP_FRAME, {4, 129041, 0}, 0}, {ANIM_STEP_FRAME, {4, 129042, 0}, 0}, {ANIM_STEP_FRAME, {4, 129043, 0}, 0}, {ANIM_STEP_FRAME, {4, 129044, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8EE0..0x800C8F4B */
const ANIM_COMMAND g_dust_animation0[14] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {3, 167936, 0}, 0}, {ANIM_STEP_FRAME, {3, 167937, 0}, 0}, {ANIM_STEP_FRAME, {3, 167938, 0}, 0}, {ANIM_STEP_FRAME, {3, 167939, 0}, 0}, {ANIM_STEP_FRAME, {3, 167940, 0}, 0}, {ANIM_STEP_FRAME, {3, 167941, 0}, 0}, {ANIM_STEP_FRAME, {3, 167942, 0}, 0}, {ANIM_STEP_FRAME, {3, 167943, 0}, 0}, {ANIM_STEP_FRAME, {3, 167944, 0}, 0}, {ANIM_STEP_FRAME, {3, 167945, 0}, 0}, {ANIM_STEP_FRAME, {3, 167946, 0}, 0}, {ANIM_STEP_FRAME, {3, 167947, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8F4C..0x800C8F87 */
const ANIM_COMMAND g_dust_animation1[8] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {3, 167948, 0}, 0}, {ANIM_STEP_FRAME, {3, 167949, 0}, 0}, {ANIM_STEP_FRAME, {3, 167950, 0}, 0}, {ANIM_STEP_FRAME, {3, 167951, 0}, 0}, {ANIM_STEP_FRAME, {3, 167952, 0}, 0}, {ANIM_STEP_FRAME, {3, 167953, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C932C..0x800C933F */
const ANIM_COMMAND g_spark_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {30, 156672, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: SLES_004.74:0x800C8CB4..0x800C8CBD */
static const sint16 impact_sounds[5] = {11, 12, 13, 14, 15};

/* Original: SLES_004.74:FUN_80090F5C. */
static void surface_impact_emit(sint32 x, sint32 y, sint32 z)
{
    sint32 sound = impact_sounds[random_range(4)];
    if (g_surface_impact_cooldown == 0)
    {
        sound_play_positional(sound, random_range(8) - 4, 0x6e, x, y, z);
        g_surface_impact_cooldown = 12;
    }
    if (random_range(4) == 0)
    {
        SPRITE *spark = world_spr_create(x, (y - 0x1000u), (z - 0x100u), g_spark_animation);
        spark->light_delta = 0x80;
        spark->light_step = -0x10;
        spark->rotation_step_x = 0x10;
        spark->scale_step = (random_range(0x80) + 0x80);
        spark->force_light = 1;
        spark->scale_x = 1;
    }
}

/* Functions. */
/* Semantic module generated by tools/merge_semantic_modules.py.
 * Original MIPS address comments remain attached to each function. */
/* Original: FUN_800A640C. */
static sint32 projectile_update(void *raw_object)
{
    PROJECTILE *object = (PROJECTILE *)raw_object;
    sint32 old_x = object->col.x;
    sint32 old_y = object->col.y;
    sint32 old_z = object->col.z;
    sint32 floor_y;
    sint16 col;
    fixed_dda_advance(&object->motion);
    if (object->wind_enabled != 0)
        object->motion.pos.x += projectile_wind_x << 8;
    object->col.x = div_256_trunc(object->motion.pos.x);
    object->col.y = div_256_trunc(object->motion.pos.y);
    object->col.z = div_256_trunc(object->motion.pos.z);
    if (object->probe_mode == 0)
    {
        if (!world_object_is_visible(object->col.x, object->col.y, object->col.z, 0xc800))
            object_destroy(object);
        animation_update(&object->animation);
        g_next_frame_object->force_light = 1;
        render_world_spr(object->animation.frame, object->col.x, object->col.y, object->col.z, 0, object->scale, object->scale, &object->col.prim, 0, 0, 0, 0);
    }
    floor_y = map_floor_height_at(object->col.x, object->col.y, object->col.z);
    col = swept_col_test(old_x, old_y, old_z, &object->col.x, &object->col.y, &object->col.z);
    if (col == 0 && object->col.y <= floor_y)
    {
        FUNC_OBJECT_UPDATE callback = object->floor_callback;
        if (callback != 0)
            callback(object);
        return 0;
    }
    {
        FUNC_OBJECT_UPDATE callback = object->impact_callback;
        if (callback == 0)
        {
            if (object->probe_mode == 0)
            {
                SPRITE *impact;
                object_destroy(object);
                impact = world_spr_create(object->col.x, object->col.y, object->col.z, g_bullet_impact_animation);
                impact->scale_x = 0x1800;
                surface_impact_emit(object->col.x, object->col.y, object->col.z);
            }
        }
        else
        {
            callback(object);
            object_destroy(object);
        }
    }
    return 1;
}

/* Original: FUN_800A62AC. */
PROJECTILE *projectile_create(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1)
{
    PROJECTILE *object = (PROJECTILE *)runtime_heap_allocate(sizeof(*object));
    POLY_FT4 *poly = &object->col.prim;
    object->col.object.update_kind = OBJECT_UPDATE_STATUS;
    object->col.object.status_update = projectile_update;
    linked_list_append(&g_object_list, object);
    fixed_dda_initialize(x0, y0, z0, x1, y1, z1, &object->motion);
    object->motion.step.x = (object->motion.step.x * 5u);
    object->motion.step.y = (object->motion.step.y * 5u);
    object->motion.step.z = (object->motion.step.z * 5u);
    setPolyFT4(poly);
    setShadeTex(poly, 0);
    setSemiTrans(poly, 1);
    object->col.x = x0;
    object->col.z = z0;
    object->col.y = y0;
    col_box_set(object, 2, 2, 2);
    object->col.receives_mask = 2;
    object->col.player_contact_callback = player_apply_damage;
    animation_start(&object->animation, enemy_bullet_animation);
    object->col.object_type = 0x18;
    object->scale = 0x1000;
    return object;
}

/* Original: FUN_800A617C. */
sint32 projectile_test_path(sint32 x0, sint32 y0, sint32 z0, sint32 x1, sint32 y1, sint32 z1, sint32 steps)
{
    PROJECTILE *probe = projectile_create(x0, y0, z0, x1, y1, z1);
    probe->probe_mode = 1;
    while (steps > 0)
    {
        if ((x1 - 0x500) < probe->col.x && probe->col.x < (x1 + 0x500) && (y1 - 0x500) < probe->col.y && probe->col.y < (y1 + 0x500) && (z1 - 0x500) < probe->col.z && probe->col.z < (z1 + 0x500))
            break;
        if (projectile_update(probe) != 0)
        {
            object_destroy(probe);
            return 1;
        }
        --steps;
    }
    object_destroy(probe);
    return 0;
}

/* Original: SLES_004.74:FUN_8009080C. */
static void world_spr_destroy(SPRITE *object)
{
    if (object->owns_resource != 0)
    {
        void *resource = object->col.frame_desc;
        spr_vram_defer_release(resource);
        runtime_heap_defer_free(resource);
    }
    object_destroy(object);
}

/* Original: FUN_80091058. */
SPRITE *surface_impact_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation)
{
    VRAM_SPRITE *desc = (VRAM_SPRITE *)runtime_heap_allocate(sizeof(*desc));
    SPRITE *effect = world_spr_create(x, y, z, animation);
    effect->owns_resource = 1;
    if (animation == g_small_surface_impact_animation)
        spr_vram_allocate(desc, 0x20, 0x30, 0);
    else
        spr_vram_allocate(desc, 0x40, 0x68, 0);
    effect->col.frame_desc = desc;
    /* 0x800D76D4 is not an independent scalar.  In the original RAM layout
     * it is DAT_800D75D8[(0x1F800 >> 10)], the shared CLUT slot for the
     * 0x1F800 surface-impact/splash frames. */
    desc->clut = g_spr_pal.cluts[0x1f800u >> 10];
    effect->light_delta = -0x40;
    effect->split_mode = 2;
    return effect;
}

/* Original: FUN_80091124. */
static void player_projectile_impact_update(void *raw_object, sint32 old_x, sint32 old_y, sint32 old_z)
{
    SPRITE *object = (SPRITE *)raw_object;
    SPRITE *effect;
    sint32 floor_y;
    sint16 col;
    sint32 sound;

    g_col_surface_resource = -2;
    floor_y = map_floor_height_at(object->col.x, object->col.y, object->col.z);
    col = swept_col_test(old_x, old_y, old_z, &object->col.x, &object->col.y, &object->col.z);
    if (col == 0 && object->col.hit.damage != 0 && floor_y >= object->col.y)
        return;

    object->lifetime = 1;
    if (object->behavior.projectile.special != 0)
    {
        effect = world_spr_create(object->col.x, object->col.y, object->col.z, g_particle_animation);
        effect->cull_distance = 0x3e800;
        effect->scale_x = (random_range(0x800) + 0x2000);
        col_box_set(effect, 0xa0, 0x60, 0x80);
        effect->col.receives_mask = 4;
        effect->col.sends_mask = 1;
        effect->col.object_type = 2;
        effect->col.hit.damage = 1;
        effect->force_light = 1;
        effect->col.box_y = (effect->col.box_y + 0x1000u);
        sound_play_positional(9, 0, 0x7f, object->col.x, object->col.y, object->col.z);
        player_particle_ring_create(object->col.x, object->col.y, object->col.z, 4, 2);
        return;
    }
    else if (floor_y < object->col.y && (g_col_surface_resource == g_stage_env->map_resource || g_col_surface_resource == -1))
    {
        effect = surface_impact_create(object->col.x, object->col.y, object->col.z, g_small_surface_impact_animation);
        sound = random_range(2) + 0x21;
        sound_play_positional(sound, random_range(3), 0x80, object->col.x, object->col.y, object->col.z);
    }
    else
    {
        effect = world_spr_create(object->col.x, object->col.y, object->col.z, g_bullet_impact_animation);
        surface_impact_emit(object->col.x, object->col.y, object->col.z);
        object->col.object_type = 0x40;
        object->lifetime = 0x1e;
        object->light_step = -0x18;
        object->light_delta = 0x80;
        if (g_col_flags & 2)
        {
            object->velocity_x = (sint32) - (uint32)object->velocity_x / 2;
            object->velocity_z /= 2;
        }
        if (g_col_flags & 4)
        {
            object->velocity_z = (sint32) - (uint32)object->velocity_z / 2;
            object->velocity_x /= 2;
        }
        if (g_col_flags & 1)
            object->velocity_y = (sint32) - (uint32)object->velocity_y / 2;
        else
        {
            object->acceleration_y = random_range(0x100);
            object->velocity_y = 0x400 - random_range(0x800);
        }
    }
    effect->scale_x = 0x1800;
    effect->force_light = 1;
    if (object->col.hit.damage == 0)
        world_spr_destroy(object);
}

/* Original: FUN_80090860. */
static void world_spr_update(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint32 old_x = object->col.x;
    uint32 old_y = object->col.y;
    sint32 old_z = object->col.z;
    sint32 lifetime;
    sint32 frame;
    VRAM_SPRITE *desc = 0;
    FUNC_OBJECT_UPDATE callback;
    FUNC_OBJECT_RESULT_UPDATE object_callback;

    if (object->scale_y == 0)
        object->scale_y = object->scale_x;

    object->col.x = (uint32)object->col.x + object->velocity_x;
    object->col.y = (uint32)object->col.y + object->velocity_y;
    object->col.z = (uint32)object->col.z + object->velocity_z;
    if (object->render_kind == 3 && object->col.z < (g_camera.world_z + 0x8000))
    {
        world_spr_destroy(object);
        return;
    }
    if (object->owns_resource != 0)
        desc = object->col.frame_desc;

    lifetime = object->lifetime;
    if (lifetime != 0)
    {
        if (lifetime == 0x1e && object->col.object_type == 0x40)
        {
            object->col.receives_mask = 0;
            object->col.sends_mask = 0;
        }
        object->lifetime = (lifetime - 1);
        if (lifetime == 1)
        {
            world_spr_destroy(object);
            return;
        }
    }
    if (object->col.object_type == 10)
    {
        player_projectile_impact_update(object, old_x, old_y, old_z);
        if (object->col.hit.damage == 0)
            return;
    }
    if (object->render_kind != 2 && object->render_kind != 3)
        object->rotation_x = ((object->rotation_x + object->rotation_step_x) & 0x3ff);
    object->velocity_x += object->acceleration_x;
    object->velocity_y = ((uint32)object->velocity_y + object->acceleration_y);
    object->velocity_z += object->acceleration_z;
    object->scale_x = (object->scale_x + object->scale_step);
    if (object->scale_y != 0)
        object->scale_y = (object->scale_y + object->scale_step);
    if (object->cull_distance == 0)
        object->cull_distance = 0x8000;
    frame = animation_update(&object->animation);
    if ((frame < 0 || frame >= 198657))
    {
        char message[40];
        snprintf(message, sizeof(message), "BAD FRAME%" PRIdPTR, (intptr_t)object->animation.start);
        fatal_error(message);
    }

    callback = object->frame_callback;
    if (callback != 0)
        callback(object);

    g_col_flags = 0;
    callback = object->col_callback_kind == SPRITE_CONTACT_NOTIFY ? object->col_callback : 0;
    object_callback = object->col_callback_kind == SPRITE_CONTACT_OBJECT ? object->col_callback_object : 0;
    if (callback != 0 || object_callback != 0)
    {
        sint32 y_offset = 0;
        MODEL_NODE *model = object->model;
        if (model != 0)
        {
            sint32 desc_idx = model->model_id;
            XASSERT(desc_idx < g_model_catalog.count);
            y_offset = (-g_model_catalog.descs[desc_idx].half_y) << 8;
        }
        object->col.y = (uint32)object->col.y + y_offset;
        if (map_motion_resolve(old_x, (old_y + y_offset), old_z, &object->col.x, &object->col.y, &object->col.z, 0, 0, 0x10, 0x50, 5) != 0)
        {
            if (object_callback != 0)
                object_callback(object);
            else
                callback(object);
        }
        object->col.y = (uint32)object->col.y - y_offset;
    }
    callback = object->impact_callback;
    if (callback != 0 && (g_col_flags & 6) != 0)
        callback(object);

    /* 80090B80..80090BA8: the original writes the current frame record
       unconditionally; frame setup guarantees DAT_800D3F80 here. */
    g_next_frame_object->light_delta = object->light_delta;
    g_next_frame_object->force_light = object->force_light;
    object->light_delta = (object->light_delta + object->light_step);

    if (object->render_kind == 0)
    {
        sint32 visible = world_object_is_visible(object->col.x, object->col.y, object->col.z, object->cull_distance);
        sint32 draw = 1;
        if (!visible || object->col.z < ((g_map.depth * 3 << 14) - (g_map.depth_subcells << 8)))
        {
            if (object->retain_offscreen == 0)
            {
                world_spr_destroy(object);
                return;
            }
            draw = 0;
        }
        if (draw)
        {
            render_world_spr(frame, object->col.x, object->col.y, object->col.z, object->velocity_x, object->scale_x, object->scale_y, &object->col.prim, object->ot_bucket, object->split_mode, desc, object->rotation_x);
            if (object->clut_override != 0)
                object->col.prim.p[0].attr = object->clut_override;
        }
    }
    else if (object->render_kind == 1)
    {
        render_world_spr_immediate(frame, object->col.x, object->col.y, object->col.z, object->velocity_x, object->scale_x, object->scale_y, &object->col.prim, 0, 0, object->rotation_x);
    }
    else if (object->render_kind == 2)
    {
        FRAME_OBJECT *model;
        if (!world_object_is_visible(object->col.x, object->col.y, object->col.z, object->cull_distance) && object->col.z >= ((g_map.depth * 3 << 14) - (g_map.depth_subcells << 8)))
        {
            world_spr_destroy(object);
            return;
        }
        object->rotation_x = ((object->rotation_x + object->rotation_step_x) & 0xfff);
        object->rotation_y = ((object->rotation_y + object->rotation_step_y) & 0xfff);
        object->rotation_z = ((object->rotation_z + object->rotation_step_z) & 0xfff);
        model = frame_model_create(&object->col, object->animation.frame);
        model->model_rot_y = object->rotation_x;
        model->model_rot_x = object->rotation_y;
        model->model_rot_z = object->rotation_z;
    }
    else if (object->render_kind == 3)
    {
        MODEL_NODE *model = object->model;
        sint32 depth_delta;

        model->rotation_y = ((model->rotation_y + object->rotation_step_x) & 0xfff);
        model->rotation_x = ((model->rotation_x + object->rotation_step_y) & 0xfff);
        model->rotation_z = ((model->rotation_z + object->rotation_step_z) & 0xfff);
        model->translation_x = ((uint32)object->col.x - g_camera.world_x);
        model->translation_y = ((uint32)object->col.y - g_camera.world_y);
        model->translation_z = ((uint32)object->col.z - g_camera.world_z);
        g_model_draw.target_frame = g_cur_render_frame;
        depth_delta = ((uint32)g_map_render.depth_anchor_z - object->col.z);
        if (depth_delta < 0)
            depth_delta += 0xff;
        g_model_draw.depth_bucket = 0x480 - (depth_delta >> 8);
        g_model_draw.world_z = object->col.z;
        model_render_node(model, &g_model_identity);
    }

    if (object->animation.next == 0)
    {
        world_spr_destroy(object);
        return;
    }
}

/* Original: FUN_80090758. */
SPRITE *world_spr_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation)
{
    SPRITE *object = (SPRITE *)object_create(sizeof(*object), world_spr_update);
    POLY_FT4 *poly = &object->col.prim;

    SetPolyFT4(poly);
    setShadeTex(poly, 0);
    object->scale_x = 0x1000;
    animation_start(&object->animation, animation);
    object->animation.owner = object;
    object->col.x = x;
    object->col.y = y;
    object->col.z = z;
    object->split_mode = 1;
    return object;
}

/* Original: FUN_80099320. */
SPRITE *expl_flash_create(sint32 x, sint32 y, sint32 z)
{
    SPRITE *effect = world_spr_create(x, y, z, g_spark_animation);
    effect->cull_distance = 0x3e8000;
    effect->light_delta = 0x80;
    effect->light_step = -0x10;
    effect->rotation_step_x = 0x10;
    effect->scale_step = 0x400;
    effect->ot_bucket = 1;
    effect->force_light = 1;
    return effect;
}

/* Direct FUN_80099380 grenade callback. */
static void thrown_explosive_update(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint32 old_x = object->col.x, old_y = object->col.y, old_z = object->col.z;
    sint16 type = object->col.object_type;
    sint32 velocity;
    world_spr_update(object);
    if ((g_input.held & PADRright) == 0)
        object->acceleration_y = 0xa0;
    map_motion_resolve(old_x, old_y, old_z, &object->col.x, &object->col.y, &object->col.z, 0, 0, 0x10, 0x10, 6);
    if (g_col_flags & 8)
        object->velocity_y = -object->velocity_y;
    if (g_col_flags & 4)
    {
        velocity = -object->velocity_z;
        object->velocity_z = (velocity < 0 ? velocity + 3 : velocity) >> 2;
    }
    if (g_col_flags & 2)
    {
        velocity = -object->velocity_x;
        object->velocity_x = (velocity < 0 ? velocity + 3 : velocity) >> 2;
    }
    if ((g_col_flags & 9) != 0 && type != 0x0b)
    {
        velocity = -object->velocity_y;
        object->velocity_y = (velocity < 0 ? velocity + 3 : velocity) >> 2;
        object->velocity_x /= 2;
        object->velocity_z /= 2;
        if ((object->velocity_y < 0 ? -object->velocity_y : object->velocity_y) > 0x100)
            sound_play_positional(0x18, 0, 0x7f, object->col.x, object->col.y, object->col.z);
    }
    if ((g_col_flags & 6) != 0 && type != 0x0b && ((object->velocity_x < 0 ? -object->velocity_x : object->velocity_x) > 0x100 || (object->velocity_z < 0 ? -object->velocity_z : object->velocity_z) > 0x100))
        sound_play_positional(0x18, 0, 0x7f, object->col.x, object->col.y, object->col.z);
    if (g_col_flags != 0 && type == 0x0b)
        sound_play_positional(0x18, 0, 0x7f, object->col.x, object->col.y, object->col.z);
    if (type == 0x86)
    {
        SPRITE *smoke = (SPRITE *)continuous_weapon_particle_create(object->col.x, object->col.y - 0x1000, object->col.z - 0x100, g_spr_pal.cluts[CLUT_SIGNAL_FLARE]);
        if (smoke != 0)
        {
            smoke->light_step = -3;
            smoke->col.receives_mask = 4;
            smoke->col.sends_mask = 0x80;
            smoke->col.hit.damage = 1;
            smoke->velocity_x *= 2;
            col_box_set(smoke, 0xe6, 0xe6, 0xe6);
            object->col.box_y += 0x2000;
        }
    }
    if ((g_col_flags & 1) != 0 && type == 0x0b)
    {
        SPRITE *effect;
        sint32 surface = g_stage_env->map_resource;
        object_destroy(object);
        if (g_col_surface_resource == surface || g_col_surface_resource == -1)
        {
            effect = surface_impact_create(old_x, old_y, old_z, g_surface_impact_slow_animation);
            sound_play_positional(0x24, 0, 0x96, old_x, old_y, old_z);
        }
        else
        {
            effect = world_spr_create(old_x, old_y, old_z, g_expl_impact_animation);
            sound_play_positional(9, 0, 0x96, old_x, old_y, old_z);
            expl_flash_create(old_x, old_y - 0x4000, old_z - 0x100);
        }
        effect->cull_distance = 0x3e800;
        effect->force_light = 1;
        effect->scale_x = (random_range(0x800) + 0x2000);
        col_box_set(effect, 0xa0, 0x60, 0x80);
        effect->col.receives_mask = 4;
        effect->col.sends_mask = 1;
        effect->col.object_type = 2;
        effect->col.hit.damage = 1;
        effect->col.box_y += 0x1000;
        player_particle_ring_create(effect->col.x, effect->col.y, effect->col.z, 0x0c, 3);
        disp_shake_start();
    }
}

/* Direct FUN_80099230 constructor. */
SPRITE *thrown_explosive_create(sint32 x, sint32 y, sint32 z, const ANIM_COMMAND *animation)
{
    SPRITE *object = (SPRITE *)runtime_heap_allocate(sizeof(*object));
    POLY_FT4 *poly = &object->col.prim;
    sint16 type;
    SetPolyFT4(poly);
    setShadeTex(poly, 0);
    object->col.object.update = thrown_explosive_update;
    linked_list_append(&g_object_list, object);
    object->cull_distance = 0x3e800;
    object->scale_x = 0x1708;
    animation_start(&object->animation, animation);
    object->col.x = x;
    object->col.y = y;
    object->col.z = z;
    type = g_player->thrown_item_type;
    object->col.object_type = type == 0x89 ? 0x86 : type;
    return object;
}

/* Direct special-projectile callbacks FUN_8009DF84/FUN_8009E118/FUN_800A1C70. */

/* Original: FUN_8009DF84. */
void homing_projectile_steer(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    sint32 vx = object->velocity_x;
    sint32 vz = object->velocity_z;
    /* 0x8009DFAC: slt v0, abs(vz), abs(vx).  Steer the weaker
     * horizontal component so homing does not rotate the dominant axis. */
    if (abs_s32(vz) < abs_s32(vx))
    {
        if (object->col.z < object->behavior.projectile.target_z)
            vz = (vz + 0x40u);
        if (object->col.z > object->behavior.projectile.target_z)
            vz = (vz - 0x40u);
        if (vz < -0x400)
            vz = -0x400;
        if (vz > 0x400)
            vz = 0x400;
        object->velocity_z = vz;
    }
    else
    {
        if (object->col.x < object->behavior.projectile.target_x)
            vx = (vx + 0x40u);
        if (object->col.x > object->behavior.projectile.target_x)
            vx = (vx - 0x40u);
        if (vx < -0x400)
            vx = -0x400;
        if (vx > 0x400)
            vx = 0x400;
        object->velocity_x = vx;
    }
    if (object->col.y < object->behavior.projectile.target_y)
        object->velocity_y = (object->velocity_y + 0x40u);
    if (object->col.y > object->behavior.projectile.target_y)
        object->velocity_y = (object->velocity_y - 0x40u);
    if (object->velocity_y < -0x200)
        object->velocity_y = -0x200;
    if (object->velocity_y > 0x200)
        object->velocity_y = 0x200;
}

/* Original: FUN_800A1C70. */
void projectile_trail_emit(void *raw_object)
{
    SPRITE *object = (SPRITE *)raw_object;
    if ((g_render_state.frame & 3) == 0)
    {
        SPRITE *trail = world_spr_create(object->col.x, object->col.y + 0x800, object->col.z + 0x100, g_particle_animation);
        trail->scale_x = 0x800;
        trail->force_light = 1;
    }
}

/* Original: SLES_004.74:FUN_8009DDC4. */
void dust_effect_create(sint32 x, sint32 y, sint32 z)
{
    const ANIM_COMMAND *script = random_range(2) == 0 ? g_dust_animation0 : g_dust_animation1;
    SPRITE *effect;
    x += random_range(0x2000) - 0x1000;
    y -= random_range(0x4000);
    effect = world_spr_create(x, y, z - 0x100, script);
    effect->light_step = -2;
    effect->velocity_x = random_range(1) * 2 - 1;
    effect->force_light = 1;
}
