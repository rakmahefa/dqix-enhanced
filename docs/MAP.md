# Map of the recompiled game

This is the M4 deliverable: a stable, machine-readable answer to "where does this
routine live, who calls it, what does it touch". It covers the ARM9 (main code,
ITCM, DTCM, overlays 0-34) and the ARM7. The generated C itself is never
committed; only the symbol map (`config/usa/arm9`) and this map are.

## Regions

- **ARM9 main** (`0x02000000`, ~972 KiB): entry point, main loop, memory, BIOS
  shims, scheduler and OS services. 6207 functions.
- **ITCM** (`0x01ff8000`, ~6 KiB): time-critical ARM9 code (IRQ delivery,
  dispatch, hot paths). 38 functions.
- **DTCM** (`0x027e0000`, 96 bytes): ARM9 tightly-coupled data. No functions.
- **Overlays 0-34** (`0x021536e0`...): loaded one at a time on top of the main
  code; 35 overlays, 9000+ functions between them.
- **ARM7** (`0x02380000`, ~164 KiB): sound, wireless, IPC, timers, RTC.
  Function boundaries are discovered at build time and are not in the tracked
  symbol map, so this group has no function list in the index.

Entry point: `0x02000800` (`Entry`).

## Stable group ids

Every group has a stable id of the form `GRP-ARM9-<MODULE>` (or `GRP-ARM7`).
These ids are the canonical reference for bug reports, port work and the
upstream ownership map (M5): a fix points at a region, a port replaces a whole
group, and the same id is cited everywhere instead of being reinvented.

A group whose purpose is not yet known is recorded as `unknown (not yet
classified)` rather than guessed. 31 of the 35 overlays are in that state in
cycle 1; classifying them is ongoing work and does not block using the map.

## Group table

| Id | Module | Region | Subsystem | Purpose | Base | Size (bytes) | Functions |
|---|---|---|---|---|---|---|---|
| GRP-ARM9-MAIN | main | arm9 | core/boot | ARM9 entry point, main loop, memory, BIOS shims, scheduler and OS services | 0x02000000 | 994912 | 6207 |
| GRP-ARM9-ITCM | itcm | arm9 | core/itcm | time-critical ARM9 code (IRQ delivery, dispatch, hot paths) | 0x01ff8000 | 5952 | 38 |
| GRP-ARM9-DTCM | dtcm | arm9 | core/dtcm | ARM9 tightly-coupled data | 0x027e0000 | 96 | 0 |
| GRP-ARM9-OV000 | ov000 | arm9 | battle | combat turn processing and combatant stat updates | 0x021536e0 | 199488 | 826 |
| GRP-ARM9-OV001 | ov001 | arm9 | unknown | unknown (not yet classified) | 0x021536e0 | 74016 | 515 |
| GRP-ARM9-OV002 | ov002 | arm9 | items | inventory item lookup | 0x021536e0 | 105664 | 213 |
| GRP-ARM9-OV003 | ov003 | arm9 | unknown | unknown (not yet classified) | 0x021536e0 | 185792 | 510 |
| GRP-ARM9-OV004 | ov004 | arm9 | unknown | unknown (not yet classified) | 0x021536e0 | 119008 | 532 |
| GRP-ARM9-OV005 | ov005 | arm9 | unknown | unknown (not yet classified) | 0x021536e0 | 38720 | 89 |
| GRP-ARM9-OV006 | ov006 | arm9 | unknown | unknown (not yet classified) | 0x021536e0 | 52384 | 132 |
| GRP-ARM9-OV007 | ov007 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 32 | 0 |
| GRP-ARM9-OV008 | ov008 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 29440 | 63 |
| GRP-ARM9-OV009 | ov009 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 27328 | 43 |
| GRP-ARM9-OV010 | ov010 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 2080 | 3 |
| GRP-ARM9-OV011 | ov011 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 18656 | 186 |
| GRP-ARM9-OV012 | ov012 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 28800 | 71 |
| GRP-ARM9-OV013 | ov013 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 15424 | 40 |
| GRP-ARM9-OV014 | ov014 | arm9 | unknown | unknown (not yet classified) | 0x021842a0 | 21856 | 69 |
| GRP-ARM9-OV015 | ov015 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 36800 | 103 |
| GRP-ARM9-OV016 | ov016 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 72480 | 87 |
| GRP-ARM9-OV017 | ov017 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 314688 | 1194 |
| GRP-ARM9-OV018 | ov018 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 32 | 0 |
| GRP-ARM9-OV019 | ov019 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 3456 | 5 |
| GRP-ARM9-OV020 | ov020 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 10112 | 18 |
| GRP-ARM9-OV021 | ov021 | arm9 | unknown | unknown (not yet classified) | 0x0218b5a0 | 1600 | 5 |
| GRP-ARM9-OV022 | ov022 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 32 | 0 |
| GRP-ARM9-OV023 | ov023 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 159648 | 827 |
| GRP-ARM9-OV024 | ov024 | arm9 | battle | attack damage calculation | 0x021d8a40 | 161536 | 573 |
| GRP-ARM9-OV025 | ov025 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 93984 | 304 |
| GRP-ARM9-OV026 | ov026 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 25792 | 24 |
| GRP-ARM9-OV027 | ov027 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 20192 | 87 |
| GRP-ARM9-OV028 | ov028 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 4256 | 34 |
| GRP-ARM9-OV029 | ov029 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 7840 | 1 |
| GRP-ARM9-OV030 | ov030 | arm9 | unknown | unknown (not yet classified) | 0x021d8a40 | 4640 | 11 |
| GRP-ARM9-OV031 | ov031 | arm9 | unknown | unknown (not yet classified) | 0x02200160 | 313376 | 1962 |
| GRP-ARM9-OV032 | ov032 | arm9 | unknown | unknown (not yet classified) | 0x02200160 | 32 | 0 |
| GRP-ARM9-OV033 | ov033 | arm9 | loading | background asset loader | 0x022a2180 | 2208 | 5 |
| GRP-ARM9-OV034 | ov034 | arm9 | unknown | unknown (not yet classified) | 0x022a2180 | 2272 | 5 |
| GRP-ARM7 | arm7 | arm7 | services | ARM7 services (sound, wireless, IPC, timers, RTC) | 0x02380000 | 167876 | 0 |

Totals: 39 groups, 14782 functions, 63601 call edges.

## Machine-readable index

`docs/map-index.json` is generated by `tools/map_index.py` from
`config/usa/arm9` (and the extracted ROM metadata for module sizes). It
contains:

- `groups`: id, module, region, subsystem, purpose, classified flag, base
  address, size, function count, and `calls_to` (group-level edge counts).
- `functions`: id, group, name, address, size, mode.
- `call_edges`: pairs of function ids (caller -> callee), 63601 of them.

Regenerate with:

```
python3 tools/map_index.py
```

or `make map` (requires the ROM to have been extracted first).

## Unknown groups

31 of the 35 overlays are still unclassified in cycle 1. They are listed as
`unknown` on purpose: no subsystem or purpose has been guessed. Classifying
them is the follow-up work for M4; the map is usable for the groups that are
classified and records the rest explicitly rather than leaving them unnamed.
