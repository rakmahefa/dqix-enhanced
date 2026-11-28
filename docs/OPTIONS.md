# Enhancement options — `upstream-dev`

Single source of truth for every enhancement the game ships: `docs/options.json`.
Each entry declares the id, settings key, default, maturity, what it interacts
with, rollback, whether it changes the picture and where it appears in the menu.
The table below, the README's experimental list, the bug report's `display.txt`
and the golden corpus are checked against that declaration by
`tools/options_model.py`; an option that exists in code but not in the model is
a failing check, not a comment.

Regenerate the table with `make options` (it also re-runs the check). An
option that exists in the code but not in the model fails `make test`.

## Maturity levels

- **experimental** — opt-in, off by default, labelled in the README and the
  menu. It may change or be removed between versions.
- **beta** — on by default where it makes sense, known gaps documented,
  rollback tested.
- **stable** — on by default, golden-checked, no known conflicts.

## Options

<!-- options:begin (generated from docs/options.json by tools/options_model.py) -->

| id | settings key | default | maturity | interacts with | in bug report | rollback |
|---|---|---|---|---|---|---|
| `mode` | `mode` | `classic` | stable | — | yes | set to classic |
| `render_scale` | `render_scale` | `auto` | stable | `mode`, `outlines`, `soft_shadows`, `smooth_shading` | yes | set to 1 |
| `smooth_textures` | `smooth_textures` | `0` | stable | `mode`, `render_scale`, `texture_packs` | yes | set to 0 |
| `outlines` | `outlines` | `thin` | stable | `mode`, `render_scale` | yes | set to ds |
| `soft_shadows` | `soft_shadows` | `0` | beta | `mode`, `render_scale` | yes | set to 0 |
| `smooth_shading` | `smooth_shading` | `0` | beta | `mode`, `render_scale` | yes | set to 0 |
| `hd_text` | `hd_text` | `0` | beta | `mode`, `hd_text_shadows` | yes | set to 0 |
| `hd_text_shadows` | `hd_text_shadows` | `0` | experimental | `hd_text` | yes | set to 0 |
| `texture_packs` | `texture_packs` | `0` | beta | `mode`, `smooth_textures`, `upscale_textures` | yes | set to 0 |
| `music_packs` | `music_packs` | `0` | beta | `music_pack_volume`, `music_song_names` | no | set to 0 |
| `music_song_names` | `music_song_names` | `0` | stable | `music_packs` | no | set to 0 |
| `transmog` | `transmog` | `0` | beta | — | yes | set to 0 |
| `dslite_filter` | `dslite_filter` | `0` | stable | `lcd_colors`, `pixel_grid`, `motion_blur`, `backlight` | yes | set to 0 |
| `lcd_colors` | `lcd_colors` | `0` | stable | `dslite_filter` | yes | set to 0 |
| `pixel_grid` | `pixel_grid` | `0` | stable | `dslite_filter`, `window_scale` | yes | set to 0 |
| `motion_blur` | `motion_blur` | `0` | experimental | `dslite_filter` | yes | set to 0 |
| `backlight` | `backlight` | `max` | stable | `dslite_filter` | yes | set to max |
| `screen_gap` | `screen_gap` | `0` | stable | `layout`, `console` | yes | set to 0 |
| `console` | `console` | `0` | stable | `console_color`, `screen_gap` | yes | set to 0 |
| `console_color` | `console_color` | `dslite_black.png` | stable | `console` | yes | set to dslite_black.png |
| `integer_scaling` | `integer_scaling` | `0` | stable | `window_scale`, `actual_size` | yes | set to 0 |
| `actual_size` | `actual_size` | `0` | stable | `monitor_width_mm`, `integer_scaling` | yes | set to 0 |
| `monitor_width_mm` | `monitor_width_mm` | `0` | stable | `actual_size` | yes | set to 0 (automatic) |
| `window_scale` | `window_scale` | `2` | stable | `integer_scaling`, `pixel_grid`, `actual_size` | yes | set to 2 |
| `layout` | `layout` | `stacked` | stable | `mode`, `single_screen`, `widescreen_popout`, `widescreen_battle_hud`, `small_screen_corner`, `small_screen_size`, `small_screen_opacity` | yes | set to stacked |
| `single_screen` | `single_screen` | `0` | stable | `layout` | yes | set to 0 |
| `small_screen_corner` | `small_screen_corner` | `0` | stable | `layout` | yes | set to 0 |
| `small_screen_size` | `small_screen_size` | `1` | stable | `layout` | yes | set to 1 |
| `small_screen_hidden` | `small_screen_hidden` | `0` | stable | `layout`, `single_screen` | yes | set to 0 |
| `small_screen_opacity` | `small_screen_opacity` | `50` | stable | `layout` | yes | set to 50 |
| `swap_screens` | `swap_screens` | `0` | stable | `layout` | yes | set to 0 |
| `widescreen_popout` | `widescreen_popout` | `0` | beta | `layout` | yes | set to 0 |
| `widescreen_battle_hud` | `widescreen_battle_hud` | `0` | experimental | `layout` | yes | set to 0 |
| `second_window` | `second_window` | `0,0,0,0` | stable | `layout` | yes | set to 0,0,0,0 |
| `second_window_fullscreen` | `second_window_fullscreen` | `0` | stable | `second_window` | yes | set to 0 |
| `fullscreen` | `fullscreen` | `0` | stable | `window_scale`, `integer_scaling` | yes | set to 0 |
| `walk_any_direction` | `walk_any_direction` | `0` | stable | — | no | set to 0 |
| `upscale_textures` | `upscale_textures` | `0` | experimental | `texture_packs`, `mode` | yes | set to 0 |
| `music_pack_volume` | `music_pack_volume` | `100` | stable | `music_packs` | no | set to 100 |
| `discord` | `discord` | `0` | beta | — | no | set to 0 |
| `downloadable_content` | `downloadable_content` | `0` | stable | — | no | set to 0 |

<!-- options:end -->

## Maturity contract

Each level has written entry and exit criteria in `docs/options.json`
(`maturity_levels`). Promotion is a one-line edit of `default` + `maturity`
there, with the README list and `docs/OPTIONS.md` regenerated in the same PR.
A regression demotes the option in the same PR that records it in
`MILESTONES.md`.

| level | entry (summary) | exit (summary) |
|---|---|---|
| experimental | declared with default 0, in the README list and the menu, rollback recorded, kept in bug reports | effect recorded on a GL machine, no out-of-scope CRC drift, one clean nightly cycle, a stable default story |
| beta | promoted from experimental with its exit criteria met, gaps written down | a golden case or matrix cell exercises it, no matrix conflict, cost recorded |
| stable | promoted from beta, covered by the corpus, rollback from `settings.ini` alone | a regression demotes it to beta and is recorded |

Known gap for every post-composition option: the automated environment has no
display or OpenGL, so the *visual* half of the exit bar (window screenshots)
is run by hand on a GL machine. That is why no experimental option was promoted
in cycle 1 — promoting on no evidence would empty the word "beta".

### Cycle 1 decisions (experimental features)

Each experimental option stays **opt-in, off by default and labelled**; none is
promoted without the evidence its exit criteria name. The decisions and their
exit criteria live in `docs/options.json` (`experimental_decisions`):

| option | decision | what promotion needs |
|---|---|---|
| `hd_text_shadows` | keep opt-in | GL capture of the HD-text corpus cases, identical CRCs, one clean nightly cycle |
| `motion_blur` | keep opt-in | before/after window captures, frame-time cost in `docs/PERF.md`, no CRC drift |
| `upscale_textures` | keep opt-in | a corpus run with a small bundled pack, GL capture, no crash/leak over a session |
| `widescreen_battle_hud` | keep opt-in | wide-layout battle window captures with/without the HUD, false-positive audit, identical CRCs |
| `layout = wide_dynamic` | keep the value experimental | window captures of a field, a menu and a battle; no CRC drift against wide; one clean nightly cycle |

## Interaction matrix

Conflicts are either fixed or documented as unsupported. Unresolved cells are
marked `?` and block M3 exit. No cell is unresolved as of cycle 1; each
`supported` cell names the code path that defines the behaviour, which is what
the golden corpus asserts.

| combo | result | status |
|---|---|---|
| Classic × any enhanced option | enhanced options ignored | supported |
| Enhanced × widescreen (wide) | 3D renders in 16:9 | supported |
| Enhanced × Dynamic Widescreen | scene-aware widescreen | supported |
| Enhanced × HD text | letters redrawn at full res | supported |
| Enhanced × DS Lite filter | filter applied to final image | supported |
| Enhanced × mods (texture) | PNGs replace game textures | supported |
| Enhanced × mods (music) | OGG/FLAC replaces game songs | supported |
| Classic × mods (texture) | mods disabled (Classic only) | supported |
| Classic × widescreen | widescreen disabled | supported |
| widescreen × single_screen | widescreen wins: `single_screen` is ignored (it only applies to the stacked, side-by-side and corner layouts) and U / the left trigger hides the small screen instead, i.e. `small_screen_hidden` (`MI_SINGLE` ticks and toggles that one in widescreen) | supported (`frontend_sdl.c`: `single = g_single && !wide` in `canvas_layout_for`; `menu_check(MI_SINGLE, wide ? g_small_hide : g_single)`; switching to a wide layout clears `g_single`) |
| hd_text × hd_text_shadows | shadows on HD letters | supported |
| dslite_filter × lcd_colors | color correction | supported |

## Replay completeness

A bug report must reproduce what the player saw, so every option that changes
the picture is copied into the report's `display.txt`
(`report_display`'s `KEEP` list in `runtime/frontend_sdl.c`). Widescreen and
Dynamic Widescreen are both `layout` values and are kept, together with the
widescreen small-screen geometry, the battle HUD, HD text and its shadows, the
DS Lite filter and its parts, scaling, the console shell and the mod switches.
`tools/options_model.py` fails if an option marked `affects_picture` is missing
from that list.

What the report cannot carry is the player's own content: a texture or music
pack's files, `transmog.ini`, a custom `fonts/` pair. `display.txt` records the
switches that were on; the folder is expected to be attached with the packs it
uses. Personal and network settings (name, MAC, server) stay out of it on
purpose.

## Notes

- `mode` is the root toggle: Classic = DS's own 3D renderer (pixel-exact),
  Enhanced = 3D on the GPU at 1-4x.
- Options with maturity `experimental` must be labelled as such in the README
  and are opt-in; `README.md` has an **Experimental options** list for exactly
  the rows marked `experimental` here (`hd_text_shadows`, `motion_blur`,
  `upscale_textures`, `widescreen_battle_hud`). The `wide_dynamic` value of
  `layout` (Dynamic Widescreen) is also experimental and is labelled
  `(Experimental)` inline in the README and the in-game menu. Adding or
  promoting an experimental option means editing that list in the same PR; the
  conformance check fails until it is done.
- Three rows act **after** the DS screens are composed and therefore cannot
  show up in the golden corpus' frame CRC (`tests/golden/corpus.json`): `dslite_filter` ×
  `lcd_colors` (fragment shader), `hd_text` × `hd_text_shadows` (the GL letter
  pass) and widescreen × `single_screen` (which screen the window shows). Those
  cells are asserted `crc_equal_postprocess`: identity is the requirement there
  — the option must not change the emulated picture — and their own effect is
  checked with `DQIX_GL_DUMP` on a GL-capable machine or a window screenshot.
  See `tests/golden/README.md`, "What the CRC covers".
- Every new enhancement must enter `docs/options.json` in the same PR that adds
  it; `make test` runs the model check before anything else.
