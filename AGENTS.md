# Agent Armstrong

Project-specific contract. `[XPORT_ROOT]` is resolved from `xport-project.json`; `toolset` points to its `tools` directory. Read [shared PIPELINE.md](../xport/tools/PIPELINE.md) before work. Shared behavior and format changes belong in `[XPORT_ROOT]/tools`; game facts remain here.

## Project identity

- Game: Agent Armstrong, PAL Europe, `SLES-00474`
- Original executable: `orig/SLES_004.74`, SHA-256 `ef77ec5370c43f1a057c9af88b67975232dfecf885dfecdca93ec5ece3e050d4`
- PS-X EXE payload: file offset `0x800`, load `0x80010000`, size `0x000C4000`, entry `0x800B7F58`, GP `0x800D3C88`
- Resident code range currently inventoried by IDA: `0x80087B60..0x800C28E4`
- Native language and toolchain: C, Visual Studio 2022 v143, x86; Debug for diagnostics and intermediate runs, mandatory Release final build
- Native solution: `src/platform/win/AA.sln`; executable: `bin/AA.exe`; working directory: `bin`; intermediates: `_build`
- Win32 links at fixed base `0x02000000` without ASLR so the complete image remains inside one 16-MiB segment required by native 24-bit ordering-table links
- Every native build defines `WND_TITLE="Agent Armstrong (xport)"`, `WND_WIDTH=960`, `WND_HEIGHT=768`, `FIELD_RATE=50` and `XPORT_NATIVE`
- Native Release executable identity after revision `2026-09-24T16:58:21Z`: SHA-256 `2161582308e86bfa8a96887ad45bf93c1e89ac046e49b4993017b9fcf65aff3d`
- Commands from project root: `python -B ../xport/tools/xport.py --project . TOOL ...`
- Brief comments start with a capital letter and omit the final period

## Static evidence and status

- `orig/asm/SLES` is the immutable IDA 7.7/Hex-Rays legacy export. It has 601 resident functions. `status/ida/images/SLES_004.74` is its canonical common-toolset conversion; independent Capstone decoding and original-byte verification remain mandatory
- `tools/ida/SLES_004.74.json` is the active image profile. Do not edit the legacy export or its generated listings
- `status/analysis.sqlite`, `status/translation-ledger.json`, `status/semantic-map.json` and generated progress are current authorities. `status/legacy-pre-common` and `tools/archive/legacy-local-20260923` are historical inputs, not active tooling or current status
- Legacy `DONE` was not transferred automatically. Bound implementations imported from the former ledger start as `WIP`; documented platform replacements may remain `SKIP`. Promote only through the shared evidence gates
- Data-reference sites absent from the legacy exporter remain null in the canonical conversion. Re-export from a supported IDA database or obtain independent instruction/xref proof before relying on an exact site
- Ghidra/PsyQ classification has not yet been imported into the common database. Treat SDK names from source, maps or the former progress report as hypotheses until the shared recognition/classification gate is completed

## Semantic names and structures

- This project already reached the late semantic phase before common-pipeline adoption. Preserve the semantic C names, module layout and object structures
- `status/semantic-map.json` is the bidirectional original-to-semantic mapping. Bound resident aliases are hash-pinned. Overlay aliases remain `pending_image_inventory` until that overlay image is separately imported and hash-pinned
- The same virtual overlay addresses are reused by different modules. `AIRSHIP:0x800FADD4`, `BIGROB:0x800FADD4`, `GUNSHIP:0x800FADD4`, `GYRO:0x800FADD4`, `INDUSTRY:0x800FADD4`, `SCUBA:0x800FADD4`, `TRUCK:0x800FADD4` and `V2:0x800FADD4` are distinct identities
- Structure entries imported from current headers are `legacy_hypothesis`. Keep their names/layouts in source, but do not mark them reviewed until size, offsets, widths, signedness, all important readers/writers and runtime lifecycle are recorded
- Preserve `/* Original: NAME_800ABCDE. */` identity comments. A semantic rename never removes image/address identity or transfers status between functions
- Apply future semantic renames and structure consolidation as bounded subsystem batches after a coverage snapshot. Run `X code_refresh` and the applicable full replay before accepting each batch

## Runtime and emulator state

- Include `[XPORT_ROOT]/src/xport.h` as the only public Xport header. Compile `psx.c`, `psx_gpu.c`, `psx_pad.c`, `psx_spu.c` and `platform/win/main.c` directly from `[XPORT_ROOT]/src`; project-local copies are forbidden
- `src/platform/win` contains only `AA.sln` and `AA.vcxproj`. Game entry, runtime, input, file and audit adapters are platform-neutral sources directly under `src`
- The shared platform `main.c` owns native `main`, Win32 window/input/timer/WaveOut/file services and audio synchronization. The game entrypoint is `int xport_main(int argc, char **argv)` and required game bindings are declared by shared `xport.h`; callback registration and silent fallbacks are forbidden
- Game code includes shared `xport.h` as the only base-type, `FUNCTION_MARKER` and `GDB_CALL` owner. Do not add another fixed-width type or debug-entry macro header
- Shared `psx_addr` maps `DRAM`, `SCRATCHPAD` and verified readable native x86 addresses. Direct PsyQ image functions consume native C pointers; do not route their buffers through `psx_addr`
- Shared `psx_spu.c` owns SPU synthesis, VAB/libsnd compatibility, final mixing and IMA ADPCM Red Book transport. Game code uses `Ss*`, `Spu*` and `Cd*` directly and must not add a local audio runtime, VAB registry, decoder or render callback
- Native Red Book tracks are stereo 44.1-kHz IMA ADPCM WAV files under `bin/MUSIC`, named by decimal CD track number
- `bin/DATA` contains runtime data extracted from the first CD data track; the Release/Debug executable remains directly at `bin/AA.exe`
- Camera shake changes `DISPENV.screen.y`; shared GPU presentation translates the selected framebuffer and fills exposed output rows with black without sampling the adjacent VRAM framebuffer
- Shared DuckStation executable and resources live only under `[XPORT_ROOT]/tools/duckstation/distribution`
- Reserved loopback GDB ports: audit `2404`, trace `2405`, user `2406`, migration `2407`; default automated role is migration
- Project-owned runtime directories: `tools/duckstation/{api-runtime,trace-runtime,user-runtime,migration-runtime}`. The former portable installation's BIOS, settings, memory cards and SLES save states were retained under `user-runtime`; old executable/DLL/resource copies are archived
- Save-state names are not evidence of scenario identity. Hash the state and record frame/event, input sequence and current overlay before reuse. Existing states predate the shared DuckStation build and require compatibility/anchor validation
- A common `[XportTrace]` profile, native continuation ABI and stage adapter are not yet proven for Agent Armstrong. `record`, `stop` and `converge` are blocked until their game addresses, layouts, hooks and adapter contract are added to `xport-project.json` and validated. Do not borrow Fighting Force addresses or structure layouts
- Old GDB, CDB, probe and runtime scripts are archived evidence. Do not invoke them as maintained workflow commands; replace reusable behavior in the common toolset and game facts in explicit profiles/adapters

## Game-specific constraints

- Disc files in `bin/DATA` must use exact ISO9660 file lengths, not sector-padded lengths. Sector padding changes allocator decisions and previously broke V2 Chase loading
- `bin/DATA` is runtime input. Do not regenerate, trim or replace it without a disc-image byte comparison and a recoverable backup
- Resident and overlay identities must never be merged by address alone. Confirm the loaded module and relocation base before applying an overlay symbol or callback
- Host replacements for CD, memory-card, STR/MDEC, GPU, SPU or other platform services require an explicit observable contract and `SKIP` evidence. Source presence or a working level is insufficient
- Existing named actor/effect structures remain partial views where their full reader/writer closure is not recorded. Do not widen, pack or merge them from equal size or common prefixes alone

## Required gates

- Before C/H work: `X query 0xADDRESS --image SLES_004.74 --audit-context`
- After every C/H change: `X code_refresh`
- Before reporting a completed task: run all required Debug gates, then build Release x86 last and record the resulting `bin/AA.exe` SHA-256 above
- If IDA/config/classification/ledger/semantic-map inputs change: `X build_database`, then `X render_progress`
- `DONE` requires original-byte identity, complete branch/delay-slot audit, ABI and memory-effect proof, unique C mapping, static evidence and comparable original/native runtime evidence
- Overlay import is separate per image. A pending semantic alias does not satisfy inventory, implementation or runtime status

## Migration archive

- `tools/archive/legacy-local-20260923` contains the retired project-local scripts, third-party tool copies, portable DuckStation implementation and root build/object artifacts
- `tools/archive/legacy-local-20260923/README.md` records restoration and active replacements. The archive is not on Python import paths and must not be used by `xport.py`
- `tools/archive/pre-shared-runtime-20260924` contains the retired local PsyQ, SPU, WaveOut and Win32 compatibility sources. It is inactive and excluded from build/include paths
