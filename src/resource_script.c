#include <stdio.h>
#include <stdlib.h>

#include "resource_script.h"
#include "cc_archive.h"
#include "code_module.h"
#include "game_file.h"
#include "level_data.h"
#include "render.h"
#include "resource_table.h"
#include "runtime_heap.h"
#include "sprite.h"

typedef struct
{
    const char *name;
    uint32 resource;
    sint16 frames;
} TEX_RESOURCE_REC;

/* Original data: SLES_004.74:0x800CBBC0..0x800CC3CF */
enum
{
    TEX_RESOURCE_COUNT = 86
};

static const TEX_RESOURCE_REC tex_resources[TEX_RESOURCE_COUNT] = {
    {"TX00.BIN", 0x6800, 1},      {"TX01.BIN", 0x6c00, 1},      {"TX02.BIN", 0x7000, 1},     {"TX03.BIN", 0x7400, 1},     {"TX04.BIN", 0x7800, 1},    {"TX05.BIN", 0x7c00, 1},   {"TX06.BIN", 0x8000, 1},     {"TX07.BIN", 0x29c00, 1},     {"TX08.BIN", 0x2a000, 1},  {"TX09.BIN", 0x2a400, 1},    {"TX10.BIN", 0x2a800, 1}, {"TX11.BIN", 0x2ac00, 1},    {"TX12.BIN", 0x2b000, 1},    {"TX13.BIN", 0x2b400, 1},     {"TX14.BIN", 0x18c00, 1}, {"TX15.BIN", 0x19000, 1},    {"TX16.BIN", 0x19400, 1}, {"TX17.BIN", 0x19800, 1},     {"TX18.BIN", 0x19c00, 1},     {"TX19.BIN", 0x1a000, 1},     {"TX20.BIN", 0x1a400, 1},    {"TX21.BIN", 0x1a800, 1},    {"TX22.BIN", 0x1ac00, 1},  {"TX23.BIN", 0x1b000, 1},     {"TX24.BIN", 0x1b400, 1},     {"TX25.BIN", 0x1b800, 1},    {"TX26.BIN", 0x1bc00, 1},    {"TX27.BIN", 0x1c000, 1},     {"TX28.BIN", 0x1fc00, 1},
    {"TX29.BIN", 0x20000, 1},     {"TX30.BIN", 0x20400, 1},     {"TX31.BIN", 0x20800, 1},    {"TX32.BIN", 0x20c00, 1},    {"TX33.BIN", 0x21000, 1},   {"TX34.BIN", 0x21400, 1},  {"TX35.BIN", 0x21800, 1},    {"TX36.BIN", 0x21c00, 1},     {"TX37.BIN", 0x22000, 1},  {"TX38.BIN", 0x22400, 1},    {"TX39.BIN", 0x22800, 1}, {"TX40.BIN", 0x1400, 1},     {"TX41.BIN", 0x1800, 1},     {"TX42.BIN", 0x1c00, 1},      {"TX43.BIN", 0x2f000, 1}, {"TX44.BIN", 0x12800, 1},    {"TX45.BIN", 0x12c00, 1}, {"SAND.BIN", 0x30400, 6},     {"HILLS.BIN", 0x18000, 4},    {"ROBTEXT.BIN", 0x15800, 1},  {"ROBTILE.BIN", 0x1c000, 1}, {"TRCK2.BIN", 0x1e400, 1},   {"TRCK3.BIN", 0x1e800, 1}, {"TRCK1.BIN", 0x1ec00, 1},    {"BELLEND.BIN", 0x6400, 1},   {"BELLEND2.BIN", 0x6400, 1}, {"BELLEND3.BIN", 0x6400, 1}, {"GYROTXT1.BIN", 0x15800, 1}, {"GUNEMP.BIN", 0x15800, 1},
    {"TANKTXT1.BIN", 0x22c00, 1}, {"TANKTXT2.BIN", 0x23000, 1}, {"CITYBACK.BIN", 0x6400, 1}, {"FLAME2.BIN", 0x23800, 12}, {"RADAR1.BIN", 0x23c00, 1}, {"HQBACK.BIN", 0x6400, 1}, {"MAPBITS.BIN", 0x2000, 11}, {"AGNTLOGO.BIN", 0x25c00, 2}, {"AIR27.BIN", 0x28800, 1}, {"MINIGTX.BIN", 0x29400, 1}, {"WRECK.BIN", 0x6400, 1}, {"UWEXP1.BIN", 0x2bc00, 29}, {"MINISUB.BIN", 0x2c000, 1}, {"DEPTHCHR.BIN", 0x2c400, 1}, {"MINE.BIN", 0x2cc00, 1}, {"SMLBOAT.BIN", 0x2d000, 1}, {"FISH.BIN", 0x2d800, 9}, {"UWGUNEMP.BIN", 0x15800, 1}, {"BULKHEAD.BIN", 0x2dc00, 1}, {"UWPICKUP.BIN", 0x2e000, 4}, {"UWBATH.BIN", 0x2e400, 1},  {"UWRADIOB.BIN", 0x5400, 3}, {"FIRE.BIN", 0x2e800, 12}, {"ROBFLARE.BIN", 0x2e800, 2}, {"ELECBALL.BIN", 0x2d800, 2}, {"WATRBACK.BIN", 0x6400, 1}, {"CITYDUSK.BIN", 0x6400, 1}, {"MAPMEDAL.BIN", 0x30800, 1},
};

/* Native diagnostic ABI counts command dispatch without changing game state */
GDB_DATA uint32 g_psx_resource_opcode_counts[10];

/* Native typed commands preserve original loading and recursion order */
/* Original: SLES_004.74:FUN_800AB478. */
GDB_CALL void resource_script_execute(const RESOURCE_COMMAND *script)
{
    uint8 *archive = 0, *direct = 0;
    sint32 archive_size = 0, direct_size = 0;
    if (script == 0)
        return;
    for (;;)
    {
        const RESOURCE_COMMAND *entry = script++;
        RESOURCE_OP command = entry->op;
        if (command == RESOURCE_END)
        {
            ++g_psx_resource_opcode_counts[0];
            if (archive != 0)
            {
                runtime_heap_defer_free(archive);
            }
            runtime_heap_sweep();
            return;
        }
        if (command == RESOURCE_ARCHIVE)
        {
            ++g_psx_resource_opcode_counts[1];
            const char *path = entry->path;
            if (archive != 0)
            {
                runtime_heap_defer_free(archive);
                runtime_heap_sweep();
            }
            archive_size = game_file_size(path);
            archive = runtime_heap_allocate_sector_aligned(archive_size);
            game_file_read(path, archive, archive_size);
            continue;
        }
        if (command == RESOURCE_FILE)
        {
            ++g_psx_resource_opcode_counts[2];
            const char *path = entry->path;
            if (archive != 0)
            {
                runtime_heap_defer_free(archive);
                archive = 0;
                archive_size = 0;
                runtime_heap_sweep();
            }
            direct_size = game_file_size(path);
            direct = runtime_heap_allocate_sector_aligned(direct_size);
            game_file_read(path, direct, direct_size);
            direct = runtime_heap_shrink(direct, direct_size);
            continue;
        }
        if (command == RESOURCE_TEXTURE)
        {
            uint32 resource;
            uint16 clut;
            ++g_psx_resource_opcode_counts[3];
            resource = entry->resource;
            XASSERT((resource >> 10) < SPRITE_CLUT_SLOT_COUNT);
            resource_table_register(resource, direct, direct_size, RESOURCE_COPY);
            clut = spr_clut_upload(resource, 0);
            /* Direct textures publish their CLUT without frame descriptors */
            g_spr_pal.cluts[resource >> 10] = clut;
            continue;
        }
        if (command == RESOURCE_INCLUDE)
        {
            ++g_psx_resource_opcode_counts[4];
            const RESOURCE_COMMAND *nested = entry->nested;
            if (archive != 0)
            {
                runtime_heap_defer_free(archive);
                archive = 0;
                archive_size = 0;
                runtime_heap_sweep();
            }
            resource_script_execute(nested);
            continue;
        }
        if (command == RESOURCE_MODELS)
        {
            ++g_psx_resource_opcode_counts[5];
            model_stage_load(entry->path, 0, entry->model_base, entry->model_count, entry->material[0], entry->material[1], entry->material[2], 0);
            continue;
        }
        if (command == RESOURCE_MATERIALS)
        {
            ++g_psx_resource_opcode_counts[6];
            const sint32 *materials = g_stage_env->material_lookup;
            sint32 material_idx;
            if (materials == 0)
            {
#if defined(_DEBUG)
                fputs("STAGE MATERIAL TABLE\n", stderr);
#endif
                abort();
            }
            for (material_idx = 0; materials[material_idx] != -1; material_idx++)
            {
                const TEX_RESOURCE_REC *rec;
                uint32 resource;
                uint16 clut;
                resource = materials[material_idx];
                if (resource == 0)
                    continue;
                rec = tex_resources;
                while (rec < tex_resources + TEX_RESOURCE_COUNT && rec->resource != resource)
                    ++rec;
                XASSERT(!(rec == tex_resources + TEX_RESOURCE_COUNT));
                spr_archive_resource_load(rec->name, resource, rec->frames, 0, archive, archive_size);
                clut = spr_clut_upload(resource, 0);
                spr_assign_clut_range(resource, rec->frames, clut);
            }
            /* 800AB814..800AB894: nonzero environment +0x10 enables the
             * stage-selected 0x30400 tile.  Stage 0 selects table entry 1. */
            {
                const char *name = g_stage_env->background_resource ? g_stage_env->background_tile : 0;
                if (name)
                {
                    uint16 clut;
                    spr_archive_resource_load(name, 0x30400, 1, 0, archive, archive_size);
                    clut = spr_clut_upload(0x30400, 0);
                    spr_assign_clut_range(0x30400, 1, clut);
                }
            }
            continue;
        }
        if (command == RESOURCE_MODEL_ARCHIVE)
        {
            ++g_psx_resource_opcode_counts[7];
            const void *model_archive = direct;
            RESOURCE_DATA model_data = archive_member_find("MODELS.BIN", model_archive, direct_size);
            model_stage_load(0, model_data.size, entry->model_base, entry->model_count, entry->material[0], entry->material[1], entry->material[2], model_data.data);
            RESOURCE_DATA pose = archive_member_find("POD.BIN", model_archive, direct_size);
            if (entry->pose != 0)
            {
                entry->pose->data = pose.data;
                entry->pose->size = pose.size;
            }
            continue;
        }
        if (command == RESOURCE_OVERLAY)
        {
            ++g_psx_resource_opcode_counts[8];
            overlay_module_select(entry->path);
            continue;
        }
        {
            ++g_psx_resource_opcode_counts[9];
            sint32 idx;
            for (idx = entry->first; idx <= entry->last; idx++)
            {
                const TEX_RESOURCE_REC *rec;
                uint32 resource;
                sint32 frames;
                uint16 clut;
                XASSERT(!(idx < 0 || idx >= TEX_RESOURCE_COUNT));
                rec = &tex_resources[idx];
                resource = rec->resource;
                frames = rec->frames;
                spr_archive_resource_load(rec->name, resource, frames, 1, archive, archive_size);
                clut = spr_clut_upload(resource, 1);
                spr_assign_clut_range(resource, frames, clut);
            }
            continue;
        }
    }
}
