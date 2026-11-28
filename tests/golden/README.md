# Golden corpus (M3)

A versioned set of replays and frames covering Classic (pixel-identical),
Enhanced, each layout, a battle and a menu. One command runs it.

## One command

```
tests/golden/run.sh                      # uses game/dqix and rom/dq9.nds
GAME=/path/to/dqix ROM=/path/to/dq9.nds tests/golden/run.sh
```

The script generates per-case `settings.ini`/`input.txt` from `corpus.json`,
then runs the game for each case with a fixed RTC and scripted input, capturing
`crc.txt` (one CRC per frame, via `DQIX_CRC`) and `frames/` (every frame as
`.ppm`, via `DQIX_DUMP`). Output lands in `tests/golden/corpus/<case>/`.

If the game binary is missing the script prints the build command and exits 0,
so CI can skip gracefully.

## Cases

See `corpus.json` -> `cases`. Each case declares its settings, input script,
frame count and tags. `gen_cases.py` turns those into `settings.ini`.

## Interaction matrix

`corpus.json` -> `matrix` lists the cells of `docs/OPTIONS.md`'s interaction
matrix as assertable checks (`crc_equal` / `crc_differs` between two cases).
`assert.py` runs them:

```
python3 tests/golden/assert.py     # 0 = pass, 1 = fail, 2 = not captured yet
```

## What the CRC covers (and what it cannot)

The CRC (`DQIX_CRC`) is one 64-bit hash per frame of engine A's composed
256x192 picture, written from `g_screen` in `runtime/gpu2d.c`. That is exactly
"what the DS would show", and nothing that is drawn after composition:

| outside the CRC | where it happens | checked by |
|---|---|---|
| DS Lite colour correction | fragment shader, `gl_render.c` (`u_lcdcolor`) | `DQIX_GL_DUMP` on a GL-capable machine |
| HD letters and their shadows | GL letter pass, `text_hd.c` (`g_tx_shadows`) | `DQIX_TXDUMP` / `DQIX_GL_DUMP` |
| which screen the window shows | layout, `frontend_sdl.c` (`canvas_layout_for`) | screenshot of the window, not the screens |

Matrix cells for those options are asserted `crc_equal_postprocess`: the CRCs
**must be identical**, because such an option must never reach into the
emulated framebuffer. The assertion is the guard against a leak; it is not a
claim that the option was seen working. Reading the cell as "the option does
nothing" would be wrong, which is why the kind has its own name.

## Classic pixel-identical

`classic_title` is the reference. `classic_enhanced_opts` and `classic_wide`
must produce byte-identical CRCs (options ignored in Classic). Any drift fails
the assertion.

## Maturity

Cases tagged `experimental` exercise options still labelled experimental in
`docs/OPTIONS.md`, the in-game menu and `README.md`.
