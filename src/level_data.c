#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "render.h"
#include "native_config.h"
#include "game.h"
#include "cc_archive.h"
#include "camera.h"
#include "effect_update.h"
#include "map_render.h"
#include "mechanism.h"
#include "mission.h"
#include "level_data.h"
#include "v2.h"
#include "truck.h"
#include "tank.h"
#include "scuba_player.h"
#include "resque_tanya.h"
#include "radar_death.h"
#include "mini_gyro.h"
#include "gyro.h"
#include "gunship.h"
#include "map.h"
#include "mechanoid.h"
#include "model.h"
#include "object.h"
#include "game_file.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "runtime_heap.h"
#include "sprite.h"

MAP_INPUT g_stage_map_input;
/* Original: DAT_800D3E90. */
void *g_stage_archive;
sint32 g_stage_archive_size;

/* Original: SLES_004.74 DAT_800CBB08, 46 resource IDs indexed by TEXINFO bytes.
 * Former native duplicates: hq_material_resources and hq_tx_resource. */
const uint32 g_hq_material_resources[46] = {0x06800, 0x06c00, 0x07000, 0x07400, 0x07800, 0x07c00, 0x08000, 0x29c00, 0x2a000, 0x2a400, 0x2a800, 0x2ac00, 0x2b000, 0x2b400, 0x18c00, 0x19000, 0x19400, 0x19800, 0x19c00, 0x1a000, 0x1a400, 0x1a800, 0x1ac00, 0x1b000, 0x1b400, 0x1b800, 0x1bc00, 0x1c000, 0x1fc00, 0x20000, 0x20400, 0x20800, 0x20c00, 0x21000, 0x21400, 0x21800, 0x21c00, 0x22000, 0x22400, 0x22800, 0x01400, 0x01800, 0x01c00, 0x2f000, 0x12800, 0x12c00};

const STAGE_ENV *g_stage_env;

/* Stage file spans and pose views borrow the current game heap */
void level_data_heap_reset(void)
{
    MODEL_POSE_RESOURCE empty = {0};
    g_stage_archive = 0;
    g_stage_archive_size = 0;
    memset(&g_stage_map_input, 0, sizeof(g_stage_map_input));
    g_gun_emplacement_initial_pose = empty;
    g_radar_initial_pose = empty;
    g_mini_gyro_initial_pose = empty;
    g_mini_gyro_type_117_initial_pose = empty;
    g_gunship_initial_pose = empty;
    g_v2_rocket_init_pose = empty;
    g_scuba_tug_initial_pose = empty;
    g_mini_sub_initial_pose = empty;
}

/* Original loader produces up to63 material entries followed by a sentinel */
/* Original: DAT_800EA198. */
sint32 g_material_resource_table[64];

/* Original data: SLES_004.74:0x800CC948..0x800CC95B */
static const RESOURCE_COMMAND guard_moves_resources[3] = {
    {RESOURCE_FILE, "MISS0\\G1MOVES.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x1f000, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CC95C..0x800CC98F */
static const RESOURCE_COMMAND truck_resources[4] = {
    {RESOURCE_ARCHIVE, "COMMON0\\TRUCK.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 50, 52},
    {RESOURCE_MODELS, "COMMON0\\TRUCK.MOD", 0, &g_truck_model_base, 2, {0x1e400, 0x1e800, 0x1ec00}, 0, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CC990..0x800CC9BB */
static const RESOURCE_COMMAND robot_resources[3] = {
    {RESOURCE_OVERLAY, "COMMON0\\BIGROB.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODELS, "COMMON0\\ROBOT.MOD", 0, &g_mechanoid_model_base, 28, {0x15800, 0x15800, 0x15800}, 0, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CC9BC..0x800CC9F7 */
static const RESOURCE_COMMAND gyro_resources[5] = {
    {RESOURCE_OVERLAY, "COMMON0\\GYRO.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_ARCHIVE, "COMMON0\\GYROTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 56, 56}, {RESOURCE_MODELS, "COMMON0\\GYRO.MOD", 0, &g_gyro_model_base, 3, {0x15800, 0x15800, 0x15800}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CC9F8..0x800CCA2F */
static const RESOURCE_COMMAND gun_emplacement_resources[5] = {
    {RESOURCE_ARCHIVE, "COMMON0\\GUNEMPTX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 57, 57}, {RESOURCE_FILE, "COMMON0\\GUNEMP.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_gun_emplacement_model_base, 4, {0x15800, 0x15800, 0x15800}, &g_gun_emplacement_initial_pose, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCA30..0x800CCA67 */
static const RESOURCE_COMMAND radar_resources[5] = {
    {RESOURCE_ARCHIVE, "COMMON0\\RADARTX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 62, 62}, {RESOURCE_FILE, "COMMON0\\RADAR.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_radar_vehicle_model_base, 4, {0x23c00, 0x23c00, 0x23c00}, &g_radar_initial_pose, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCA68..0x800CCA9F */
static const RESOURCE_COMMAND tank_resources[5] = {
    {RESOURCE_ARCHIVE, "COMMON0\\TANKTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 58, 59}, {RESOURCE_FILE, "COMMON0\\TANK.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_tank_model_base, 3, {0x22c00, 0x23000, 0x22c00}, &g_tank_initial_pose, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCAA0..0x800CCAB3 */
static const RESOURCE_COMMAND uzi_guard_resources[3] = {
    {RESOURCE_FILE, "MISS0\\E1UZI.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x13000, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCAB4..0x800CCAC7 */
static const RESOURCE_COMMAND mortar_guard_resources[3] = {
    {RESOURCE_FILE, "MISS0\\E1MORTAR.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x400, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCAC8..0x800CCADB */
static const RESOURCE_COMMAND flame_guard_resources[3] = {
    {RESOURCE_FILE, "MISS0\\FLAMEMVE.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x23400, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCADC..0x800CCAEF */
static const RESOURCE_COMMAND guard2_resources[3] = {
    {RESOURCE_FILE, "MISS0\\E2ALL.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x15400, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCAF0..0x800CCB03 */
static const RESOURCE_COMMAND bomber_resources[3] = {
    {RESOURCE_FILE, "MISS0\\BMANMOVE.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x27c00, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCB04..0x800CCB17 */
static const RESOURCE_COMMAND tanya_moves_resources[3] = {
    {RESOURCE_FILE, "MISS0\\TNYAMVES.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x17c00, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCB18..0x800CCB4F */
static const RESOURCE_COMMAND v2_resources[5] = {
    {RESOURCE_ARCHIVE, "COMMON0\\V2TX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 66, 66}, {RESOURCE_FILE, "COMMON0\\V2.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_v2_rocket_model_base, 1, {0x28800, 0x0, 0x0}, &g_v2_rocket_init_pose, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCB50..0x800CCB63 */
static const RESOURCE_COMMAND water_expl_resources[3] = {
    {RESOURCE_FILE, "DOCKS\\WATEXPFL.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x1f800, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCB64..0x800CCB9B */
static const RESOURCE_COMMAND mini_gyro_resources[5] = {
    {RESOURCE_ARCHIVE, "COMMON0\\MINIGTX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 67, 67}, {RESOURCE_FILE, "COMMON0\\MINGYRO.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_mini_gyro_model_base, 1, {0x29400, 0x0, 0x0}, &g_mini_gyro_initial_pose, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCB9C..0x800CCBC3 */
static const RESOURCE_COMMAND mini_sub_resources[3] = {
    {RESOURCE_FILE, "WATER\\MINISUB.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_mini_sub_model_base, 4, {0x2c000, 0x0, 0x0}, &g_mini_sub_initial_pose, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCBC4..0x800CCBEB */
static const RESOURCE_COMMAND depth_charge_resources[3] = {
    {RESOURCE_FILE, "WATER\\DEPTHCHR.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_depth_charge_model_id, 1, {0x2c400, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCBEC..0x800CCC13 */
static const RESOURCE_COMMAND small_ship_resources[3] = {
    {RESOURCE_FILE, "WATER\\SMLSHIP.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_scuba_tug_model_base, 1, {0x2d000, 0x0, 0x0}, &g_scuba_tug_initial_pose, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCC14..0x800CCC27 */
static const RESOURCE_COMMAND enemy_diver_resources[3] = {
    {RESOURCE_FILE, "WATER\\ENMDIVER.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x2d400, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCC28..0x800CCC4F */
static const RESOURCE_COMMAND underwater_gun_resources[3] = {
    {RESOURCE_FILE, "WATER\\UWGUN.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_gun_emplacement_model_base, 3, {0x15800, 0x0, 0x0}, &g_gun_emplacement_initial_pose, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCC50..0x800CCC77 */
static const RESOURCE_COMMAND underwater_base_resources[3] = {
    {RESOURCE_FILE, "WATER\\UWBASE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_mini_gyro_type_117_model_base, 1, {0x2e400, 0x0, 0x0}, &g_mini_gyro_type_117_initial_pose, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCC78..0x800CCCA7 */
static const RESOURCE_COMMAND gunship_resources[4] = {
    {RESOURCE_FILE, "JUNGLE\\GUNSHIP.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_MODEL_ARCHIVE, 0, 0, &g_gunship_model_base, 7, {0x1fc00, 0x0, 0x0}, &g_gunship_initial_pose, 0x0, 0, 0},
    {RESOURCE_OVERLAY, "JUNGLE\\GUNSHIP.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCCA8..0x800CCCBB */
static const RESOURCE_COMMAND droid_resources[3] = {
    {RESOURCE_FILE, "IND\\DRODMVES.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x2ec00, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCCBC..0x800CCCCF */
static const RESOURCE_COMMAND mine_guard_resources[3] = {
    {RESOURCE_FILE, "MISS0\\MINEMVES.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x1d800, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCCD0..0x800CCCE3 */
static const RESOURCE_COMMAND assassin_resources[3] = {
    {RESOURCE_FILE, "MISS0\\ASASMVES.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x30000, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCCE4..0x800CCCF7 */
static const RESOURCE_COMMAND soldier_resources[3] = {
    {RESOURCE_FILE, "MISS0\\S1MOVES.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
    {RESOURCE_TEXTURE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x15c00, 0, 0},
    {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCCF8..0x800CCD27 */
static const RESOURCE_COMMAND stage_resources0[7] = {
    {RESOURCE_ARCHIVE, "DOCKS\\DOCKS1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 55, 55}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCD28..0x800CCD7F */
static const RESOURCE_COMMAND stage_resources1[12] = {
    {RESOURCE_OVERLAY, "COMMON0\\TRUCK.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, truck_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_ARCHIVE, "DOCKS\\DOCKS1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 54, 54}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mine_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCD80..0x800CCDAF */
static const RESOURCE_COMMAND stage_resources2[7] = {
    {RESOURCE_ARCHIVE, "DOCKS\\DOCKS1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 54, 54}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCDB0..0x800CCDDF */
static const RESOURCE_COMMAND stage_resources3[7] = {
    {RESOURCE_INCLUDE, 0, guard2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_ARCHIVE, "AIRSHIP\\AIRSHIP.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "AIRSHIP\\AIRSHIP.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCDE0..0x800CCE07 */
static const RESOURCE_COMMAND stage_resources4[6] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\ARENA.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 81, 82}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 48, 49}, {RESOURCE_INCLUDE, 0, robot_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCE08..0x800CCE4F */
static const RESOURCE_COMMAND stage_resources5[10] = {
    {RESOURCE_ARCHIVE, "DOCKS\\DOCKS1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 84, 84}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, tanya_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCE50..0x800CCE77 */
static const RESOURCE_COMMAND stage_resources6[6] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\CAVES1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, flame_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCE78..0x800CCEB7 */
static const RESOURCE_COMMAND stage_resources7[9] = {
    {RESOURCE_ARCHIVE, "DOCKS\\DOCKS1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 54, 54}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, radar_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCEB8..0x800CCEF7 */
static const RESOURCE_COMMAND stage_resources8[9] = {
    {RESOURCE_ARCHIVE, "DOCKS\\DOCKS1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 60, 60}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gyro_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCEF8..0x800CCF37 */
static const RESOURCE_COMMAND stage_resources9[9] = {
    {RESOURCE_ARCHIVE, "JUNGLE\\JUNGLE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, bomber_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCF38..0x800CCF87 */
static const RESOURCE_COMMAND stage_resources10[11] = {
    {RESOURCE_ARCHIVE, "JUNGLE\\JUNGLE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 28, 28}, {RESOURCE_INCLUDE, 0, gunship_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, flame_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCF88..0x800CCFA7 */
static const RESOURCE_COMMAND stage_resources11[5] = {
    {RESOURCE_ARCHIVE, "HQ\\HQTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 63, 65}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 85, 85}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCFA8..0x800CCFEF */
static const RESOURCE_COMMAND stage_resources12[10] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\AIRTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 1, 1}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, radar_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, v2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, bomber_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "COMMON0\\V2.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CCFF0..0x800CD037 */
static const RESOURCE_COMMAND stage_resources13[10] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\AIRTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 1, 1}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, soldier_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_gyro_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "COMMON0\\V2.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD038..0x800CD08F */
static const RESOURCE_COMMAND stage_resources14[12] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\AIRTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 1, 1}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, tanya_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, radar_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, v2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "COMMON0\\V2.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD090..0x800CD0D7 */
static const RESOURCE_COMMAND stage_resources15[10] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\AIRTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 1, 1}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, radar_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, bomber_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "COMMON0\\V2.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD0D8..0x800CD127 */
static const RESOURCE_COMMAND stage_resources16[11] = {
    {RESOURCE_ARCHIVE, "WATER\\WATER1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 68, 79}, {RESOURCE_OVERLAY, "WATER\\SCUBA.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, depth_charge_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_sub_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, small_ship_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, enemy_diver_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_gun_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_base_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD128..0x800CD167 */
static const RESOURCE_COMMAND stage_resources17[9] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\AIRTEX.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, bomber_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_gyro_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "COMMON0\\V2.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD168..0x800CD1BF */
static const RESOURCE_COMMAND stage_resources18[12] = {
    {RESOURCE_ARCHIVE, "WATER\\WATER1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 69, 79}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 83, 83}, {RESOURCE_OVERLAY, "WATER\\SCUBA.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, depth_charge_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_sub_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, small_ship_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, enemy_diver_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_gun_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_base_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD1C0..0x800CD217 */
static const RESOURCE_COMMAND stage_resources19[12] = {
    {RESOURCE_ARCHIVE, "WATER\\WATER1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 69, 79}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 83, 83}, {RESOURCE_OVERLAY, "WATER\\SCUBA.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, depth_charge_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_sub_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, small_ship_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, enemy_diver_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_gun_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_base_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD218..0x800CD267 */
static const RESOURCE_COMMAND stage_resources20[11] = {
    {RESOURCE_ARCHIVE, "WATER\\WATER1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 68, 79}, {RESOURCE_OVERLAY, "WATER\\SCUBA.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, depth_charge_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_sub_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, small_ship_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, enemy_diver_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_gun_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_base_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD268..0x800CD2BF */
static const RESOURCE_COMMAND stage_resources21[12] = {
    {RESOURCE_ARCHIVE, "WATER\\WATER1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 69, 79}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 83, 83}, {RESOURCE_OVERLAY, "WATER\\SCUBA.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, depth_charge_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mini_sub_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, small_ship_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, enemy_diver_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_gun_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, underwater_base_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD2C0..0x800CD2FF */
static const RESOURCE_COMMAND stage_resources22[9] = {
    {RESOURCE_ARCHIVE, "JUNGLE\\JUNGLE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, flame_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD300..0x800CD33F */
static const RESOURCE_COMMAND stage_resources23[9] = {
    {RESOURCE_ARCHIVE, "JUNGLE\\JUNGLE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, tank_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, flame_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD340..0x800CD37F */
static const RESOURCE_COMMAND stage_resources24[9] = {
    {RESOURCE_ARCHIVE, "JUNGLE\\JUNGLE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, flame_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD380..0x800CD3D7 */
static const RESOURCE_COMMAND stage_resources25[12] = {
    {RESOURCE_ARCHIVE, "JUNGLE\\JUNGLE1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, radar_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, water_expl_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, mortar_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, v2_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, flame_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "COMMON0\\V2.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD3D8..0x800CD3FF */
static const RESOURCE_COMMAND stage_resources26[6] = {
    {RESOURCE_ARCHIVE, "AIRSHIP\\CAVES1.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, guard_moves_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD400..0x800CD437 */
static const RESOURCE_COMMAND stage_resources27[8] = {
    {RESOURCE_ARCHIVE, "IND\\IND.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_OVERLAY, "IND\\INDUSTRY.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, droid_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD438..0x800CD46F */
static const RESOURCE_COMMAND stage_resources28[8] = {
    {RESOURCE_INCLUDE, 0, droid_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, assassin_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_ARCHIVE, "IND\\IND.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_RANGE, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 80, 80}, {RESOURCE_OVERLAY, "IND\\INDUSTRY.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD470..0x800CD49F */
static const RESOURCE_COMMAND stage_resources29[7] = {
    {RESOURCE_ARCHIVE, "IND\\IND.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "IND\\INDUSTRY.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, droid_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, uzi_guard_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD4A0..0x800CD4CF */
static const RESOURCE_COMMAND stage_resources31[7] = {
    {RESOURCE_INCLUDE, 0, droid_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, bomber_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_ARCHIVE, "IND\\IND.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "IND\\INDUSTRY.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800CD4D0..0x800CD4F7 */
static const RESOURCE_COMMAND stage_resources32[6] = {
    {RESOURCE_INCLUDE, 0, droid_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_INCLUDE, 0, gun_emplacement_resources, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_ARCHIVE, "IND\\IND.CC", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_MATERIALS, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_OVERLAY, "IND\\INDUSTRY.BIN", 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0}, {RESOURCE_END, 0, 0, 0, 0, {0x0, 0x0, 0x0}, 0, 0x0, 0, 0},
};

/* Original data: SLES_004.74:0x800C9884..0x800C989D */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands0[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_TARGETS, 1, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_TARGETS, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives0 = {stage_objective_commands0, 7};

/* Original data: SLES_004.74:0x800C98A0..0x800C98B9 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands1[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_NONZERO, 0, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives1 = {stage_objective_commands1, 7};

/* Original data: SLES_004.74:0x800C98BC..0x800C98CD */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands2[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 6, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives2 = {stage_objective_commands2, 5};

/* Original data: SLES_004.74:0x800C98D0..0x800C98E1 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands3[] = {
    {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_TARGETS, 1, 0}, {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_NONZERO, 21, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives3 = {stage_objective_commands3, 5};

/* Original data: SLES_004.74:0x800C98E4..0x800C98ED */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands4[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0},
    {MISSION_OBJECTIVE_COUNT_NONZERO, 22, 0},
    {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives4 = {stage_objective_commands4, 3};

/* Original data: SLES_004.74:0x800C98F0..0x800C990F */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands5[] = {
    {MISSION_OBJECTIVE_TIME_LIMIT, 600, 0}, {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_NONZERO, 2, 0}, {MISSION_OBJECTIVE_NO_TIME_LIMIT, 0, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives5 = {stage_objective_commands5, 9};

/* Original data: SLES_004.74:0x800C9B48..0x800C9B51 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands6[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0},
    {MISSION_OBJECTIVE_COUNT_NONZERO, 2, 0},
    {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives6 = {stage_objective_commands6, 3};

/* Original data: SLES_004.74:0x800C9910..0x800C9921 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands7[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 4, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives7 = {stage_objective_commands7, 5};

/* Original data: SLES_004.74:0x800C9924..0x800C992D */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands8[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0},
    {MISSION_OBJECTIVE_COUNT_ZERO, 7, 0},
    {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives8 = {stage_objective_commands8, 3};

/* Original data: SLES_004.74:0x800C9930..0x800C9943 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands9[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ABOVE, 14, 19}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives9 = {stage_objective_commands9, 5};

/* Original data: SLES_004.74:0x800C9944..0x800C9955 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands10[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 13, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives10 = {stage_objective_commands10, 5};

/* Original data: SLES_004.74:0x800C9958..0x800C9961 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands12[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0},
    {MISSION_OBJECTIVE_COUNT_ZERO, 5, 0},
    {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives12 = {stage_objective_commands12, 3};

/* Original data: SLES_004.74:0x800C9964..0x800C997D */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands13[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 8, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 6, 0}, {MISSION_OBJECTIVE_PRIMARY, 3, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives13 = {stage_objective_commands13, 7};

/* Original data: SLES_004.74:0x800C9980..0x800C998F */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands14[] = {
    {MISSION_OBJECTIVE_TIME_LIMIT, 600, 0}, {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_NONZERO, 2, 0}, {MISSION_OBJECTIVE_TIMER_CHECK, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives14 = {stage_objective_commands14, 5};

/* Original data: SLES_004.74:0x800C9990..0x800C99A1 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands15[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 6, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 4, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives15 = {stage_objective_commands15, 5};

/* Original data: SLES_004.74:0x800C99A4..0x800C99B5 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands16[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 23, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives16 = {stage_objective_commands16, 5};

/* Original data: SLES_004.74:0x800C99B8..0x800C99D9 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands17[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 8, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_TARGETS, 1, 0}, {MISSION_OBJECTIVE_PRIMARY, 3, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives17 = {stage_objective_commands17, 9};

/* Original data: SLES_004.74:0x800C99DC..0x800C99ED */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands18[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 9, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives18 = {stage_objective_commands18, 5};

/* Original data: SLES_004.74:0x800C99F0..0x800C9A03 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands19[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ABOVE, 14, 9}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives19 = {stage_objective_commands19, 5};

/* Original data: SLES_004.74:0x800C9A04..0x800C9A15 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands20[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 10, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives20 = {stage_objective_commands20, 5};

/* Original data: SLES_004.74:0x800C9A18..0x800C9A29 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands21[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 11, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives21 = {stage_objective_commands21, 5};

/* Original data: SLES_004.74:0x800C9A2C..0x800C9A47 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands22[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ABOVE, 14, 59}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives22 = {stage_objective_commands22, 7};

/* Original data: SLES_004.74:0x800C9A48..0x800C9A69 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands23[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 12, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_TARGETS, 1, 0}, {MISSION_OBJECTIVE_PRIMARY, 3, 0}, {MISSION_OBJECTIVE_ACTION, 1, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives23 = {stage_objective_commands23, 9};

/* Original data: SLES_004.74:0x800C9A6C..0x800C9A85 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands24[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 18, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives24 = {stage_objective_commands24, 7};

/* Original data: SLES_004.74:0x800C9A88..0x800C9AA1 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands25[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 5, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_PRIMARY, 3, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives25 = {stage_objective_commands25, 7};

/* Original data: SLES_004.74:0x800C9AA4..0x800C9ABF */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands26[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_TARGETS, 1, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ABOVE, 14, 19}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives26 = {stage_objective_commands26, 7};

/* Original data: SLES_004.74:0x800C9AC0..0x800C9ADB */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands27[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 16, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ABOVE, 20, 29}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives27 = {stage_objective_commands27, 7};

/* Original data: SLES_004.74:0x800C9ADC..0x800C9AE5 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands28[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0},
    {MISSION_OBJECTIVE_ACTION, 0, 0},
    {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives28 = {stage_objective_commands28, 3};

/* Original data: SLES_004.74:0x800C9AE8..0x800C9B03 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands29[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_TARGETS, 0, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ABOVE, 14, 59}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives29 = {stage_objective_commands29, 7};

/* Original data: SLES_004.74:0x800C9B18..0x800C9B31 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands31[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 15, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_SECONDARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 3, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives31 = {stage_objective_commands31, 7};

/* Original data: SLES_004.74:0x800C9B34..0x800C9B45 */
static const MISSION_OBJECTIVE_COMMAND stage_objective_commands32[] = {
    {MISSION_OBJECTIVE_PRIMARY, 1, 0}, {MISSION_OBJECTIVE_COUNT_ZERO, 17, 0}, {MISSION_OBJECTIVE_PRIMARY, 2, 0}, {MISSION_OBJECTIVE_ACTION, 0, 0}, {MISSION_OBJECTIVE_END, 0, 0},
};
static const MISSION_OBJECTIVE_SCRIPT stage_objectives32 = {stage_objective_commands32, 5};

/* Native stage records from SLES_004.74:0x800CD4F8..0x800CD99B */
static STAGE_ENV stage_envs[STAGE_COUNT] = {
    {"DOCKS\\M0.CC", &stage_objectives0, stage_resources0, 0, 25600, -46080, 113664, {0x00050000}, {0x0000fea2}, "TILEDOKN.BIN"},
    {"DOCKS\\M1.CC", &stage_objectives1, stage_resources1, 0, 25600, -44032, 113664, {0x00060001}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"DOCKS\\M2.CC", &stage_objectives2, stage_resources2, 0, 25600, -44032, 113664, {0x00070001}, {0x0000fea2}, "TILEDOKD.BIN"},
    {"AIRSHIP\\M3.CC", &stage_objectives3, stage_resources3, 0, 177152, -18432, 0, {0x00100000}, {0x0000fd44}, "TILEAIRS.BIN"},
    {"AIRSHIP\\M4.CC", &stage_objectives4, stage_resources4, 0, 0, -44032, 0, {0x000a0000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"DOCKS\\M5.CC", &stage_objectives5, stage_resources5, 0, 25600, -52224, 113664, {0x00070000}, {0x0000fd44}, "TILEDUSK.BIN"},
    {"MISS0\\M6.CC", &stage_objectives6, stage_resources6, 0, 0, -44032, 0, {0x00050000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"DOCKS\\M7.CC", &stage_objectives7, stage_resources7, 0, 25600, -44032, 113664, {0x00060000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"DOCKS\\M8.CC", &stage_objectives8, stage_resources8, 0, 25600, -36864, 113664, {0x00070000}, {0x0000fea2}, "TILECITY.BIN"},
    {"JUNGLE\\M9.CC", &stage_objectives9, stage_resources9, 0, 107520, -32768, 131072, {0x00080000}, {0x0000fd44}, "TILEJNGL.BIN"},
    {"JUNGLE\\M10.CC", &stage_objectives10, stage_resources10, 0, 107520, -32768, 131072, {0x000e0000}, {0x0000fd44}, "TILEJNGL.BIN"},
    {"HQ\\M11.CC", &stage_objectives6, stage_resources11, 0, 25600, -40960, 0, {0x00020000}, {0x0000fe0c}, "TILEHQ.BIN"},
    {"MISS0\\M12.CC", &stage_objectives12, stage_resources12, 0, 27648, -35840, 0, {0x000a0000}, {0x0000fd44}, "TILEAIRF.BIN"},
    {"MISS0\\M13.CC", &stage_objectives13, stage_resources13, 0, 27648, -35840, 0, {0x00100000}, {0x0000fd44}, "TILEAIRF.BIN"},
    {"MISS0\\M14.CC", &stage_objectives14, stage_resources14, 0, 0, -35840, 0, {0x00060000}, {0x0000fd44}, "TILEAIRF.BIN"},
    {"MISS0\\M15.CC", &stage_objectives15, stage_resources15, 0, 27648, -35840, 0, {0x00070000}, {0x0000fd44}, "TILEAIRF.BIN"},
    {"WATER\\M16.CC", &stage_objectives16, stage_resources16, 0, 25600, -35840, 0, {0x000e0000}, {0x0000fea2}, "TILEWATR.BIN"},
    {"MISS0\\M17.CC", &stage_objectives17, stage_resources17, 0, 0, -35840, 0, {0x000d0000}, {0x0000fce0}, "TILEAIRF.BIN"},
    {"WATER\\M18.CC", &stage_objectives18, stage_resources18, 0, 25600, -35840, 0, {0x000d0000}, {0x0000fea2}, "TILEWBCK.BIN"},
    {"WATER\\M19.CC", &stage_objectives19, stage_resources19, 0, 25600, -35840, 0, {0x000d0000}, {0x0000fea2}, "TILEWBCK.BIN"},
    {"WATER\\M20.CC", &stage_objectives20, stage_resources20, 0, 25600, -35840, 0, {0x000e0000}, {0x0000fea2}, "TILEWATR.BIN"},
    {"WATER\\M21.CC", &stage_objectives21, stage_resources21, 0, 25600, -35840, 0, {0x000e0000}, {0x0000fea2}, "TILEWBCK.BIN"},
    {"JUNGLE\\M22.CC", &stage_objectives22, stage_resources22, 0, 107520, -32768, 131072, {0x000d0000}, {0x0000fd44}, "TILEJNGL.BIN"},
    {"JUNGLE\\M23.CC", &stage_objectives23, stage_resources23, 0, 107520, -32768, 131072, {0x00100000}, {0x0000fd44}, "TILEJNGL.BIN"},
    {"JUNGLE\\M24.CC", &stage_objectives24, stage_resources24, 0, 107520, -32768, 131072, {0x000e0000}, {0x0000fd44}, "TILEJNGL.BIN"},
    {"JUNGLE\\M25.CC", &stage_objectives25, stage_resources25, 0, 107520, -32768, 131072, {0x00080000}, {0x0000fd44}, "TILEJNGL.BIN"},
    {"AIRSHIP\\M26.CC", &stage_objectives26, stage_resources26, 0, 0, -18432, 0, {0x00080000}, {0x0000fdc4}, "TILEDOKD.BIN"},
    {"IND\\M27.CC", &stage_objectives27, stage_resources27, 0, 0, -18432, 0, {0x00050000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"IND\\M28.CC", &stage_objectives28, stage_resources28, 0, 0, -18432, 0, {0x00100000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"IND\\M29.CC", &stage_objectives29, stage_resources29, 0, 0, -18432, 0, {0x00060000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"XXXX\\XXXX", &stage_objectives6, stage_resources27, 0, 0, -18432, 0, {0x00080000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"IND\\M31.CC", &stage_objectives31, stage_resources31, 0, 0, -18432, 0, {0x00070000}, {0x0000fd44}, "TILEDOKD.BIN"},
    {"IND\\M32.CC", &stage_objectives32, stage_resources32, 0, 0, -18432, 0, {0x000a0000}, {0x0000fd44}, "TILEDOKD.BIN"},
};

/* Functions. */
/* Original: FUN_800AAEC8. */
void hq_assets_load(void)
{
    enum { STAGE_ENV_COUNT = sizeof(stage_envs) / sizeof(stage_envs[0]) };
    const char *archive_path;
    const uint8 *map_info;
    RESOURCE_DATA member;
    sint32 map_width, map_depth;
    sint32 map_models;
    const void *effect_data;
    uint32 effect_size;
    uint32 models_size;
    void *models;
    uint8 *texinfo;
    sint32 texinfo_size;
    sint16 saved_model_count;
    sint32 archive_size;
    void *archive;
    STAGE_ENV *stage;

    /* 800AAEEC..800AAF28: this is the exact stage-class predicate. */
    g_session.scuba_active = (g_session.stage == 0x10 || (g_session.stage >= 18 && g_session.stage < 20) || (g_session.stage >= 20 && g_session.stage < 22));

    XASSERT(!(g_session.stage < 0 || g_session.stage >= STAGE_ENV_COUNT));
    stage = &stage_envs[g_session.stage];
    archive_path = stage->archive_path;
    g_stage_env = stage;
    archive_size = game_file_size(archive_path);
    if (archive_size <= 0)
    {
#if defined(_DEBUG)
        fprintf(stderr, "STAGE ARCHIVE: %s\n", archive_path);
#endif
        abort();
    }
    archive = runtime_heap_allocate_sector_aligned(archive_size);
    game_file_read(archive_path, archive, archive_size);
    archive = runtime_heap_shrink(archive, archive_size);
    /* 0x800AAF94: retain the loaded CC base at gp+0x208. */
    g_stage_archive = archive;
    g_stage_archive_size = archive_size;
    member = archive_member_find("MAPINFO.BIN", archive, archive_size);
    map_info = member.data;
    XASSERT(!(map_info == 0 || member.size < 12));
    map_width = xport_load_le32(map_info);
    map_depth = xport_load_le32(map_info + 4);
    map_models = xport_load_le32(map_info + 8);
    XASSERT(!(map_width <= 0 || map_depth <= 0 || map_depth > 0x7fffffff / 0xc000 || map_width > 0x7fffffff / 0x4000 || map_width > 0x7fffffff / (map_depth + 5) / 8 || map_models > 0x7fff));
    map_runtime_initialize(map_width, map_depth);
    g_model_packet_count = 0;
    g_model_count = map_models;
    member = archive_member_find("TEXINFO.BIN", archive, archive_size);
    texinfo = member.data;
    texinfo_size = member.size;
    {
        sint32 material_count = 0;

        /* Original AAFF0..AB080 binds the stage only when TEXINFO exists */
        if (texinfo != 0)
        {
            while (material_count < 0x3f)
            {
                uint8 material;
                XASSERT(material_count < texinfo_size);
                material = texinfo[material_count];
                if (material == 0xff)
                    break;
                XASSERT(!(material >= sizeof(g_hq_material_resources) / sizeof(g_hq_material_resources[0])));
                g_material_resource_table[material_count++] = g_hq_material_resources[material];
            }
            g_material_resource_table[material_count] = -1;
            stage->material_lookup = g_material_resource_table;
        }
    }
    member = archive_member_find("MODELS.BIN", archive, archive_size);
    models = member.data;
    models_size = member.size;

    /* Reserve level slots before the original script appends its models */
    sint32 models_ready = model_begin(models, models_size, map_models) && model_reserve_base(map_models);
    XASSERT(models_ready);
    (void)models_ready;
    /* FUN_800AB478 runs after MODELS.BIN has been resolved but before the
     * map/effect members are resolved from the level archive. */
    resource_script_execute(stage->resource_script);

    saved_model_count = g_model_count;
    g_stage_map_input.floor = archive_member_find("MAPFLOOR.BIN", archive, archive_size);
    g_stage_map_input.grps = archive_member_find("CELLSDAT.BIN", archive, archive_size);
    member = archive_member_find("EFFECTS.BIN", archive, archive_size);
    effect_data = member.data;
    effect_size = member.size;
    member = archive_member_find("MODLATTS.BIN", archive, archive_size);
    g_stage_map_input.attribute_lookup = member;
    if (member.data != 0)
    {
        uint32 offset = member.data - (uint8 *)archive;
        /* Original 95604 reads following CC members until an attribute is zero */
        g_stage_map_input.attribute_lookup.size = archive_size - offset;
    }
    effects_initialize(effect_data, effect_size);
    g_model_count = 0;
    {
        uint32 model_cursor = 0;
        sint32 model_idx = 0;
        g_model_catalog.src = (const uint8 *)models;
        g_model_catalog.src_size = models_size;
        while (model_idx < map_models)
        {
            g_model_draw.model_idx = model_idx;
            sint32 consumed;
            if (model_idx >= g_model_catalog.capacity || model_cursor > models_size)
            {
                fatal_error_with_val("MODEL SOURCE RANGE", g_model_draw.model_idx);
                abort();
            }
            g_model_catalog.descs[model_idx].data = (const uint8 *)models + model_cursor;
            g_model_catalog.descs[model_idx].data_size = models_size - model_cursor;
            consumed = model_parse_and_build_packets(0, 0, 0, 0, stage->material_lookup, sizeof(g_material_resource_table) / sizeof(g_material_resource_table[0]));
            if (consumed < 0)
            {
                fatal_error_with_val("MODEL ERROR", g_model_draw.model_idx);
                abort();
            }
            model_cursor += consumed;
            model_idx++;
        }
    }
    g_map_render.background_y = stage->background_y;
    room_depth_limit_create();
    g_model_count = saved_model_count;

    /* 0x800AB214..0x800AB3C8: per-level state is cleared after the effect
     * records and model descriptors have been built, then seeded with the
     * exact counts of the effect types consumed by mission/player logic. */
    memset(g_mechanism_switch_states, 0, sizeof(g_mechanism_switch_states));
    memset(g_mission.actions, 0, sizeof(g_mission.actions));
    memset(g_mission.primary, 0, sizeof(g_mission.primary));
    memset(g_mission.secondary, 0, sizeof(g_mission.secondary));
    memset(g_mission.counts, 0, sizeof(g_mission.counts));
    memset(g_mission.message_slots, 0, sizeof(g_mission.message_slots));
    g_mission.counts[3] = effect_count(0x35);
    g_mission.counts[4] = effect_count(0x38);
    g_mission.counts[5] = effect_count(0x50);
    g_mission.counts[6] = effect_count(0x28) / 2;
    g_mission.counts[7] = effect_count(0x32);
    g_mission.counts[8] = effect_count(0x62);
    g_mission.counts[9] = effect_count(0x6d);
    g_mission.counts[10] = effect_count(0x73);
    g_mission.counts[11] = effect_count(0x75);
    g_mission.counts[23] = effect_count(0x74);
    g_mission.counts[12] = effect_count(0x36);
    g_mission.counts[13] = effect_count(0x7b);
    g_mission.counts[15] = effect_count(0x7d);
    g_mission.counts[16] = effect_count(0x7f);
    g_mission.counts[17] = effect_count(0x80);
    g_mission.counts[18] = effect_count(0x37);
    if (g_session.stage == 2 || g_session.stage == 13)
        g_mission.required_items = 4;
    /* 800AB3EC..800AB444.  Mission 0 takes the default branch. */
    g_map.terrain_variation = 0x1f;
    if ((g_session.stage >= 9 && g_session.stage < 11) || (g_session.stage >= 22 && g_session.stage < 24) || g_session.stage == 0x18)
        g_map.terrain_variation = 4;
    /* Original 800AB41C..800AB458 loads the stage4 pose after scene setup */
    if (g_session.stage == 4)
    {
        sint32 pose_size = 0;
        free((void *)g_mechanoid_initial_pose.data);
        g_mechanoid_initial_pose.data = game_file_load("AIRSHIP\\RB.POD", &pose_size);
        g_mechanoid_initial_pose.size = pose_size > 0 ? pose_size : 0;
        XASSERT(!(g_mechanoid_initial_pose.data == 0 || pose_size <= 0));
    }
}
