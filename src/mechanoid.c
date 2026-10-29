#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game_math.h"
#include "native_config.h"
#include "animation.h"
#include "spatial.h"
#include "game_sound.h"
#include "camera.h"
#include "code_module.h"
#include "collision.h"
#include "map_render.h"
#include "render.h"
#include "hq.h"
#include "mechanoid.h"
#include "mission.h"
#include "model.h"
#include "object.h"
#include "game_file.h"
#include "player.h"
#include "projectile.h"
#include "random.h"
#include "sprite.h"
#include "sprite_renderer.h"

typedef struct
{
    COLLISION col;
    MODEL_NODE nodes[37]; /* Legacy +0x007C: root + 36 script nodes */
    MODEL_NODE *node_table[36];
    ANIM animation;
    sint32 velocity_x;
    sint32 velocity_z;
    sint16 target_yaw;
    sint16 yaw_step;
    sint32 target_x;
    sint32 target_z;
    sint16 aim_yaw;
    sint16 target_reached;
    sint16 moving;
    sint16 aim_jitter;
    sint16 attack_sequence_active;
    sint32 velocity_y;
    sint32 base_y;
    sint16 attack_variant;
    sint16 sound_handle;
    sint32 max_x;
    sint32 min_x;
    sint32 max_z;
    sint32 min_z;
    sint16 phase;
} MECHANOID_ACTOR;

/* Original: DAT_800D4148. */
sint16 g_mechanoid_model_base;

/* Original data: COMMON0/BIGROB.BIN:0x800FBF28..0x800FBF3B */
static const ANIM_COMMAND spray_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {100, 190465, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FBF3C..0x800FBF4F */
static const ANIM_COMMAND projectile_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {300, 186368, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FBF7C..0x800FBF9B */
static const ANIM_COMMAND idle_animation0[4] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0},
    {ANIM_FLAGS_SET, {1, 0, 0}, 0},
    {ANIM_FRAME_RANGE, {2, 51, 2}, 0},
    {ANIM_LOOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FBF9C..0x800FC073 */
static const ANIM_COMMAND attack_prepare_animation0[27] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FLAGS_CLEAR, {1, 0, 0}, 0}, {ANIM_SOUND_POSITIONAL, {86, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 52, 0}, 0}, {ANIM_STEP_FRAME, {2, 53, 0}, 0}, {ANIM_STEP_FRAME, {2, 54, 0}, 0}, {ANIM_STEP_FRAME, {2, 55, 0}, 0}, {ANIM_STEP_FRAME, {2, 56, 0}, 0}, {ANIM_STEP_FRAME, {2, 57, 0}, 0}, {ANIM_STEP_FRAME, {2, 58, 0}, 0}, {ANIM_STEP_FRAME, {2, 59, 0}, 0}, {ANIM_STEP_FRAME, {2, 60, 0}, 0}, {ANIM_STEP_FRAME, {2, 61, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 62, 0}, 0}, {ANIM_STEP_FRAME, {2, 63, 0}, 0}, {ANIM_STEP_FRAME, {2, 64, 0}, 0}, {ANIM_STEP_FRAME, {2, 65, 0}, 0}, {ANIM_STEP_FRAME, {2, 66, 0}, 0}, {ANIM_STEP_FRAME, {2, 67, 0}, 0}, {ANIM_STEP_FRAME, {2, 68, 0}, 0}, {ANIM_STEP_FRAME, {2, 69, 0}, 0}, {ANIM_STEP_FRAME, {2, 70, 0}, 0}, {ANIM_STEP_FRAME, {2, 71, 0}, 0}, {ANIM_STEP_FRAME, {2, 72, 0}, 0}, {ANIM_STEP_FRAME, {5, 72, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC074..0x800FC14F */
static const ANIM_COMMAND attack_prepare_animation1[27] = {
    {ANIM_FLAGS_CLEAR, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {2, 74, 0}, 0}, {ANIM_SOUND_POSITIONAL, {86, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 75, 0}, 0}, {ANIM_STEP_FRAME, {2, 76, 0}, 0}, {ANIM_STEP_FRAME, {2, 77, 0}, 0}, {ANIM_STEP_FRAME, {2, 78, 0}, 0}, {ANIM_STEP_FRAME, {2, 79, 0}, 0}, {ANIM_STEP_FRAME, {2, 80, 0}, 0}, {ANIM_STEP_FRAME, {2, 81, 0}, 0}, {ANIM_STEP_FRAME, {2, 82, 0}, 0}, {ANIM_STEP_FRAME, {2, 83, 0}, 0}, {ANIM_STEP_FRAME, {2, 84, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 85, 0}, 0}, {ANIM_STEP_FRAME, {2, 86, 0}, 0}, {ANIM_STEP_FRAME, {2, 87, 0}, 0}, {ANIM_STEP_FRAME, {2, 88, 0}, 0}, {ANIM_STEP_FRAME, {2, 89, 0}, 0}, {ANIM_STEP_FRAME, {2, 90, 0}, 0}, {ANIM_STEP_FRAME, {2, 91, 0}, 0}, {ANIM_STEP_FRAME, {2, 92, 0}, 0}, {ANIM_STEP_FRAME, {2, 93, 0}, 0}, {ANIM_STEP_FRAME, {2, 94, 0}, 0}, {ANIM_STEP_FRAME, {2, 95, 0}, 0}, {ANIM_STEP_FRAME, {5, 72, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC150..0x800FC1CB */
static const ANIM_COMMAND idle_animation1[15] = {
    {ANIM_SOUND_POSITIONAL, {87, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 245, 0}, 0}, {ANIM_STEP_FRAME, {2, 246, 0}, 0}, {ANIM_STEP_FRAME, {2, 247, 0}, 0}, {ANIM_STEP_FRAME, {2, 248, 0}, 0}, {ANIM_STEP_FRAME, {2, 249, 0}, 0}, {ANIM_STEP_FRAME, {2, 250, 0}, 0}, {ANIM_FLAGS_SET, {1, 0, 0}, 0}, {ANIM_STEP_FRAME, {2, 251, 0}, 0}, {ANIM_STEP_FRAME, {2, 252, 0}, 0}, {ANIM_STEP_FRAME, {2, 253, 0}, 0}, {ANIM_STEP_FRAME, {2, 254, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 255, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC1CC..0x800FC363 */
static const ANIM_COMMAND attack_animation0[43] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_SOUND_POSITIONAL, {91, 200, 0}, 0}, {ANIM_FRAME_RANGE, {199, 229, 2}, 0}, {ANIM_FLAGS_SET, {2, 0, 0}, 0},       {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0},
    {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0},     {ANIM_RESET_FRAME, {0, 0, 0}, 0},     {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {230, 233, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FLAGS_CLEAR, {2, 0, 0}, 0},     {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {239, 244, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_STEP_FRAME, {5, 72, 0}, 0},     {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC364..0x800FC38F */
static const ANIM_COMMAND death_interrupt_animation[6] = {
    {ANIM_FLAGS_CLEAR, {2, 0, 0}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {239, 244, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_STEP_FRAME, {1, 72, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC390..0x800FC55F */
static const ANIM_COMMAND turn_left_animation[57] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0},  {ANIM_SOUND_POSITIONAL, {86, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 98, 0}, 0},  {ANIM_STEP_FRAME, {2, 99, 0}, 0},         {ANIM_STEP_FRAME, {2, 100, 0}, 0}, {ANIM_STEP_FRAME, {2, 101, 0}, 0}, {ANIM_STEP_FRAME, {2, 102, 0}, 0}, {ANIM_STEP_FRAME, {2, 103, 0}, 0}, {ANIM_STEP_FRAME, {2, 104, 0}, 0}, {ANIM_STEP_FRAME, {2, 105, 0}, 0},        {ANIM_STEP_FRAME, {2, 106, 0}, 0}, {ANIM_STEP_FRAME, {2, 107, 0}, 0}, {ANIM_FLAGS_SET, {4, 0, 0}, 0},    {ANIM_STEP_FRAME, {2, 108, 0}, 0}, {ANIM_STEP_FRAME, {2, 109, 0}, 0}, {ANIM_STEP_FRAME, {2, 110, 0}, 0}, {ANIM_STEP_FRAME, {2, 111, 0}, 0}, {ANIM_STEP_FRAME, {2, 112, 0}, 0}, {ANIM_STEP_FRAME, {2, 113, 0}, 0},
    {ANIM_STEP_FRAME, {2, 114, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 115, 0}, 0}, {ANIM_STEP_FRAME, {2, 116, 0}, 0},        {ANIM_STEP_FRAME, {2, 117, 0}, 0}, {ANIM_STEP_FRAME, {2, 118, 0}, 0}, {ANIM_STEP_FRAME, {2, 119, 0}, 0}, {ANIM_STEP_FRAME, {2, 120, 0}, 0}, {ANIM_STEP_FRAME, {2, 121, 0}, 0}, {ANIM_SOUND_POSITIONAL, {87, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 122, 0}, 0}, {ANIM_STEP_FRAME, {2, 123, 0}, 0}, {ANIM_STEP_FRAME, {2, 124, 0}, 0}, {ANIM_STEP_FRAME, {2, 125, 0}, 0}, {ANIM_STEP_FRAME, {2, 126, 0}, 0}, {ANIM_STEP_FRAME, {2, 127, 0}, 0}, {ANIM_FLAGS_CLEAR, {4, 0, 0}, 0},  {ANIM_STEP_FRAME, {2, 128, 0}, 0}, {ANIM_STEP_FRAME, {2, 129, 0}, 0},
    {ANIM_STEP_FRAME, {2, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 131, 0}, 0},        {ANIM_STEP_FRAME, {2, 132, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 133, 0}, 0}, {ANIM_STEP_FRAME, {2, 134, 0}, 0}, {ANIM_STEP_FRAME, {2, 135, 0}, 0}, {ANIM_STEP_FRAME, {2, 136, 0}, 0}, {ANIM_STEP_FRAME, {2, 137, 0}, 0}, {ANIM_STEP_FRAME, {2, 138, 0}, 0},        {ANIM_STEP_FRAME, {2, 139, 0}, 0}, {ANIM_STEP_FRAME, {2, 140, 0}, 0}, {ANIM_STEP_FRAME, {2, 141, 0}, 0}, {ANIM_STEP_FRAME, {2, 142, 0}, 0}, {ANIM_STEP_FRAME, {2, 143, 0}, 0}, {ANIM_STEP_FRAME, {2, 144, 0}, 0}, {ANIM_STEP_FRAME, {1, 145, 0}, 0}, {ANIM_STEP_FRAME, {1, 72, 0}, 0},  {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC560..0x800FC72F */
static const ANIM_COMMAND turn_right_animation[57] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0},  {ANIM_SOUND_POSITIONAL, {86, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 148, 0}, 0}, {ANIM_STEP_FRAME, {2, 149, 0}, 0},        {ANIM_STEP_FRAME, {2, 150, 0}, 0}, {ANIM_STEP_FRAME, {2, 151, 0}, 0}, {ANIM_STEP_FRAME, {2, 152, 0}, 0}, {ANIM_STEP_FRAME, {2, 153, 0}, 0}, {ANIM_STEP_FRAME, {2, 154, 0}, 0}, {ANIM_STEP_FRAME, {2, 155, 0}, 0},        {ANIM_STEP_FRAME, {2, 156, 0}, 0}, {ANIM_STEP_FRAME, {2, 157, 0}, 0}, {ANIM_FLAGS_SET, {4, 0, 0}, 0},    {ANIM_STEP_FRAME, {2, 158, 0}, 0}, {ANIM_STEP_FRAME, {2, 159, 0}, 0}, {ANIM_STEP_FRAME, {2, 160, 0}, 0}, {ANIM_STEP_FRAME, {2, 161, 0}, 0}, {ANIM_STEP_FRAME, {2, 162, 0}, 0}, {ANIM_STEP_FRAME, {2, 163, 0}, 0},
    {ANIM_STEP_FRAME, {2, 164, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 165, 0}, 0}, {ANIM_STEP_FRAME, {2, 166, 0}, 0},        {ANIM_STEP_FRAME, {2, 167, 0}, 0}, {ANIM_STEP_FRAME, {2, 168, 0}, 0}, {ANIM_STEP_FRAME, {2, 169, 0}, 0}, {ANIM_STEP_FRAME, {2, 170, 0}, 0}, {ANIM_STEP_FRAME, {2, 171, 0}, 0}, {ANIM_SOUND_POSITIONAL, {87, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 172, 0}, 0}, {ANIM_STEP_FRAME, {2, 173, 0}, 0}, {ANIM_STEP_FRAME, {2, 174, 0}, 0}, {ANIM_STEP_FRAME, {2, 175, 0}, 0}, {ANIM_STEP_FRAME, {2, 176, 0}, 0}, {ANIM_STEP_FRAME, {2, 177, 0}, 0}, {ANIM_FLAGS_CLEAR, {4, 0, 0}, 0},  {ANIM_STEP_FRAME, {2, 178, 0}, 0}, {ANIM_STEP_FRAME, {2, 179, 0}, 0},
    {ANIM_STEP_FRAME, {2, 180, 0}, 0}, {ANIM_STEP_FRAME, {2, 181, 0}, 0},        {ANIM_STEP_FRAME, {2, 182, 0}, 0}, {ANIM_SOUND_POSITIONAL, {85, 130, 0}, 0}, {ANIM_STEP_FRAME, {2, 183, 0}, 0}, {ANIM_STEP_FRAME, {2, 184, 0}, 0}, {ANIM_STEP_FRAME, {2, 185, 0}, 0}, {ANIM_STEP_FRAME, {2, 186, 0}, 0}, {ANIM_STEP_FRAME, {2, 187, 0}, 0}, {ANIM_STEP_FRAME, {2, 188, 0}, 0},        {ANIM_STEP_FRAME, {2, 189, 0}, 0}, {ANIM_STEP_FRAME, {2, 190, 0}, 0}, {ANIM_STEP_FRAME, {2, 191, 0}, 0}, {ANIM_STEP_FRAME, {2, 192, 0}, 0}, {ANIM_STEP_FRAME, {2, 193, 0}, 0}, {ANIM_STEP_FRAME, {2, 194, 0}, 0}, {ANIM_STEP_FRAME, {1, 195, 0}, 0}, {ANIM_STEP_FRAME, {1, 72, 0}, 0},  {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC730..0x800FC7AB */
static const ANIM_COMMAND attack_animation1[13] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_SOUND_POSITIONAL, {92, 150, 0}, 0}, {ANIM_FRAME_RANGE, {259, 273, 2}, 0}, {ANIM_SOUND_POSITIONAL, {93, 200, 0}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {274, 281, 2}, 0}, {ANIM_STEP_FRAME, {25, 281, 0}, 0}, {ANIM_SOUND_POSITIONAL, {93, 200, 0}, 0}, {ANIM_STEP_FRAME, {5, 283, 0}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {283, 291, 2}, 0}, {ANIM_STEP_FRAME, {9999, 291, 0}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC7AC..0x800FC7FB */
static const ANIM_COMMAND attack_animation2[9] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_SOUND_POSITIONAL, {90, 200, 0}, 0}, {ANIM_FRAME_RANGE, {312, 328, 2}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_SOUND_POSITIONAL, {88, 200, 0}, 0}, {ANIM_STEP_FRAME, {30, 328, 0}, 0}, {ANIM_RESET_FRAME, {0, 0, 0}, 0}, {ANIM_FRAME_RANGE, {329, 334, 2}, 0}, {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC7FC..0x800FC813 */
static const ANIM_COMMAND landing_animation[3] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0},
    {ANIM_FRAME_RANGE, {292, 310, 2}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC814..0x800FC833 */
static const ANIM_COMMAND death_animation[4] = {
    {ANIM_RESET_FRAME, {0, 0, 0}, 0},
    {ANIM_FRAME_RANGE, {336, 464, 2}, 0},
    {ANIM_STEP_FRAME, {30000, 464, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC848..0x800FC85B */
static const ANIM_COMMAND trail_animation[3] = {
    {ANIM_TRANSLUCENCY, {1, 0, 0}, 0},
    {ANIM_STEP_FRAME, {32, 186369, 0}, 0},
    {ANIM_STOP, {0, 0, 0}, 0},
};

/* Original data: COMMON0/BIGROB.BIN:0x800FBF50..0x800FBF73 */
static const uint8 model_offsets[36] = {
    23, 22, 4, 13, 12, 6, 14, 11, 10, 24, 7, 5, 21, 9, 3, 2, 1, 0, 25, 26, 8, 20, 19, 17, 18, 16, 15, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

/* Original data: COMMON0/BIGROB.BIN:0x800FC834..0x800FC845 */
static const sint16 drop_types[9] = {
    11, 20, 20, 20, 19, 138, 140, 136, 21,
};

/* Variables. */
/* Original scripts are typed constants; AIRSHIP/RB.POD owns the pose bytes */

MODEL_POSE_RESOURCE g_mechanoid_initial_pose;

/* Functions. */

/* Exact 0x800FB018..0x800FB08C. */
/* Original: BIGROB:BIGROB_800FB018. */
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

/* Original: BIGROB:BIGROB_800FBD30. */
static void mechanoid_damage(void *raw_boss, void *src)
{
    MECHANOID_ACTOR *boss = (MECHANOID_ACTOR *)raw_boss;
    if (object_damage_apply(boss, src))
    {
        boss->col.receives_mask = 0;
        boss->phase = 1;
    }
}

/* Original: BIGROB:BIGROB_800FBD6C. */
static void mechanoid_damage_player_on_contact(void *raw_boss, void *src)
{
    MECHANOID_ACTOR *boss = (MECHANOID_ACTOR *)raw_boss;
    player_apply_damage(0, g_player);
}

/* Original: BIGROB:BIGROB_800FBD94. */
static void mechanoid_projectile_expand_col(void *raw_o)
{
    SPRITE *o = (SPRITE *)raw_o;
    o->col.box_x -= 0x300;
    o->col.box_y -= 0x300;
    o->col.box_z -= 0x300;
    o->col.width += 0x300;
    o->col.height += 0x300;
    o->col.depth += 0x300;
}

/* Original: BIGROB:BIGROB_800FBDE0. */
static void mechanoid_projectile_retarget_player(void *raw_o)
{
    SPRITE *o = (SPRITE *)raw_o;
    FIXED_DDA_LINE line;
    o->velocity_y = -o->velocity_y;
    fixed_dda_initialize(o->col.x, o->col.y, o->col.z, g_player_world.x, g_player_world.y, g_player_world.z, &line);
    o->velocity_x = 3 * div_256_trunc(line.step.x);
    o->velocity_z = 3 * div_256_trunc(line.step.z);
}

/* Original: BIGROB:BIGROB_800FBE94. */
static void mechanoid_projectile_update_trail(void *raw_o)
{
    SPRITE *o = (SPRITE *)raw_o;
    if (o->velocity_y > 0x600)
        o->velocity_y = 0x600;
    if ((g_render_state.frame & 3) == 0)
    {
        SPRITE *e;
        o->light_delta = (-8 * random_range(8));
        e = world_spr_create(o->col.x, o->col.y, o->col.z, trail_animation);
        e->light_step = -4;
        e->scale_step = -0x80;
    }
}

static void mechanoid_attack_projectile_create(MECHANOID_ACTOR *b)
{
    SPRITE *p = world_spr_create(b->nodes[35].world_x, b->nodes[35].world_y, b->nodes[35].world_z, projectile_animation);
    p->frame_callback = mechanoid_projectile_update_trail;
    p->col_callback = mechanoid_projectile_retarget_player;
    p->acceleration_y = 0x40;
    p->rotation_step_x = 0x20;
    p->col.player_contact_callback = player_apply_damage;
    p->cull_distance = 0x3e800;
    p->col.receives_mask = 2;
    col_box_set(p, 0x40, 0x40, 0x40);
    p->col.box_y += 0x2000;
}

static void mechanoid_render(MECHANOID_ACTOR *b)
{
    b->nodes[0].translation_x = ((uint32)b->col.x - g_camera.world_x);
    b->nodes[0].translation_y = ((uint32)b->col.y - g_camera.world_y);
    b->nodes[0].translation_z = ((uint32)b->col.z - g_camera.world_z);
    g_model_draw.depth_bucket = 0x43f - div_256_trunc(((uint32)g_map_render.depth_anchor_z - b->col.z));
    g_model_draw.target_frame = 0;
    g_model_draw.world_z = b->col.z;
    model_render_begin();
    if (b->col.flash_clut_ticks)
    {
        b->col.flash_clut_ticks -= 1;
        g_model_draw.clut_override = g_spr_pal.hit_flash;
    }
    model_render_node(&b->nodes[0], &g_model_identity);
    g_model_draw.clut_override = 0;
    model_render_end();
}

/* Exact control-flow translation of 0x800FB090..0x800FBD30. */
/* Original: BIGROB:BIGROB_800FB090. */
static void mechanoid_update(void *raw_b)
{
    MECHANOID_ACTOR *b = (MECHANOID_ACTOR *)raw_b;
    MODEL_NODE *bone;
    ANIM *c = &b->animation;
    const ANIM_COMMAND *start;
    FIXED_DDA_LINE line;
    sint32 angle, cur, turn;
    if ((g_render_state.frame & 0x1ff) == 0)
    {
        const sint16 *types = drop_types;
        PLAYER_ACTION *item = player_action_object_create(g_player_world.x - 0x4000, g_player_world.y - 0xc800, g_player_world.z, types[random_range(8)]);
        mechanoid_clamp_to_arena(b, &item->col.x, &item->col.z);
        item->lifetime = 0xc8;
    }
    animation_update(c);
    start = c->start;
    if (start == attack_animation0 && (b->animation.flags & 2) && b->phase == 1)
        animation_start(c, death_interrupt_animation);
    if (b->phase == 1 && b->animation.frame == 0x48)
    {
        b->phase = 2;
        animation_start(c, death_animation);
        sound_voice_stop(&b->sound_handle);
    }
    if (b->phase && (g_render_state.frame & 3) == 0)
    {
        const ANIM_COMMAND *dust;
        sound_play_positional(random_range(1) ? 0x34 : 0x35, 0, 0x7f, b->col.x, b->col.y, b->col.z);
        dust = random_range(2) ? g_dust_animation1 : g_dust_animation0;
        bone = &b->nodes[1 + random_range(35)];
        SPRITE *spr = world_spr_create(bone->world_x, bone->world_y, bone->world_z, dust);
        spr->light_step = -2;
        spr->velocity_x = random_range(1) * 2 - 1;
        spr->force_light = 1;
        spr->ot_bucket = (0x480 - div_256_trunc(g_map_render.depth_anchor_z + 0x2000 - bone->world_z));
    }
    if (b->animation.frame == 0x1d0)
        g_mission.counts[MISSION_COUNT_MECHANOID_DONE] = 1;
    model_pose_apply(&g_mechanoid_initial_pose, b->node_table, 36, b->animation.frame);
    b->nodes[20].translation_y += 0xe00;
    if (!b->moving)
    {
        if (b->target_x - 0x1000 < b->col.x && b->col.x < b->target_x + 0x1000 && b->target_z - 0x1000 < b->col.z && b->col.z < b->target_z + 0x1000)
            b->target_reached = 1;
        if (b->target_reached)
        {
            if (b->animation.frame == 2)
                animation_start(c, attack_prepare_animation0);
            if (b->animation.frame == 0x1b)
                animation_start(c, attack_prepare_animation1);
        }
        start = c->start;
        if (c->next == 0 && (start == attack_prepare_animation0 || start == attack_prepare_animation1))
        {
            if (b->attack_variant == 0)
                animation_start(c, attack_animation0);
            if (b->attack_variant == 1)
                animation_start(c, attack_animation1);
            if (b->attack_variant == 2)
                animation_start(c, attack_animation2);
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
        b->col.x = g_player_world.x;
        b->col.z = g_player_world.z;
        mechanoid_clamp_to_arena(b, &b->col.x, &b->col.z);
    }
    if ((b->animation.flags & 2) && b->sound_handle == -1)
        b->sound_handle = sound_play_positional(0x54, 0, 0xc8, b->col.x, b->col.y, b->col.z);
    if (b->animation.frame == 0x148 && b->animation.delay == 0xf)
        mechanoid_attack_projectile_create(b);
    if (b->attack_sequence_active && c->next == 0)
    {
        sound_voice_stop(&b->sound_handle);
        b->attack_sequence_active = 0;
        b->target_reached = 0;
        b->moving = 1;
        b->target_x = g_camera.world_x;
        b->target_z = g_camera.world_z + 0x16c00;
        mechanoid_clamp_to_arena(b, &b->target_x, &b->target_z);
        fixed_dda_initialize(b->col.x, b->col.y, b->col.z, b->target_x, b->col.y, b->target_z, &line);
        angle = (fixed_angle_from_vector(div_4096_trunc(line.step.x), div_4096_trunc(line.step.z)) * 4) & 0xfff0;
        b->target_yaw = angle;
        b->velocity_x = ((uint32)line.step.x + (line.step.x / 2));
        b->velocity_z = ((uint32)line.step.z + (line.step.z / 2));
        cur = b->nodes[0].rotation_y;
        turn = angle < cur ? -24 : 24;
        if (abs(cur - angle) >= 0x801)
            turn = -turn;
        b->yaw_step = turn;
        animation_start(c, turn < 0 ? turn_left_animation : turn_right_animation);
        if (cur == angle)
            b->moving = 0;
    }
    if (b->moving && b->nodes[0].rotation_y != b->target_yaw && (b->animation.flags & 4))
        b->nodes[0].rotation_y = angle_approach_wrapped(b->target_yaw, 0x38, b->nodes[0].rotation_y);
    else if (!b->moving || b->nodes[0].rotation_y == b->target_yaw)
    {
        b->moving = 0;
        if (c->start != idle_animation1 && c->next == 0)
            animation_start(c, idle_animation1);
        if (c->next == 0 && c->start == idle_animation1)
            animation_start(c, idle_animation0);
    }
    if (b->animation.flags & 2)
    {
        /* BIGROB:800FB828 calls SLES:800A4E1C; delay slot sets divisor=1. */
        angle = ((player_angle_from_point(b->nodes[34].world_x, b->nodes[34].world_z, 1) * 4 - b->nodes[0].rotation_y) & 0xfff) + b->aim_jitter;
        angle &= 0xfff;
        if ((g_render_state.frame & 7) == 0)
            b->aim_jitter = (20 * (random_range(0x20) - 0x10));
        b->aim_yaw = angle_approach_wrapped(angle, 0x20, b->aim_yaw);
    }
    else
        b->aim_yaw = angle_approach_wrapped(0, 0x20, b->aim_yaw);
    b->nodes[31].rotation_y = b->aim_yaw;
    if (b->animation.flags & 1)
    {
        b->col.x = b->col.x + (uint32)div_256_trunc(b->velocity_x);
        b->col.z = b->col.z + (uint32)div_256_trunc(b->velocity_z);
    }
    b->col.y = (uint32)b->col.y + b->velocity_y;
    if (b->col.y > b->base_y)
    {
        b->col.y = b->base_y;
        b->velocity_y = 0;
        animation_start(c, landing_animation);
        disp_shake_start();
        sound_play_nonpositional(0x55, 0, 0x96);
    }
    mechanoid_render(b);
    if (b->animation.flags & 2)
    {
        if ((g_render_state.frame & 7) == 0)
        {
            fixed_dda_initialize(b->nodes[33].world_x, b->nodes[33].world_y, b->nodes[33].world_z, b->nodes[34].world_x, b->nodes[34].world_y, b->nodes[34].world_z, &line);
            SPRITE *spr = world_spr_create(b->nodes[34].world_x, b->nodes[34].world_y, b->nodes[34].world_z, spray_animation);
            spr->velocity_x = 5 * div_256_trunc(line.step.x);
            spr->velocity_y = 5 * div_256_trunc(line.step.y) + 0x100;
            spr->velocity_z = 5 * div_256_trunc(line.step.z);
            spr->light_step = -4;
            spr->col.player_contact_callback = player_apply_damage;
            spr->col.hit.damage = 0x80;
            spr->col.object_type = 0;
            spr->col.sends_mask |= 1;
            spr->col.receives_mask |= 4;
            spr->col.receives_mask = 2;
            col_box_set(spr, 0x20, 0x20, 0x20);
            spr->scale_x = 0x200;
            spr->scale_step = 0x80;
            spr->frame_callback = mechanoid_projectile_expand_col;
            spr->col.box_y += 0x1000;
        }
        g_next_frame_object->light_delta = -0x28;
        g_next_frame_object->force_light = 1;
        bone = &b->nodes[35];
        render_world_spr(0x2e800, bone->world_x, bone->world_y, bone->world_z, 0, 0x1000, 0x1000, &b->col.prim, 0, 2, 0, (g_render_state.frame << 2) & 0x3ff);
    }
    if (b->animation.frame == 2 && b->animation.delay == 1)
    {
        sound_play_positional(0x55, -4, 0x82, b->col.x, b->col.y, b->col.z);
        sound_play_positional(0x56, -7, 0x82, b->col.x, b->col.y, b->col.z);
    }
    if (b->animation.frame == 0x1a && b->animation.delay == 1)
    {
        sound_play_positional(0x55, -4, 0x82, b->col.x, b->col.y, b->col.z);
        sound_play_positional(0x57, -7, 0x82, b->col.x, b->col.y, b->col.z);
    }
    hud_bar_render(&g_boss_health_bar, b->col.hit.health, 0, 0);
}

/* Original range: overlay address 0x800FADD4 */ /* Original: BIGROB:BIGROB_800FADD4. */
void mechanoid_create(sint32 x, sint32 y, sint32 z)
{
    MECHANOID_ACTOR *b;
    const uint8 *map;
    sint32 n;
    b = (MECHANOID_ACTOR *)object_create(sizeof(*b), mechanoid_update);
    setPolyFT4(&b->col.prim);
    setShadeTex(&b->col.prim, 0);
    setSemiTrans(&b->col.prim, 1);
    b->col.x = x;
    b->col.y = y;
    b->col.z = z;
    b->base_y = y;
    map = model_offsets;
    for (n = 0; n < 36; n++)
    {
        MODEL_NODE *node = &b->nodes[n + 1];
        b->node_table[n] = node;
        if (map[n])
            node->model_id = g_mechanoid_model_base + map[n];
        node->pose_enabled = 0x3f;
    }
    model_pose_init(&g_mechanoid_initial_pose, b->node_table, 36, 1);
    model_attach_child(&b->nodes[29], &b->nodes[0]);
    animation_start(&b->animation, idle_animation0);
    b->target_x = x;
    b->target_z = z;
    b->col.hit_callback_kind = COLLISION_CALLBACK_NOTIFY;
    b->col.hit_callback = mechanoid_damage;
    b->col.player_contact_callback = mechanoid_damage_player_on_contact;
    b->col.receives_mask = 0x43;
    b->col.hit.health = 3500;
    col_box_set(b, 0x50, 0x100, 0x50);
    b->col.box_y += 0x10000;
    hud_bar_initialize(&g_boss_health_bar, -0x28, -0x60, 0x50, 3500);
    b->animation.owner = b;
    b->sound_handle = -1;
    b->min_x = x - 0x16000;
    b->max_x = x + 0x12000;
    b->min_z = z - 0x8000;
    b->max_z = z + 0x8000;
    b->nodes[0].rotation_y = 0x800;
}
