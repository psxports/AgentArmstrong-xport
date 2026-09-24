#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cc_archive.h"
#include "effect_update.h"
#include "global.h"
#include "level_data.h"
#include "map.h"
#include "mechanoid.h"
#include "model.h"
#include "object.h"
#include "original_file.h"
#include "game_runtime.h"
#include "player.h"
#include "psx.h"
#include "runtime_heap.h"
#include "sprite.h"
#include "stubs.h"

/* Types. */
typedef struct
{
    const char *archive_path;
    uint32 mission_script;
    uint32 resource_script;
    uint32 material_table;
    uint32 background_resource;
    uint32 background_y;
    uint32 map_resource;
    uint32 music_and_flags;
    uint32 display_flags;
} STAGE_ENV_SOURCE;

/* Variables. */

/* Exact 0x24-byte records at PAL 0x800CD4F8..0x800CD99C.  FUN_800AAEC8
 * indexes this table directly with DAT_800D3CAC; there is no fallback stage. */
static const STAGE_ENV_SOURCE stage_envs[33] = {{"DOCKS\\M0.CC", 0x800c9884u, 0x800cccf8u, 0, 0x00006400u, 0xffff4c00u, 0x0001bc00u, 0x00050000u, 0x0000fea2u},  {"DOCKS\\M1.CC", 0x800c98a0u, 0x800ccd28u, 0, 0x00006400u, 0xffff5400u, 0x0001bc00u, 0x00060001u, 0x0000fd44u},   {"DOCKS\\M2.CC", 0x800c98bcu, 0x800ccd80u, 0, 0x00006400u, 0xffff5400u, 0x0001bc00u, 0x00070001u, 0x0000fea2u},   {"AIRSHIP\\M3.CC", 0x800c98d0u, 0x800ccdb0u, 0, 0x0002b400u, 0xffffb800u, 0x00000000u, 0x00100000u, 0x0000fd44u}, {"AIRSHIP\\M4.CC", 0x800c98e4u, 0x800ccde0u, 0, 0x00000000u, 0xffff5400u, 0x00000000u, 0x000a0000u, 0x0000fd44u}, {"DOCKS\\M5.CC", 0x800c98f0u, 0x800cce08u, 0, 0x00006400u, 0xffff3400u, 0x0001bc00u, 0x00070000u, 0x0000fd44u},    {"MISS0\\M6.CC", 0x800c9b48u, 0x800cce50u, 0, 0x00000000u, 0xffff5400u, 0x00000000u, 0x00050000u, 0x0000fd44u},
                                                {"DOCKS\\M7.CC", 0x800c9910u, 0x800cce78u, 0, 0x00006400u, 0xffff5400u, 0x0001bc00u, 0x00060000u, 0x0000fd44u},  {"DOCKS\\M8.CC", 0x800c9924u, 0x800cceb8u, 0, 0x00006400u, 0xffff7000u, 0x0001bc00u, 0x00070000u, 0x0000fea2u},   {"JUNGLE\\M9.CC", 0x800c9930u, 0x800ccef8u, 0, 0x0001a400u, 0xffff8000u, 0x00020000u, 0x00080000u, 0x0000fd44u},  {"JUNGLE\\M10.CC", 0x800c9944u, 0x800ccf38u, 0, 0x0001a400u, 0xffff8000u, 0x00020000u, 0x000e0000u, 0x0000fd44u}, {"HQ\\M11.CC", 0x800c9b48u, 0x800ccf88u, 0, 0x00006400u, 0xffff6000u, 0x00000000u, 0x00020000u, 0x0000fe0cu},     {"MISS0\\M12.CC", 0x800c9958u, 0x800ccfa8u, 0, 0x00006c00u, 0xffff7400u, 0x00000000u, 0x000a0000u, 0x0000fd44u},   {"MISS0\\M13.CC", 0x800c9964u, 0x800ccff0u, 0, 0x00006c00u, 0xffff7400u, 0x00000000u, 0x00100000u, 0x0000fd44u},
                                                {"MISS0\\M14.CC", 0x800c9980u, 0x800cd038u, 0, 0x00000000u, 0xffff7400u, 0x00000000u, 0x00060000u, 0x0000fd44u}, {"MISS0\\M15.CC", 0x800c9990u, 0x800cd090u, 0, 0x00006c00u, 0xffff7400u, 0x00000000u, 0x00070000u, 0x0000fd44u},  {"WATER\\M16.CC", 0x800c99a4u, 0x800cd0d8u, 0, 0x00006400u, 0xffff7400u, 0x00000000u, 0x000e0000u, 0x0000fea2u},  {"MISS0\\M17.CC", 0x800c99b8u, 0x800cd128u, 0, 0x00000000u, 0xffff7400u, 0x00000000u, 0x000d0000u, 0x0000fce0u},  {"WATER\\M18.CC", 0x800c99dcu, 0x800cd168u, 0, 0x00006400u, 0xffff7400u, 0x00000000u, 0x000d0000u, 0x0000fea2u},  {"WATER\\M19.CC", 0x800c99f0u, 0x800cd1c0u, 0, 0x00006400u, 0xffff7400u, 0x00000000u, 0x000d0000u, 0x0000fea2u},   {"WATER\\M20.CC", 0x800c9a04u, 0x800cd218u, 0, 0x00006400u, 0xffff7400u, 0x00000000u, 0x000e0000u, 0x0000fea2u},
                                                {"WATER\\M21.CC", 0x800c9a18u, 0x800cd268u, 0, 0x00006400u, 0xffff7400u, 0x00000000u, 0x000e0000u, 0x0000fea2u}, {"JUNGLE\\M22.CC", 0x800c9a2cu, 0x800cd2c0u, 0, 0x0001a400u, 0xffff8000u, 0x00020000u, 0x000d0000u, 0x0000fd44u}, {"JUNGLE\\M23.CC", 0x800c9a48u, 0x800cd300u, 0, 0x0001a400u, 0xffff8000u, 0x00020000u, 0x00100000u, 0x0000fd44u}, {"JUNGLE\\M24.CC", 0x800c9a6cu, 0x800cd340u, 0, 0x0001a400u, 0xffff8000u, 0x00020000u, 0x000e0000u, 0x0000fd44u}, {"JUNGLE\\M25.CC", 0x800c9a88u, 0x800cd380u, 0, 0x0001a400u, 0xffff8000u, 0x00020000u, 0x00080000u, 0x0000fd44u}, {"AIRSHIP\\M26.CC", 0x800c9aa4u, 0x800cd3d8u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x00080000u, 0x0000fdc4u}, {"IND\\M27.CC", 0x800c9ac0u, 0x800cd400u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x00050000u, 0x0000fd44u},
                                                {"IND\\M28.CC", 0x800c9adcu, 0x800cd438u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x00100000u, 0x0000fd44u},   {"IND\\M29.CC", 0x800c9ae8u, 0x800cd470u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x00060000u, 0x0000fd44u},    {"XXXX\\XXXX", 0x800c9b48u, 0x800cd400u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x00080000u, 0x0000fd44u},     {"IND\\M31.CC", 0x800c9b18u, 0x800cd4a0u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x00070000u, 0x0000fd44u},    {"IND\\M32.CC", 0x800c9b34u, 0x800cd4d0u, 0, 0x00000000u, 0xffffb800u, 0x00000000u, 0x000a0000u, 0x0000fd44u}};

/* Functions. */
/* Original: FUN_800AAEC8. */
void hq_assets_load(void)
{
    static STAGE_ENV env;
    const char *archive_path;
    uint32 mission_script_address;
    uint32 resource_script_address;
    sint32 *map_info;
    sint32 models_size;
    void *models;
    uint8 *texinfo;
    sint32 texinfo_size;
    sint16 saved_model_count;
    sint32 archive_size;
    void *archive;
    const STAGE_ENV_SOURCE *stage;

    /* 800AAEEC..800AAF28: this is the exact stage-class predicate. */
    g_scuba_stage_active = (g_stage_index == 0x10 || (uint32)(g_stage_index - 0x12) < 2u || (uint32)(g_stage_index - 0x14) < 2u);

    stage = &stage_envs[g_stage_index];
    archive_path = stage->archive_path;
    mission_script_address = stage->mission_script;
    resource_script_address = stage->resource_script;
    env.background_resource = (sint32)stage->background_resource;
    env.background_y = (sint32)stage->background_y;
    env.map_resource = (sint32)stage->map_resource;
    env.music.packed = stage->music_and_flags;
    env.display.packed = stage->display_flags;
    env.archive_path = archive_path;
    env.mission_script = (const sint32 *)player_assets_executable_address(mission_script_address);
    env.resource_script = (const sint32 *)player_assets_executable_address(resource_script_address);
    env.material_lookup = g_material_resource_table;
    g_stage_env = &env;
    g_model_descriptors = (OriginalModelDescriptor *)runtime_heap_allocate_best_fit(0x1388);
    archive_size = game_file_size(archive_path);
    if (archive_size <= 0)
        return;
    archive = runtime_heap_allocate_sector_aligned(archive_size);
    game_file_read(archive_path, archive);
    archive = runtime_heap_shrink(archive, archive_size);
    /* 0x800AAF94: retain the loaded CC base at gp+0x208. */
    g_stage_archive = archive;
    map_info = (sint32 *)archive_member_find("MAPINFO.BIN", archive);
    if (map_info == 0)
        return;

    map_runtime_initialize(map_info[0], map_info[1]);
    g_model_packet_count = 0;
    g_model_count = (sint16)map_info[2];
    texinfo = (uint8 *)archive_member_find("TEXINFO.BIN", archive);
    texinfo_size = g_archive_member_size;
    {
        sint32 material_count = 0;

        /* SLES_004.74 0x800AAF9C..0x800AB010. */
        if (texinfo != 0)
        {
            while (material_count < 0x3f && (sint8)texinfo[material_count] != -1)
            {
                g_material_resource_table[material_count] = g_hq_material_resources[texinfo[material_count]];
                ++material_count;
            }
        }
        g_material_resource_table[material_count] = -1;
    }
    models = archive_member_find("MODELS.BIN", archive);
    models_size = g_archive_member_size;

    /* FUN_800AB478 runs after MODELS.BIN has been resolved but before the
     * map/effect members are resolved from the level archive. */
    psx_load_stage_textures(texinfo, (uint32)texinfo_size, resource_script_address);

    saved_model_count = g_model_count;
    g_map_floor_cells = archive_member_find("MAPFLOOR.BIN", archive);
    g_map_model_groups = archive_member_find("CELLSDAT.BIN", archive);
    g_effect_data = archive_member_find("EFFECTS.BIN", archive);
    g_map_model_attributes = archive_member_find("MODLATTS.BIN", archive);
    effects_initialize();
    g_model_count = 0;
    g_stage_model_count = (sint16)map_info[2];
    g_shared_scratch_value = 0;
    /* 0x800AB0B4 executes the resource script before 0x800AB15C parses the
       level MODELS.BIN. BIGROB packet templates therefore begin at zero,
       while its model descriptors begin after the level model slots. */
    if (model_begin(models, models_size, map_info[2] + (g_stage_index == 4 ? 28 : 0)))
    {
        uint8 *model_cursor = (uint8 *)models;
        if (g_stage_index == 4)
        {
            model_reserve_base(map_info[2]);
            if (!mechanoid_load_resources())
                fatal_error_with_value("BIGROB RESOURCE ERROR", g_stage_index);
            model_select_source(models, models_size);
        }
        while (g_shared_scratch_value < g_stage_model_count)
        {
            g_current_model_id = g_shared_scratch_value;
            g_model_descriptors[g_shared_scratch_value].model_data = model_cursor;
            model_cursor = (uint8 *)model_parse_and_build_packets(0, 0, 0, 0, g_material_resource_table);
            if (model_cursor == 0)
                fatal_error_with_value("MODEL ERROR", g_current_model_id);
            g_shared_scratch_value++;
        }
        /* FUN_80095300 appends resource-script models after the level's
           descriptor range.  The native parser indexes level models
           explicitly, so advance its append cursor to the same boundary. */
        if (g_stage_index != 4)
            model_reserve_base(g_stage_model_count);
        if (!psx_finalize_stage_resources())
            fatal_error_with_value("STAGE MODEL RESOURCE ERROR", g_stage_index);
        if (getenv("OA_MODEL_PARSER_AUDIT"))
            model_dump_parser_audit("../status/mission_model_parser_port.bin");
        if (getenv("OA_MODEL_PARSER_AUDIT"))
            sprite_resource_dump_audit("../status/mission_vram_descriptors_port.csv");
    }
    /* 0x800AB41C..0x800AB458 loads AIRSHIP/RB.POD. The host loader retains
       those same bytes in Mechanoid_LoadResources. */
    if (getenv("OA_DUMP_VRAM"))
        psx_dump_vram("VRAM.raw");
    g_stage_background_y = env.background_y;
    room_depth_limit_create();
    g_model_count = saved_model_count;

    /* 0x800AB214..0x800AB3C8: per-level state is cleared after the effect
     * records and model descriptors have been built, then seeded with the
     * exact counts of the effect types consumed by mission/player logic. */
    memset(g_trigger_states, 0, 0x80);
    memset(g_action_trigger_states, 0, 0x20);
    memset(g_primary_objective_states, 0, 0x10);
    memset(g_secondary_objective_states, 0, 0x10);
    memset(g_objective_counts, 0, 0x20);
    memset(g_mission_message_slots, 0, 0x10);
    g_objective_counts[3] = (uint8)effect_count(0x35);
    g_objective_counts[4] = (uint8)effect_count(0x38);
    g_objective_counts[5] = (uint8)effect_count(0x50);
    g_objective_counts[6] = (uint8)((sint16)effect_count(0x28) / 2);
    g_objective_counts[7] = (uint8)effect_count(0x32);
    g_objective_counts[8] = (uint8)effect_count(0x62);
    g_objective_counts[9] = (uint8)effect_count(0x6d);
    g_objective_counts[10] = (uint8)effect_count(0x73);
    g_objective_counts[11] = (uint8)effect_count(0x75);
    g_objective_counts[23] = (uint8)effect_count(0x74);
    g_objective_counts[12] = (uint8)effect_count(0x36);
    g_objective_counts[13] = (uint8)effect_count(0x7b);
    g_objective_counts[15] = (uint8)effect_count(0x7d);
    g_objective_counts[16] = (uint8)effect_count(0x7f);
    g_objective_counts[17] = (uint8)effect_count(0x80);
    g_objective_counts[18] = (uint8)effect_count(0x37);
    g_mission_script = (sint32)(uintptr_t)env.mission_script;
    if (g_stage_index == 2 || g_stage_index == 13)
        g_required_mission_item_count = 4;
    /* 800AB3EC..800AB444.  Mission 0 takes the default branch. */
    g_terrain_variation_limit = 0x1f;
    if ((uint32)(g_stage_index - 9) < 2u || (uint32)(g_stage_index - 0x16) < 2u || g_stage_index == 0x18)
        g_terrain_variation_limit = 4;
}
