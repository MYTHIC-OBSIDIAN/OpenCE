# Build 1749 (halo1749.exe)

The Windows release ships `halo.exe` twice. Its copy, `halo1749.exe`, runs as
Halo's Xbox build **01.08.15.1749** (15 August 2001, before the retail
game), on that build's own maps, as stock. It is the same program; the name
decides the mode ([build_1749.c](../port/linux/src/build_1749.c)). Any
executable whose name begins `halo1749`, or `HALO_BUILD=1749` in the
environment, runs the same way.

Nothing of build 1749 is in this repository or the release. The player
copies their own dump into the `1749` folder beside the executable:

| Path | What |
|---|---|
| `1749\maps\ui.map` | the build's menus |
| `1749\maps\*.map` | its levels |
| `1749\default.xbe` | optional: its executable |
| `1749\bink\` | optional: its videos |
| `1749\config.toml` | its own settings, written on the first start |
| `1749\savegame\` | its own profiles, saved games and cache files |

`paths.data` and `paths.saves` in `1749\config.toml` move them.

## What differs from this build's maps

The comparison is of 1749's `ui.map`, `a10.map`, `b30.map`, `c40.map` and
`wizard.map` and its `cachebeta.exe` (`halobeta xbox 01.08.15.1749(CACHE)`,
the same kind of build as 2342's).

- **The tag cache's address.** 1749's maps were linked to a tag cache at
  `0x803AF000`, 36 KB above this build's `0x803A6000`. halo1749.exe puts its
  tag cache there (`physical_memory_map.c`, `halo_tag_cache_base_address`),
  so the maps load as they are. It ends at `0x819AF000`, below the native
  game state at `0x81A00000` ([halo_port_capacity.h](../port/linux/include/halo_port_capacity.h)).
  A map of the other address is refused by name: 1749's in halo.exe, this
  build's in halo1749.exe (`cache_file_tag_header_verify`).
- **The tags.** Their layout is this build's. Read at 1749's address, every
  tag of `ui.map` (1,282 tags of 45 groups) and `b30.map` (2,790 tags of 64
  groups: the AI, encounters, vehicles, devices, cutscenes and both
  structure bsps among them) passes the tag validator. Its only corrections
  are of the same two harmless kinds: references 1749 left as zeros in
  fields this build reads later (weapons' zoom and pickup sounds, globals'
  night vision and co-op respawn effects, the HUD's checkpoint sound), and
  bitmap sequences of no bitmaps. `tools/map_validate --build-1749 map.map`
  checks a map so. `a10.map`, `c40.map` and `wizard.map` pass as well, with
  the same two kinds of correction (references left as zeros include the
  dialogue's `vocalizations`, the skies' `indoor_fog_screen` and the sniper
  rifle's split-screen mask).
- **Tags that share their data.** 1749's tools wrote a tag the same as one of
  its group already written as that one: `a10.map`'s eleven collision models
  of doors and others are one model of a single `<none>` material. This
  build's validator refuses a tag whose bytes another holds, so 1749's maps
  are checked with `tag_validate_build_1749_tags`
  ([tag_validate.c](../port/linux/game/tag_validate.c)): a tag whose root
  starts where an earlier tag of its group's does is taken as the same bytes,
  walked once, through the earlier tag. Anything else that overlaps is still
  refused, and this build's maps are checked as before.
- **The scripts.** 1749 numbers its script functions otherwise: 614 of
  b30.map's 1,209 calls (89 functions) have 1749's numbers. Every function
  b30 calls is this build's, by name, and the scripts keep the names, so
  each call is given this build's number as the map loads
  (`build_1749_scripts_convert`, in
  [custom_edition_scripts.c](../port/linux/game/custom_edition_scripts.c),
  as Custom Edition maps' are). A function this build lacks would do
  nothing, and is logged. The value types are numbered alike: every call's
  type in b30 is its function's, or that of where it is used. `a10.map`,
  `c40.map` and `ui.map` call no function this build lacks either.
- **Engine globals that are functions here.** `a10.map`'s cryo bay sets the
  engine global `player0_look_invert_pitch` to try the player's look
  inverted, and reads it to say which was kept; this build has functions
  for that (`player0_look_invert_pitch`, which saves it to the profile, and
  `player0_look_pitch_is_inverted`). A set is made a call of the function
  with the value, in place; a read, a call of the reader, with a new script
  node for its name, written over the name of an engine global made inert
  (the scripts' loading finds functions by their names). The other,
  `player0_alternate_player_control`, which picks the help text for 1749's
  alternate controls, is made inert: it reads false, so the help is the
  normal joystick set's (`tools/harness/tests/test_scripts_1749.py`).
- **The menus' functions.** The widgets' event handler functions (1 to 94 in
  1749's maps) and game data inputs (1 to 35) are numbered as this build's
  first ones: each is used by the widget its name says (`launch_new_campaign_button`
  starts a new game, `level_select_list` lists the levels, and so on).
- **The executable's own tags.** 1749's executable loads the same levels
  (`a10` to `d40`, and the multiplayer maps of `levels\test`) and menus as
  this build's. This build also loads a few that 1749's `ui.map` lacks (the
  menus' cursor, forward and back sounds, `temp_strings`): their absence is
  logged and does nothing else.
- **No loading screen picture.** 1749's executable has none (this build's
  picture comes from the retail `default.xbe`): its loading screen is the
  light alone, over black.

## Stock

halo1749.exe holds every OpenCE addition that changes what is seen, heard
or played at its stock value, whatever `config.toml` says
(`build_1749_forced` in [port_config.c](../port/linux/src/port_config.c)):
the Xbox's menus out of 1749's `ui.map`, the Xbox's HUD and text, 30 frames
a second, the stock view and effects, no mods, no Custom Edition maps, no
voice or text chat, map sharing or votes, and no over-the-shoulder camera.
The menus take no mouse: they are driven as a controller drives them (the
keyboard still stands in for a controller). The display's own settings
(window, resolution, vsync, anti-aliasing) and the controls are the
player's.

## Reading route

1. [build_1749.c](../port/linux/src/build_1749.c): the mode, its folder and
   its tag cache address.
2. [xbox_files.c](../port/linux/src/xbox_files.c) (`platform_data_root`,
   `platform_save_root`) and [port_config.c](../port/linux/src/port_config.c)
   (`config_path`, `config_apply_build_1749`).
3. [physical_memory_map.c](../source/cache/physical_memory_map.c) and
   [cache_files.c](../source/cache/cache_files.c): the tag cache, the maps
   refused by address, and the scripts' conversion as a map loads.
4. [custom_edition_scripts.c](../port/linux/game/custom_edition_scripts.c):
   the scripts.
5. [ci_build.py](../tools/ci_build.py): the release's `halo1749.exe` and
   `1749` folder.
