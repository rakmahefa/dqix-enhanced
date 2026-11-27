# Enhancement options — `upstream-dev`

Single source of truth for every enhancement the game ships. Each entry: id,
settings key, default, maturity, what it interacts with, rollback. The menu,
`settings.ini`, replay metadata and docs are generated from this table; an
option that exists in code but not here is a bug.

## Maturity levels

- **experimental** — opt-in, labelled in the README, may break or be removed.
- **beta** — on by default for some users, known gaps documented, rollback tested.
- **stable** — on by default, golden-checked, no known conflicts.

## Options

| id | settings key | default | maturity | interacts with | rollback |
|---|---|---|---|---|---|
| `mode` | `mode` | `classic` | stable | (root toggle; Classic vs Enhanced) | switch to classic |
| `render_scale` | `render_scale` | `auto` | stable | enhanced only; window size | set to 1 |
| `smooth_textures` | `smooth_textures` | `0` | stable | enhanced, render_scale > 1 | set to 0 |
| `outlines` | `outlines` | `thin` | stable | enhanced, render_scale > 1 | set to `ds` |
| `soft_shadows` | `soft_shadows` | `0` | beta | enhanced, render_scale > 1 | set to 0 |
| `smooth_shading` | `smooth_shading` | `0` | beta | enhanced, render_scale > 1 | set to 0 |
| `hd_text` | `hd_text` | `0` | beta | enhanced | set to 0 |
| `hd_text_shadows` | `hd_text_shadows` | `0` | experimental | hd_text | set to 0 |
| `texture_packs` | `texture_packs` | `0` | beta | enhanced | set to 0 |
| `music_packs` | `music_packs` | `0` | beta | — | set to 0 |
| `music_song_names` | `music_song_names` | `0` | stable | music_packs | set to 0 |
| `transmog` | `transmog` | `0` | beta | — | set to 0 |
| `dslite_filter` | `dslite_filter` | `0` | stable | — | set to 0 |
| `lcd_colors` | `lcd_colors` | `0` | stable | dslite_filter | set to 0 |
| `pixel_grid` | `pixel_grid` | `0` | stable | — | set to 0 |
| `motion_blur` | `motion_blur` | `0` | experimental | — | set to 0 |
| `backlight` | `backlight` | `0` | stable | dslite_filter | set to 0 |
| `screen_gap` | `screen_gap` | `0` | stable | — | set to 0 |
| `console` | `console` | `0` | stable | — | set to 0 |
| `integer_scaling` | `integer_scaling` | `0` | stable | — | set to 0 |
| `actual_size` | `actual_size` | `0` | stable | — | set to 0 |
| `layout` | `layout` | `normal` | stable | widescreen, dynamic widescreen | set to `normal` |
| `single_screen` | `single_screen` | `0` | stable | layout | set to 0 |
| `widescreen_popout` | `widescreen_popout` | `0` | beta | layout=wide | set to 0 |
| `widescreen_battle_hud` | `widescreen_battle_hud` | `0` | beta | layout=wide | set to 0 |
| `walk_any_direction` | `walk_any_direction` | `0` | stable | — | set to 0 |
| `upscale_textures` | `upscale_textures` | `0` | experimental | texture_packs | set to 0 |
| `music_pack_volume` | `music_pack_volume` | `100` | stable | music_packs | set to 100 |
| `discord` | `discord` | `0` | beta | — | set to 0 |

## Interaction matrix

Conflicts are either fixed or documented as unsupported. Unresolved cells are
marked `?` and block M3 exit.

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
| widescreen × single_screen | ? | unresolved |
| hd_text × hd_text_shadows | shadows on HD letters | supported |
| dslite_filter × lcd_colors | color correction | supported |

## Notes

- `mode` is the root toggle: Classic = DS's own 3D renderer (pixel-exact),
  Enhanced = 3D on the GPU at 1-4x.
- Options with maturity `experimental` must be labelled as such in the README
  and are opt-in.
- Every new enhancement must enter this table in the same PR that adds it.
