#include <stddef.h>
#include "airship.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "global.h"
#include "gunship.h"
#include "hq.h"
#include "mechanoid.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "sprite.h"

/* Types. */
/* JUNGLE/GUNSHIP.BIN, PAL overlay 0x800FADD4..0x800FB4DC. */

typedef struct
{
    COLLISION collision;        /* +0x000 */
    sint16 health;              /* +0x078 */
    sint16 flash_clut_ticks;    /* +0x07A */
    uint8 field_07c[0x60];      /* +0x07C */
    MODEL_NODE nodes[31];       /* +0x0DC */
    uint8 field_c7c[0x60];      /* +0xC7C */
    MODEL_NODE *node_table[31]; /* +0xCDC */
    uint8 field_d58[4];         /* +0xD58 */
    EFFECT *owner_effect;       /* +0xD5C */
    sint32 origin_x;            /* +0xD60 */
    sint32 velocity_x;          /* +0xD64 */
    sint32 target_velocity_x;   /* +0xD68 */
    uint8 field_d6c[4];         /* +0xD6C */
    sint32 aim_vector[7];       /* +0xD70 */
    uint8 field_d8c[0x0E];      /* +0xD8C */
    sint16 aim_bias;            /* +0xD9A */
    sint16 attack_ticks;        /* +0xD9C */
    sint16 active_gun_side;     /* +0xD9E */
    sint16 target_heading;      /* +0xDA0 */
    uint8 field_da2[2];         /* +0xDA2 */
} GUNSHIP_ACTOR;

#if defined(AP_32BIT)
typedef char GunshipActor_size_da4[sizeof(GUNSHIP_ACTOR) == 0xDA4 ? 1 : -1];
typedef char GunshipActor_nodes_at_dc[offsetof(GUNSHIP_ACTOR, nodes) == 0xDC ? 1 : -1];
typedef char GunshipActor_table_at_cdc[offsetof(GUNSHIP_ACTOR, node_table) == 0xCDC ? 1 : -1];
typedef char GunshipActor_owner_at_d5c[offsetof(GUNSHIP_ACTOR, owner_effect) == 0xD5C ? 1 : -1];
#endif

/* Functions. */
/* Original: GUNSHIP_800FAF48. */
static void gunship_update(GUNSHIP_ACTOR *o)
{
    static const uint8 a[8] = {27, 28, 29, 30, 17, 19, 18, 20};
    static const uint8 b[8] = {28, 29, 30, 17, 19, 18, 20, 6};
    static const MATRIX id = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    MODEL_NODE *target_node;
    PROJECTILE *p;
    sint32 q, k;
    if (!world_object_is_visible(o->collision.x, o->collision.y, o->collision.z, 0x7d000))
    {
        object_destroy(o);
        o->owner_effect->type = 0x7b;
    }
    o->collision.x += o->velocity_x;
    if (o->collision.x < o->origin_x - 0x8000)
        o->target_velocity_x = 0x200;
    if (o->origin_x + 0x8000 < o->collision.x)
        o->target_velocity_x = -0x200;
    if (o->velocity_x < o->target_velocity_x)
        o->velocity_x += 8;
    if (o->target_velocity_x < o->velocity_x)
        o->velocity_x -= 8;
    if ((g_frame_counter & 15) == 0)
        o->target_heading = (sint16)(-(random_range(140) + 64) & 0xfff);
    if (o->active_gun_side)
    {
        target_node = &o->nodes[25];
        k = 0;
        o->nodes[26].rotation_y = angle_approach_wrapped(o->target_heading, 16, o->nodes[26].rotation_y);
    }
    else
    {
        target_node = &o->nodes[15];
        k = 4;
        o->nodes[17].rotation_y = angle_approach_wrapped((sint16)(((uint16)o->target_heading + 230) & 0xfff), 16, o->nodes[17].rotation_y);
    }
    if (o->attack_ticks == 0)
    {
        o->attack_ticks = (sint16)(random_range(120) + 160);
        o->active_gun_side ^= 1;
    }
    else
        --o->attack_ticks;
    q = 0;
    if (o->attack_ticks >= 20)
    {
        fixed_dda_initialize(target_node->world_x, target_node->world_y, target_node->world_z, g_player_world_x, g_player_world_y - 0x3000, g_player_world_z, o->aim_vector);
        q = (fixed_angle_from_vector(div_2048_trunc(o->aim_vector[3]), div_2048_trunc(o->aim_vector[5])) << 2) - o->nodes[0].rotation_y + o->aim_bias;
        if (target_node == &o->nodes[15])
            q += 0x800;
        q &= 0xfff;
    }
    target_node->rotation_y = angle_approach_wrapped((sint16)q, 24, target_node->rotation_y);
    o->nodes[0].translation_x = o->collision.x - g_camera_world_x;
    o->nodes[0].translation_y = o->collision.y - g_camera_world_y + player_bob_offset_update(0);
    o->nodes[0].translation_z = o->collision.z - g_camera_world_z;
    g_render_depth_bucket = 0x480 - (g_render_row_world_z - o->collision.z) / 0x100;
    g_model_render_frame = 0;
    g_current_model_world_z = o->collision.z;
    model_render_begin();
    if (o->flash_clut_ticks)
    {
        g_model_clut_override = g_hit_flash_clut;
        --o->flash_clut_ticks;
    }
    model_render_node(&o->nodes[0], (MATRIX *)&id);
    g_model_clut_override = 0;
    hierarchy_collision_box_update(o, &o->nodes[7], 4);
    model_render_end();
    hud_bar_render(g_boss_health_bar, o->health, 0, 0);
    if ((uint16)((uint16)o->attack_ticks - 21) < 139 && (g_frame_counter & 3) == 0)
    {
        MODEL_NODE *from, *to;
        if ((g_frame_counter & 4) == 0)
            k += 2;
        from = &o->nodes[a[k]];
        to = &o->nodes[b[k]];
        p = projectile_create(from->world_x, from->world_y - 0x100, from->world_z, to->world_x, to->world_y - 0x100, to->world_z);
        p->motion[3] *= 2;
        p->motion[4] *= 2;
        p->motion[5] *= 2;
    }
}

/* Original: GUNSHIP_800FB3A0. */
static void gunship_damage(COLLISION *collision, COLLISION *source)
{
    GUNSHIP_ACTOR *o = (GUNSHIP_ACTOR *)collision;
    SPRITE *p;
    o->flash_clut_ticks = 1;
    if (!object_damage_apply(o, source))
        return;
    object_destroy(o);
    player_particle_ring_create(o->collision.x, o->collision.y, o->collision.z, 12, 3);
    camera_shake_start();
    speech_random_request(4);
    expl_flash_create(o->collision.x, o->collision.y, o->collision.z - 0x100);
    timed_callback_create(expl_debris_particle_create, o->collision.x, o->collision.y, o->collision.z, 0x40, 8);
    timed_callback_create(expl_trail_particle_create, o->collision.x, o->collision.y, o->collision.z, 0x30, 0x10);
    p = world_sprite_create(o->collision.x, o->collision.y, o->collision.z - 0x2800, (const sint32 *)player_assets_executable_address(0x800c8b5cu));
    p->scale_x = 0x2000;
    sound_play_positional(10, 0, 0x7f, o->collision.x, o->collision.y, o->collision.z);
    --g_objective_counts[13];
}

/* Original: GUNSHIP_800FADD4. */
void gunship_create(EFFECT *e)
{
    static const sint8 m[31] = {6, 0, 5, 4, 1, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    GUNSHIP_ACTOR *o = (GUNSHIP_ACTOR *)object_create(sizeof(*o), (FUNC_COLLISION_UPDATE)gunship_update);
    sint32 n;
    e->type = 0;
    o->collision.x = o->origin_x = e->x;
    o->collision.y = e->y - 0x3800;
    o->collision.z = e->z;
    o->collision.receives_mask = 1;
    o->collision.callback_18 = (FUNC_COLLISION_CALLBACK)gunship_damage;
    o->health = 500;
    o->target_velocity_x = -0x200;
    o->owner_effect = e;
    for (n = 0; n < 31; n++)
    {
        MODEL_NODE *node = &o->nodes[n];
        o->node_table[n] = node;
        node->model_id = m[n] < 0 ? 0 : g_gunship_model_base + m[n];
        node->render_flags = 0x3f;
    }
    model_initialize_pose(g_gunship_initial_pose, o->node_table, 1);
    o->nodes[0].rotation_y = 1000;
    o->nodes[0].translation_y = 0x2000;
    hud_bar_initialize(g_boss_health_bar, -40, -96, 80, 500);
    o->target_heading = 0xf40;
}
