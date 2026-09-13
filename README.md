# AgentArmstrong-xport
Agent Armstrong (PSX) decompilation port via Codex

Disc data must retain the exact ISO9660 file lengths, not whole-sector lengths.
Sector-padded extraction changes the original allocator's shrink/best-fit
decisions and can make V2 Chase fail while loading `COMMON0/AA.CC`.

Audit existing `bin/DATA` against the PAL MODE2/2352 data track:

```powershell
python tools/audit_disc_files.py "path/to/Track 01.bin"
```

To repair verified sector padding, add
`--repair-padding _build/_data-padding-backup-new` (a new directory).
The tool verifies bytes against the disc, backs up each changed file, and
removes only bytes beyond the ISO directory length. Form2 audio/video is left
untouched. See `status/v2-loading-regression.md` for the runtime evidence.
