# Agent Armstrong

Resolve `[XPORT_ROOT]` from `xport-project.json` and read `[XPORT_ROOT]/AGENTS.md`. Keep shared methodology there; keep detailed project evidence under `status`.

## Identity and build

- Game/revision: Agent Armstrong PAL Europe, `SLES-00474`
- Original executable: `orig/SLES_004.74`, SHA-256 `ef77ec5370c43f1a057c9af88b67975232dfecf885dfecdca93ec5ece3e050d4`
- Payload: file offset `0x800`, load `0x80010000`, size `0x000C4000`, entry `0x800B7F58`, GP `0x800D3C88`
- Native C/VS2022 v143 x86: `src/platform/win/AA.sln` -> `bin/AA.exe`; working directory `bin`; intermediates `_build`
- Web/Emscripten: `src/platform/web/Makefile` -> `../.github/site/AA`; run `make CONFIG=Release` from the Makefile directory
- Commands: `python -B ../xport/tools/xport.py --project . TOOL ...`

## Evidence and boundaries

- Immutable legacy IDA export: `orig/asm/SLES`; canonical image: `status/ida/images/SLES_004.74`; profile: `tools/ida/SLES_004.74.json`
- Authorities: `status/analysis.sqlite`, `status/translation-ledger.json`, `status/semantic-map.json`
- `status/legacy-pre-common` and `tools/archive/legacy-local-20260923` are historical evidence, never active tooling
- Preserve semantic C names, module layout, structures and `/* Original: NAME_800ABCDE. */` identities. Imported structure facts remain hypotheses until reader/writer and layout evidence is complete
- Overlay modules reuse virtual addresses; never merge identities without the loaded image and relocation context
- Ghidra/PsyQ classification is not imported. Treat SDK names as hypotheses until the shared classification gate passes

## Runtime

- Compile the shared `[XPORT_ROOT]/src` runtime directly; project-local runtime copies and callback-registration fallbacks are forbidden
- Disc data stays under `bin/DATA` with exact ISO9660 lengths; Red Book tracks are decimal-name WAV files under `bin/MUSIC`
- Reserved GDB ports: audit `2404`, trace `2405`, user `2406`, migration `2407`
- Trace workflow remains disabled until Agent Armstrong hooks, layouts, continuation ABI and adapter are validated. Never reuse another game's addresses or structures
- Before C/H work use `X query 0xADDRESS --image SLES_004.74 --audit-context`; after changes use `X code_refresh`
