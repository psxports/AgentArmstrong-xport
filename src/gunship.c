#include <stddef.h>

#include "spatial.h"
#include "game_math.h"
#include "effect_update.h"
#include "airship.h"
#include "game_sound.h"
#include "camera.h"
#include "collision.h"
#include "map_render.h"
#include "render.h"
#include "gunship.h"
#include "hq.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "sprite.h"

/* Original: DAT_800D3ED8. */
MODEL_POSE_RESOURCE g_gunship_initial_pose;

/* Original: DAT_800D3ED2. */
sint16 g_gunship_model_base;

/* Types. */
/* JUNGLE/GUNSHIP.BIN, PAL overlay 0x800FADD4..0x800FB4DC. */

typedef struct
{
    COLLISION col;
    MODEL_NODE nodes[31];
    MODEL_NODE *node_table[31];
    EFFECT *owner_effect;
    sint32 origin_x;
    sint32 velocity_x;
    sint32 target_velocity_x;
    FIXED_DDA_LINE aim_line;
    sint16 aim_bias;
    sint16 attack_ticks;
    sint16 active_gun_side;
    sint16 target_heading;
} GUNSHIP_ACTOR;

/* Functions. */
/* Original: GUNSHIP:GUNSHIP_800FAF48. */
static void gunship_update(void *raw_o)
{
    GUNSHIP_ACTOR *actor = (GUNSHIP_ACTOR *)raw_o;
    /* Original data: JUNGLE/GUNSHIP.BIN:0x800FB4DC..0x800FB4E3 */
    static const uint8 firing_nodes[4][2] = {{27, 28}, {29, 30}, {17, 19}, {18, 20}};
    static const MATRIX identity = {{{4096, 0, 0}, {0, 4096, 0}, {0, 0, 4096}}, {0, 0, 0}};
    MODEL_NODE *target_node;
    PROJECTILE *projectile;
    sint32 angle, gun_idx, bob, depth;
    if (!world_object_is_visible(actor->col.x, actor->col.y, actor->col.z, 0x7d000))
    {
        object_destroy(actor);
        actor->owner_effect->type = 0x7b;
    }
    actor->col.x = (uint32)actor->col.x + actor->velocity_x;
    if (actor->col.x < (actor->origin_x - 0x8000))
        actor->target_velocity_x = 0x200;
    if ((actor->origin_x + 0x8000) < actor->col.x)
        actor->target_velocity_x = -0x200;
    if (actor->velocity_x < actor->target_velocity_x)
        actor->velocity_x += 8;
    if (actor->target_velocity_x < actor->velocity_x)
        actor->velocity_x -= 8;
    if ((g_render_state.frame & 15) == 0)
        actor->target_heading = -(random_range(140) + 64) & 0xfff;
    if (actor->active_gun_side)
    {
        target_node = &actor->nodes[25];
        gun_idx = 0;
        actor->nodes[26].rotation_y = angle_approach_wrapped(actor->target_heading, 16, actor->nodes[26].rotation_y);
    }
    else
    {
        target_node = &actor->nodes[15];
        gun_idx = 2;
        actor->nodes[17].rotation_y = angle_approach_wrapped((actor->target_heading + 230) & 0xfff, 16, actor->nodes[17].rotation_y);
    }
    if (actor->attack_ticks == 0)
    {
        actor->attack_ticks = random_range(120) + 160;
        actor->active_gun_side ^= 1;
    }
    else
        --actor->attack_ticks;
    angle = 0;
    if (actor->attack_ticks >= 20)
    {
        fixed_dda_initialize(target_node->world_x, target_node->world_y, target_node->world_z, g_player_world.x, g_player_world.y - 0x3000u, g_player_world.z, &actor->aim_line);
        angle = (fixed_angle_from_vector(div_2048_trunc(actor->aim_line.step.x), div_2048_trunc(actor->aim_line.step.z)) << 2) - actor->nodes[0].rotation_y + actor->aim_bias;
        if (target_node == &actor->nodes[15])
            angle += 0x800;
        angle &= 0xfff;
    }
    target_node->rotation_y = angle_approach_wrapped(angle, 24, target_node->rotation_y);
    actor->nodes[0].translation_x = (uint32)actor->col.x - g_camera.world_x;
    bob = player_bob_offset_update(0);
    actor->nodes[0].translation_y = (uint32)actor->col.y - g_camera.world_y + bob;
    actor->nodes[0].translation_z = (uint32)actor->col.z - g_camera.world_z;
    depth = (uint32)g_map_render.depth_anchor_z - actor->col.z;
    g_model_draw.depth_bucket = 0x480u - (depth / 0x100);
    g_model_draw.target_frame = 0;
    g_model_draw.world_z = actor->col.z;
    model_render_begin();
    if (actor->col.flash_clut_ticks)
    {
        g_model_draw.clut_override = g_spr_pal.hit_flash;
        --actor->col.flash_clut_ticks;
    }
    model_render_node(&actor->nodes[0], &identity);
    g_model_draw.clut_override = 0;
    hierarchy_col_box_update(&actor->col, &actor->nodes[7], 4);
    model_render_end();
    hud_bar_render(&g_boss_health_bar, actor->col.hit.health, 0, 0);
    if (actor->attack_ticks >= 21 && actor->attack_ticks < 160 && (g_render_state.frame & 3) == 0)
    {
        MODEL_NODE *from, *to;
        if ((g_render_state.frame & 4) == 0)
            ++gun_idx;
        from = &actor->nodes[firing_nodes[gun_idx][0]];
        to = &actor->nodes[firing_nodes[gun_idx][1]];
        projectile = projectile_create(from->world_x, from->world_y - 0x100u, from->world_z, to->world_x, to->world_y - 0x100u, to->world_z);
        projectile->motion.step.x = projectile->motion.step.x * 2u;
        projectile->motion.step.z = projectile->motion.step.z * 2u;
        projectile->motion.step.y = projectile->motion.step.y * 2u;
    }
}

/* Original: GUNSHIP:GUNSHIP_800FB3A0. */
static void gunship_damage(void *raw_col, void *raw_src)
{
    COLLISION *col = (COLLISION *)raw_col;
    COLLISION *src = (COLLISION *)raw_src;
    GUNSHIP_ACTOR *o = (GUNSHIP_ACTOR *)col;
    SPRITE *p;
    o->col.flash_clut_ticks = 1;
    if (!object_damage_apply(o, src))
        return;
    object_destroy(o);
    player_particle_ring_create(o->col.x, o->col.y, o->col.z, 12, 3);
    disp_shake_start();
    speech_random_request(4);
    expl_flash_create(o->col.x, o->col.y, o->col.z - 0x100);
    timed_callback_create(expl_debris_particle_create, o->col.x, o->col.y, o->col.z, 0x40, 8);
    timed_callback_create(expl_trail_particle_create, o->col.x, o->col.y, o->col.z, 0x30, 0x10);
    p = world_spr_create(o->col.x, o->col.y, o->col.z - 0x2800, g_expl_impact_animation);
    p->scale_x = 0x2000;
    sound_play_positional(10, 0, 0x7f, o->col.x, o->col.y, o->col.z);
    --g_mission.counts[13];
}

/* Original: GUNSHIP:GUNSHIP_800FADD4. */
void gunship_create(EFFECT *e)
{
    static const sint8 m[31] = {6, 0, 5, 4, 1, 3, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    GUNSHIP_ACTOR *o = (GUNSHIP_ACTOR *)object_create(sizeof(*o), gunship_update);
    sint32 n;
    e->type = 0;
    o->col.x = o->origin_x = e->x;
    o->col.y = e->y - 0x3800;
    o->col.z = e->z;
    o->col.receives_mask = 1;
    o->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    o->col.hit_callback = gunship_damage;
    o->col.hit.health = 500;
    o->target_velocity_x = -0x200;
    o->owner_effect = e;
    for (n = 0; n < 31; n++)
    {
        MODEL_NODE *node = &o->nodes[n];
        o->node_table[n] = node;
        node->model_id = m[n] < 0 ? 0 : g_gunship_model_base + m[n];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_gunship_initial_pose, o->node_table, 31, 1);
    o->nodes[0].rotation_y = 1000;
    o->nodes[0].translation_y = 0x2000;
    hud_bar_initialize(&g_boss_health_bar, -40, -96, 80, 500);
    o->target_heading = 0xf40;
}
