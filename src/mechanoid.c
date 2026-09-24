#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "animation.h"
#include "game_sound.h"
#include "camera.h"
#include "code_module.h"
#include "collision.h"
#include "global.h"
#include "hq.h"
#include "mechanoid.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "original_file.h"
#include "original_tables.h"
#include "game_file.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "sprite.h"
#include "sprite_renderer.h"

/* Variables. */
/* COMMON0/BIGROB.BIN, PAL overlay 0x800FADD0..0x800FC85C.
 * Control flow and constants are translated from the MIPS listing; the IDA
 * output is used only to name temporaries.  Immutable scripts remain in the
 * loaded original overlay and AIRSHIP/RB.POD remains the pose resource. */

static uint8 *pose_resource, *robot_models;

static sint32 pose_size, robot_size;

static uint32 trace_updates;

/* Functions. */
static void mechanoid_trace(const char *event, MECHANOID_ACTOR *b)
{
    const char *name = getenv("OA_MECHANOID_TRACE");
    FILE *f;
    if (!name || !b)
        return;
    f = fopen(strcmp(name, "1") == 0 ? "mechanoid_port.csv" : name, strcmp(event, "construct") == 0 ? "w" : "a");
    if (!f)
        return;
    if (strcmp(event, "construct") == 0)
        fprintf(f, "event,tick,x,y,z,hp,phase,frame,part,flags,yaw,aim,model_base\n");
    fprintf(f, "%s,%u,%d,%d,%d,%d,%d,%d,%d,%u,%d,%d,%d\n", event, g_frame_counter, b->collision.x, b->collision.y, b->collision.z, b->health, b->phase, b->animation.frame, b->animation.delay, b->animation.flags, b->nodes[0].rotation_y, b->aim_yaw, g_mechanoid_model_base);
    fclose(f);
}

sint32 mechanoid_load_resources(void)
{
    sint32 base;
    overlay_module_load("COMMON0\\BIGROB.BIN");
    /* FUN_800AB478 executes the stage resource script on every level load.
       Its -6 command reaches FUN_80095300, which allocates and fills a fresh
       model buffer before FUN_80092D50 mutates the packed polygons in place.
       The host allocations survive the runtime-heap reset, so release the
       preceding level's replacements explicitly before following that path. */
    app_file_free(pose_resource);
    app_file_free(robot_models);
    /* PAL AB438..AB458 retains the sector-sized POD allocation. The model
       parser also consumes the empty 28th record at ROBOT.MOD +0x2042,
       beyond ISO EOF but within the transferred sector (PAL RAM audit).
       Both disc-sector tails are verified zero by audit_v2_loading.py. */
    pose_resource = (uint8 *)game_file_load_sector_buffer("AIRSHIP\\RB.POD", &pose_size);
    robot_models = (uint8 *)game_file_load_sector_buffer("COMMON0\\ROBOT.MOD", &robot_size);
    if (!pose_resource || pose_size < 202752 || !robot_models || robot_size <= 0)
        return 0;
    /* Nested asset script 0x800CC990 executes FUN_80095300 with 28 models,
       three material ids 0x15800 and destination DAT_800D4148. */
    base = model_append(robot_models, robot_size, 28, 0x15800, 0x15800, 0x15800);
    if (base < 0)
        return 0;
    if (getenv("OA_MODEL_PARSER_AUDIT"))
        model_dump_parser_audit("../status/mechanoid_model_parser_port.bin");
    g_mechanoid_model_base = (sint16)base;
    return 1;
}

/* Exact 0x800FB018..0x800FB08C. */
/* Original: BIGROB_800FB018. */
static void mechanoid_clamp_to_arena(MECHANOID_ACTOR *b, sint32 *x, sint32 *z)
{
    if (*x < b->min_x)
        *x = b->min_x;
    if (*x > b->max_x)
        *x = b->max_x;
    if (*z < b->min_z)
        *z = b->min_z;
    if (*z > b->max_z)
        *z = b->max_z;
}

/* Exact FUN_800ACF98, 0x800ACF98..0x800AD034. */
sint16 angle_approach_wrapped(sint16 target, sint32 step, sint16 current)
{
    sint32 distance = target - current, move = 0;
    if (distance < 0)
        distance = -distance;
    if (distance > (sint16)step)
        distance = (sint16)step;
    if (current != target)
    {
        move = target < current ? -distance : distance;
        distance = current - target;
        if (distance < 0)
            distance = -distance;
        if (distance >= 0x801)
            move = -move;
    }
    return (sint16)((current + move) & 0xfff);
}

/* Original: BIGROB_800FBD30. */
static void mechanoid_damage(MECHANOID_ACTOR *boss, void *source)
{
    if (object_damage_apply(boss, source))
    {
        boss->collision.receives_mask = 0;
        boss->phase = 1;
    }
}

/* Original: BIGROB_800FBD6C. */
static void mechanoid_damage_player_on_contact(MECHANOID_ACTOR *boss, void *source)
{
    player_apply_damage(0, g_player);
}

/* Original: BIGROB_800FBD94. */
static void mechanoid_projectile_expand_collision(SPRITE *o)
{
    o->collision.box_x -= 0x300;
    o->collision.box_y -= 0x300;
    o->collision.box_z -= 0x300;
    o->collision.width += 0x300;
    o->collision.height += 0x300;
    o->collision.depth += 0x300;
}

/* Original: BIGROB_800FBDE0. */
static void mechanoid_projectile_retarget_player(SPRITE *o)
{
    sint32 line[6];
    o->velocity_y = -o->velocity_y;
    fixed_dda_initialize(o->collision.x, o->collision.y, o->collision.z, g_player_world_x, g_player_world_y, g_player_world_z, line);
    o->velocity_x = 3 * div_256_trunc(line[3]);
    o->velocity_z = 3 * div_256_trunc(line[5]);
}

/* Original: BIGROB_800FBE94. */
static void mechanoid_projectile_update_trail(SPRITE *o)
{
    if (o->velocity_y > 0x600)
        o->velocity_y = 0x600;
    if ((g_frame_counter & 3) == 0)
    {
        SPRITE *e;
        o->field_a8 = (sint16)(-8 * random_range(8));
        e = world_sprite_create(o->collision.x, o->collision.y, o->collision.z, win_code_module_address(0x800fc848));
        e->field_aa = -4;
        e->scale_step = -0x80;
    }
}

static void spawn_attack_projectile(MECHANOID_ACTOR *b)
{
    SPRITE *p = world_sprite_create(b->nodes[35].world_x, b->nodes[35].world_y, b->nodes[35].world_z, win_code_module_address(0x800fbf3c));
    p->frame_callback = (FUNC_COLLISION_UPDATE)mechanoid_projectile_update_trail;
    p->collision_callback = (FUNC_COLLISION_UPDATE)mechanoid_projectile_retarget_player;
    p->acceleration_y = 0x40;
    p->rotation_step_x = 0x20;
    p->collision.callback_14 = player_apply_damage;
    p->cull_distance = 0x3e800;
    p->collision.receives_mask = 2;
    collision_box_set(p, 0x40, 0x40, 0x40);
    p->collision.box_y += 0x2000;
}

static void draw_boss(MECHANOID_ACTOR *b)
{
    static MATRIX identity = {{{0x1000, 0, 0}, {0, 0x1000, 0}, {0, 0, 0x1000}}, {0, 0, 0}};
    b->nodes[0].translation_x = b->collision.x - g_camera_world_x;
    b->nodes[0].translation_y = b->collision.y - g_camera_world_y;
    b->nodes[0].translation_z = b->collision.z - g_camera_world_z;
    g_render_depth_bucket = 0x43f - (g_render_row_world_z - b->collision.z) / 0x100;
    g_model_render_frame = 0;
    g_current_model_world_z = b->collision.z;
    model_render_begin();
    if (b->hit_flash_ticks)
    {
        b->hit_flash_ticks -= 1;
        g_model_clut_override = g_hit_flash_clut;
    }
    model_render_node(&b->nodes[0], &identity);
    g_model_clut_override = 0;
    model_render_end();
}

/* Exact control-flow translation of 0x800FB090..0x800FBD30. */
/* Original: BIGROB_800FB090. */
static void mechanoid_update(MECHANOID_ACTOR *b)
{
    MODEL_NODE *bone;
    ANIM *c = &b->animation;
    const sint32 *start;
    sint32 line[6], angle, current, turn;
    if ((trace_updates++ & 7) == 0)
        mechanoid_trace("update", b);
    if ((g_frame_counter & 0x1ff) == 0)
    {
        const sint16 *types = (const sint16 *)win_code_module_address(0x800fc834u);
        PLAYER_ACTION *item = player_action_object_create(g_player_world_x - 0x4000, g_player_world_y - 0xc800, g_player_world_z, types[random_range(8)]);
        mechanoid_clamp_to_arena(b, &item->collision.x, &item->collision.z);
        item->lifetime = 0xc8;
    }
    animation_update(c);
    start = animation_get_start(c);
    if (start == win_code_module_address(0x800fc1cc) && (b->animation.flags & 2) && b->phase == 1)
        animation_start(c, win_code_module_address(0x800fc364));
    if (b->phase == 1 && b->animation.frame == 0x48)
    {
        b->phase = 2;
        animation_start(c, win_code_module_address(0x800fc814));
        sound_voice_stop(&b->sound_handle);
    }
    if (b->phase && (g_frame_counter & 3) == 0)
    {
        const sint32 *dust = (const sint32 *)original_tables_address(random_range(2) ? 0x800c8f4cu : 0x800c8ee0u);
        bone = &b->nodes[1 + random_range(35)];
        sound_play_positional(random_range(1) ? 0x34 : 0x35, 0, 0x7f, b->collision.x, b->collision.y, b->collision.z);
        {
            SPRITE *sprite = world_sprite_create(bone->world_x, bone->world_y, bone->world_z, dust);
            sprite->field_aa = -2;
            sprite->velocity_x = random_range(1) * 2 - 1;
            sprite->field_ae = 1;
            sprite->field_ac = (sint16)(0x480 - div_256_trunc(g_render_row_world_z + 0x2000 - bone->world_z));
        }
    }
    if (b->animation.frame == 0x1d0)
        g_mechanoid_destruction_complete = 1;
    model_apply_pose(pose_resource, b->node_table, b->animation.frame);
    b->nodes[20].translation_y += 0xe00;
    if (!b->moving)
    {
        if (b->target_x - 0x1000 < b->collision.x && b->collision.x < b->target_x + 0x1000 && b->target_z - 0x1000 < b->collision.z && b->collision.z < b->target_z + 0x1000)
            b->target_reached = 1;
        if (b->target_reached)
        {
            if (b->animation.frame == 2)
                animation_start(c, win_code_module_address(0x800fbf9c));
            if (b->animation.frame == 0x1b)
                animation_start(c, win_code_module_address(0x800fc074));
        }
        start = animation_get_start(c);
        if (animation_get_next(c) == 0 && (start == win_code_module_address(0x800fbf9c) || start == win_code_module_address(0x800fc074)))
        {
            if (b->attack_variant == 0)
                animation_start(c, win_code_module_address(0x800fc1cc));
            if (b->attack_variant == 1)
                animation_start(c, win_code_module_address(0x800fc730));
            if (b->attack_variant == 2)
                animation_start(c, win_code_module_address(0x800fc7ac));
            if (++b->attack_variant == 3)
                b->attack_variant = 0;
            b->attack_sequence_active = 1;
        }
    }
    if (b->animation.frame == 0x112)
        b->velocity_y = -0x800;
    if (b->animation.frame == 0x11b)
    {
        b->velocity_y = 0xa00;
        b->collision.x = g_player_world_x;
        b->collision.z = g_player_world_z;
        mechanoid_clamp_to_arena(b, &b->collision.x, &b->collision.z);
    }
    if ((b->animation.flags & 2) && b->sound_handle == -1)
        b->sound_handle = (sint16)sound_play_positional(0x54, 0, 0xc8, b->collision.x, b->collision.y, b->collision.z);
    if (b->animation.frame == 0x148 && b->animation.delay == 0xf)
        spawn_attack_projectile(b);
    if (b->attack_sequence_active && animation_get_next(c) == 0)
    {
        sound_voice_stop(&b->sound_handle);
        b->attack_sequence_active = 0;
        b->target_reached = 0;
        b->moving = 1;
        b->target_x = g_camera_world_x;
        b->target_z = g_camera_world_z + 0x16c00;
        mechanoid_clamp_to_arena(b, &b->target_x, &b->target_z);
        fixed_dda_initialize(b->collision.x, b->collision.y, b->collision.z, b->target_x, b->collision.y, b->target_z, line);
        memcpy(b->movement_line, line, sizeof(line));
        angle = (fixed_angle_from_vector(div_4096_trunc(line[3]), div_4096_trunc(line[5])) * 4) & 0xfff0;
        b->target_yaw = (sint16)angle;
        b->movement_line[4] = line[3] + line[3] / 2;
        b->movement_z = line[5] + line[5] / 2;
        current = b->nodes[0].rotation_y;
        turn = angle < current ? -24 : 24;
        if (abs(current - angle) >= 0x801)
            turn = -turn;
        b->yaw_step = (sint16)turn;
        animation_start(c, turn < 0 ? win_code_module_address(0x800fc390) : win_code_module_address(0x800fc560));
        if (current == angle)
            b->moving = 0;
    }
    if (b->moving && b->nodes[0].rotation_y != b->target_yaw && (b->animation.flags & 4))
        b->nodes[0].rotation_y = angle_approach_wrapped(b->target_yaw, 0x38, b->nodes[0].rotation_y);
    else if (!b->moving || b->nodes[0].rotation_y == b->target_yaw)
    {
        b->moving = 0;
        if (animation_get_start(c) != win_code_module_address(0x800fc150) && animation_get_next(c) == 0)
            animation_start(c, win_code_module_address(0x800fc150));
        if (animation_get_next(c) == 0 && animation_get_start(c) == win_code_module_address(0x800fc150))
            animation_start(c, win_code_module_address(0x800fbf7c));
    }
    if (b->animation.flags & 2)
    {
        /* BIGROB:800FB828 calls SLES:800A4E1C; delay slot sets divisor=1. */
        angle = ((angle_to_player(b->nodes[34].world_x, b->nodes[34].world_z, 1) * 4 - b->nodes[0].rotation_y) & 0xfff) + b->aim_jitter;
        angle &= 0xfff;
        if ((g_frame_counter & 7) == 0)
            b->aim_jitter = (sint16)(20 * (random_range(0x20) - 0x10));
        b->aim_yaw = angle_approach_wrapped((sint16)angle, 0x20, b->aim_yaw);
    }
    else
        b->aim_yaw = angle_approach_wrapped(0, 0x20, b->aim_yaw);
    b->nodes[31].rotation_y = b->aim_yaw;
    if (b->animation.flags & 1)
    {
        b->collision.x += div_256_trunc(b->movement_line[4]);
        b->collision.z += div_256_trunc(b->movement_z);
    }
    b->collision.y += b->velocity_y;
    if (b->collision.y > b->base_y)
    {
        b->collision.y = b->base_y;
        b->velocity_y = 0;
        animation_start(c, win_code_module_address(0x800fc7fc));
        camera_shake_start();
        sound_play_nonpositional(0x55, 0, 0x96);
    }
    draw_boss(b);
    if (b->animation.flags & 2)
    {
        if ((g_frame_counter & 7) == 0)
        {
            fixed_dda_initialize(b->nodes[33].world_x, b->nodes[33].world_y, b->nodes[33].world_z, b->nodes[34].world_x, b->nodes[34].world_y, b->nodes[34].world_z, line);
            {
                SPRITE *sprite = world_sprite_create(b->nodes[34].world_x, b->nodes[34].world_y, b->nodes[34].world_z, win_code_module_address(0x800fbf28));
                sprite->velocity_x = 5 * div_256_trunc(line[3]);
                sprite->velocity_y = 5 * div_256_trunc(line[4]) + 0x100;
                sprite->velocity_z = 5 * div_256_trunc(line[5]);
                sprite->field_aa = -4;
                sprite->collision.callback_14 = player_apply_damage;
                sprite->damage = 0x80;
                sprite->collision.object_type = 0;
                sprite->collision.sends_mask |= 1;
                sprite->collision.receives_mask |= 4;
                sprite->collision.receives_mask = 2;
                collision_box_set(sprite, 0x20, 0x20, 0x20);
                sprite->scale_x = 0x200;
                sprite->scale_step = 0x80;
                sprite->frame_callback = (FUNC_COLLISION_UPDATE)mechanoid_projectile_expand_collision;
                sprite->collision.box_y += 0x1000;
            }
        }
        g_next_frame_object->light_delta = -0x28;
        g_next_frame_object->force_light = 1;
        bone = &b->nodes[35];
        render_world_sprite(0x2e800, bone->world_x, bone->world_y, bone->world_z, 0, 0x1000, 0x1000, (POLY_FT4 *)b->collision.prim, 0, 2, 0, (g_frame_counter << 2) & 0x3ff);
    }
    if (b->animation.frame == 2 && b->animation.delay == 1)
    {
        sound_play_positional(0x55, -4, 0x82, b->collision.x, b->collision.y, b->collision.z);
        sound_play_positional(0x56, -7, 0x82, b->collision.x, b->collision.y, b->collision.z);
    }
    if (b->animation.frame == 0x1a && b->animation.delay == 1)
    {
        sound_play_positional(0x55, -4, 0x82, b->collision.x, b->collision.y, b->collision.z);
        sound_play_positional(0x57, -7, 0x82, b->collision.x, b->collision.y, b->collision.z);
    }
    hud_bar_render(g_boss_health_bar, b->health, 0, 0);
}

/* Original: BIGROB_800FADD4 (overlay address 0x800FADD4). */
void mechanoid_create(sint32 x, sint32 y, sint32 z)
{
    MECHANOID_ACTOR *b;
    const uint8 *map;
    sint32 n;
    if (!pose_resource && !mechanoid_load_resources())
        return;
    b = (MECHANOID_ACTOR *)object_create(sizeof(*b), (FUNC_COLLISION_UPDATE)mechanoid_update);
    setPolyFT4(b->collision.prim);
    setShadeTex(b->collision.prim, 0);
    setSemiTrans(b->collision.prim, 1);
    b->collision.x = x;
    b->collision.y = y;
    b->collision.z = z;
    b->base_y = y;
    map = (const uint8 *)win_code_module_address(0x800fbf50u);
    for (n = 0; n < 36; n++)
    {
        MODEL_NODE *node = &b->nodes[n + 1];
        b->node_table[n] = node;
        if (map[n])
            node->model_id = (sint32)g_mechanoid_model_base + map[n];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(pose_resource, b->node_table, 1);
    b->movement_line[0] = 2;
    model_attach_child(&b->nodes[29], &b->nodes[0]);
    animation_start(&b->animation, win_code_module_address(0x800fbf7c));
    b->target_x = x;
    b->target_z = z;
    b->collision.callback_18 = (FUNC_COLLISION_CALLBACK)mechanoid_damage;
    b->collision.callback_14 = (FUNC_COLLISION_CALLBACK)mechanoid_damage_player_on_contact;
    b->collision.receives_mask = 0x43;
    b->health = 3500;
    collision_box_set(b, 0x50, 0x100, 0x50);
    b->collision.box_y += 0x10000;
    hud_bar_initialize(g_boss_health_bar, -0x28, -0x60, 0x50, 3500);
    animation_set_owner(&b->animation, b);
    b->sound_handle = -1;
    b->min_x = x - 0x16000;
    b->max_x = x + 0x12000;
    b->min_z = z - 0x8000;
    b->max_z = z + 0x8000;
    b->nodes[0].rotation_y = 0x800;
    trace_updates = 0;
    mechanoid_trace("construct", b);
}
