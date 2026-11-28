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

## Classic pixel-identical

`classic_title` is the reference. `classic_enhanced_opts` and `classic_wide`
must produce byte-identical CRCs (options ignored in Classic). Any drift fails
the assertion.

## Maturity

Cases tagged `experimental` exercise options still labelled experimental in
`docs/OPTIONS.md`, the in-game menu and `README.md`.
