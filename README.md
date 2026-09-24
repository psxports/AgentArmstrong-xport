# AgentArmstrong-xport
Agent Armstrong (PSX) decompilation port via Codex

The project uses the shared xport pipeline at `../xport/tools`. From this directory:

```powershell
python -B ../xport/tools/xport.py --project . doctor
python -B ../xport/tools/xport.py --project . query 0x80087B60 --image SLES_004.74 --audit-context
python -B ../xport/tools/xport.py --project . code_refresh
```

Current static status is generated from `status/analysis.sqlite`, `status/translation-ledger.json` and `status/semantic-map.json`. Retired local scripts and tool copies are under `tools/archive/legacy-local-20260923` and are not active commands. Runtime trace/converge remains blocked until the Agent Armstrong trace profile and continuation ABI are evidence-backed.

Disc data must retain the exact ISO9660 file lengths, not whole-sector lengths.
Sector-padded extraction changes the original allocator's shrink/best-fit
decisions and can make V2 Chase fail while loading `COMMON0/AA.CC`.

The former disc audit is retained under the migration archive for evidence only. Promote a maintained, profile-driven version into the shared toolset before auditing or repairing current runtime data. See `status/v2-loading-regression.md` for the historical runtime evidence.
