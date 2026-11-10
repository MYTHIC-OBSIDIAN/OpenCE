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

A profile copied off the Xbox with the dump (`1749\savegame\UDATA\<id>\`,
whose `SaveMeta.xbx` is a `Name=` line) is used as it is. This build names
its own profiles' folders after their names, and finds the others by the
name in their `SaveMeta.xbx`, as the Xbox did
(`port/linux/src/xbox_xapi.c`): renaming such a profile deletes its old
folder. Before 1749-build-8 it didn't, so the profile was listed twice
after a restart, and the old folder can be deleted.

## What differs from this build's maps

The comparison is of 1749's `ui.map`, its ten campaign levels (`a10` to
`d40`) and thirteen multiplayer maps, and its `cachebeta.exe`
(`halobeta xbox 01.08.15.1749(CACHE)`, the same kind of build as 2342's).

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
  checks a map so. Every level and multiplayer map passes, with the same two
  kinds of correction (references left as zeros include the dialogue's
  `vocalizations`, the skies' `indoor_fog_screen` and the sniper rifle's
  split-screen mask).
- **Tags that share their data.** 1749's tools wrote a tag the same as one of
  its group already written as that one: `a10.map`'s eleven collision models
  of doors and others are one model of a single `<none>` material (`a50`,
  `c10`, `d20` and `d40` have such tags too). This
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
  nothing, and is logged. 1749's array of script nodes holds 16,384, where
  this build's holds 19,001: halo1749.exe takes a map's array of either
  size as sound (`hs_scenario_syntax_maximum_count` in `hs.c`, and the
  validator's check of the nodes), where it would otherwise run none of a
  1749 map's scripts. The value types are numbered alike: every call's
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
- **The menus' lists.** This build's game data inputs find a list item's
  parts by their order among its children, and three of 1749's lists have
  fewer: the game types' and playlists' items have no container around the
  description (the description is shown and hidden itself), the campaign
  levels' have no completion, difficulty or co-op markers, and the player
  profiles' have one text box where this build's have a container of
  labelled fields (the name and colour are filled in, and the text box
  holds the profile's description as 1749's executable wrote it: each of
  `profile_description_labels` over its value, a line each, the level
  reached, the difficulty and the joystick and button sets, or for a
  default profile its sets' long descriptions). The rest of 1749's lists
  and screens are laid out as these functions expect (the descriptions,
  option menus, server list, pregame screens and quarter-screen profile
  spinners). 1749's own code drew some of
  these screens otherwise (its asserts name a wrapper of three children
  around the level and profile lists), which this build doesn't do.
- **Difficulty.** 1749 has three difficulties, Normal, Hard and Impossible,
  numbered from 0, where this build has four from Easy. The scripts name
  them (`a10`'s opening runs the cryo bay's tutorial only on `normal`), and
  their names are this build's numbers once loaded, so halo1749.exe plays
  1749's as this build's 1 to 3: the difficulty list's item is the
  difficulty after its index (`ui_widget_event_handler_functions.c`), and
  the profile list names a difficulty from 1749's `difficulty_names`
  before it. 1749's table of the values each difficulty scales (the
  globals' `difficulty_information`) has three columns to a row where this
  build's has four, and is read so (`game_globals.c`). 1749's executable
  read only its rows of the enemies' damage, vitality, shields and
  recharge, the AI's aim and fire, grenades and upgrades, left the rest
  zero, and gave allies, and everyone in multiplayer, the enemies' values
  on Normal: halo1749.exe does the same, and the rows it didn't read leave
  what they scale as it is (homing against the player and the melee delay).
  Every other name the scripts use (teams, AI states, actor types, HUD
  corners) is numbered as this build's.
- **Controller sets.** 1749's menus have two joystick sets, DEFAULT and
  LEGACY (its "flight stick" set, and the layout `a10`'s help describes for
  it), stored as 0 and 1: halo1749.exe plays LEGACY as this build's legacy
  set and names this build's sets with 1749's two. Its three button sets are
  this build's first three.
- **The menus' handlers.** The event handlers that walk a menu's items do so
  in the order of 1749's items, but for the advanced controls, which have no
  flight stick item in 1749: halo1749.exe passes over it, as 1749's
  executable did. 1749's Slayer options list kill penalty before kill in
  order where its executable (and this build) read them the other way: so
  did 1749, and halo1749.exe keeps it.
- **Text built into 1749's executable.** 1749's HUD globals
  (`ui\hud\default`) end at the carnage report's bitmap: what follows in
  this build's (the item messages to show while loading, when done, at a
  checkpoint and after it, and the checkpoint's sound) is padding in 1749's,
  all zero, and item message 0 of 1749's `hud_item_messages` is "Picked up
  a flamethrower", which this build's code showed at every checkpoint.
  1749's executable had the text built in, "loading...", "loading...
  done", "Checkpoint..." and "Checkpoint... done", and no sound: so does
  halo1749.exe (`hud_load` and `hud_autosave` in `hud.c`). Its
  `ui\multiplayer_game_text` holds 26 strings, and the executable had the
  rest built in, as the NTSC release's did: halo1749.exe takes the PAL
  maps' English where 1749's is the same and 1749's own where it isn't
  ("Slaughterhouse!" for ten kills in a row, "Player quit out of game", "You
  are in %s place with %s %s"), and shows nothing where 1749 had no
  message ("Hold BACK for score", a flag scored, a telefrag, "%s quit")
  (`text_group.c`).
- **The game types' own text.** 1749's game engines show their text as
  this build's do, with three differences in when (1749betaP.xbe's
  engines): Slayer shows the score all the time ("first place kills 3 of
  25", as Race does here), there is no "Hold BACK for score" over a game's
  first 15 seconds, and Oddball shows the time with the ball whatever the
  ball (Hunter's and Stalker's too). halo1749.exe does the same
  (`game_engine_slayer.c`, `game_engine.c`, `game_engine_oddball.c`).
- **A wider view.** This build's camera draws a field of view's tangent
  times 0.85 (`render_camera_get_adjusted_field_of_view_tangent`); 1749's
  divides by 0.85, so the same 70 degrees are 79 across a 4:3 screen where
  this build's are 61. halo1749.exe draws 1749's, and every weapon's view
  and scope with it.
- **Quick start's game.** 1749's executable took quick start's stage from
  `d:\playlist.txt` (`1749\playlist.txt` here), an entry at random by its
  weight: a line `version 1`, then for each entry its map, its game type's
  name (`slayer`, `ctf`, `team_king`, ...) and five lines of numbers, the
  fewest and most players, the range of playlist types and the weight (1 to
  255). Without the file it played Slayer on Derelict, every time. halo1749.exe
  does the same (`build_1749_playlist_next`): it had played Derelict with no
  game type at all, so nothing the game type runs (teleporters among it)
  worked.
- **Halo's opening cutscene.** 1749's `a30` has its opening cutscene,
  `cutscene_intro` (the lifeboat's fall, "Arrival"), but none of its
  scripts runs it, and `mission_lz` waits for the conversation it ends with
  (`intro_1`), so without it the landing zone's mission never goes on.
  halo1749.exe runs it once as the level starts, as the other levels'
  startup scripts run theirs (`hs_runtime_initialize_for_new_map`).
- **Powerups.** 1749's executable flashed the screen for no powerup (no
  white flash for a health pack, none for an overshield or camouflage),
  played no pickup sound, took an overshield whether or not the shield
  could take more, and took camouflage while camouflaged, adding its time.
  halo1749.exe does the same (`player_handle_powerup_equipment`,
  `player_handle_powerup` in `players.c`).
- **Green vehicles on the motion tracker.** 1749's tracker has one kind of
  vehicle blip, green, whoever is in it, where this build's shows a vehicle
  yellow or red by its driver's team (`motion_sensor.c`).
- **No message for a powerup walked over.** 1749's powerups have no item
  message of their own (theirs is 0, the flamethrower's), and its
  executable named a powerup only when it was swapped for: halo1749.exe
  names none walked over (`players.c`).
- **1749's game types.** 1749's executable built twenty game types of its
  own (`port/linux/game/build_1749_variants.c`, from its builders): Dark
  Stalker, Race, Team Race, Rally, Slayer, Team Slayer, Killer,
  Elimination, Hunter, Stalker, Team Oddball, Accumulation, Magic,
  Oddball, Crazy Ball, Assault, Capture the Flag, King of the Hill, Team
  King of the Hill and Iron CTF, which its menus name in that order but
  for Iron CTF. halo1749.exe had built this build's 26 under 1749's names,
  so its "Slayer" was Endurance (five lives and odd man out, whose message
  came at each death): it now makes the menus' default game types, the
  game types by name, and a new game type's start (Slayer) 1749's.
- **Swapping weapons on the press.** This build swaps for a weapon on the
  ground when the action button has been held for the globals'
  `minimum_weapon_swap_ticks`. 1749's globals have no such field (it is
  padding there, zero), and its executable swapped as the button went down,
  without reloading: halo1749.exe does the same (`player_control.c`,
  `players.c`). Read as zero, the swap had fired with no button held, once,
  and never again.
- **Weapons the player can't carry.** A unit picks up a weapon only if its
  animations have that weapon's label, in 1749's executable as in this
  build's. 1749's cyborg has `ar`, `sg`, `sr`, `pr`, `pb`, `f` (rifles),
  `hp`, `pp`, `ne`, `b` (pistols), `rl` and `ft`, but not the fuel rod
  gun's `fr` nor the energy sword's `fb`, so in `c10` and `d20` the fuel rod
  guns on the floor stay there, as they did in 1749.
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

The picture is always the Xbox's 4:3: the game draws 640 wide, as the Xbox
did, whatever the display's or window's shape, and a wider one shows it
pillarboxed in black (`screen_mode_choose` in
[d3d8_gl.c](../port/linux/src/d3d8_gl.c)). The view is 1749's, not widened,
and the HUD and menus keep their places.

## Icon

halo1749.exe's icon is Halo Custom Edition's (`port/windows/build-1749-icon.ico`). halo.exe holds it
as its second icon (`port/windows/halo.rc`), and when CI makes halo1749.exe from halo.exe it makes that
icon the first (`tools/pe_icons.py`), which Explorer, the taskbar and the window show. Only the two
entries of the resource directory change: the code is halo.exe's, so `halo.pdb` still reads its
crashes.

## Build numbers

halo1749.exe gives the game's build as 1749's executable does
(`halo_game_build_number`, in [build_1749.c](../port/linux/src/build_1749.c)):
`01.08.15.1749` in the menus' build number, the first line of each
`debug.txt` (`halobeta xbox 01.08.15.1749(CACHE)`) and a saved game's
header, where halo.exe gives `01.01.14.2342`.

A build of the `build-1749` branch names its release, `1749-build-<n>`
(`HALO_BUILD_1749_NUMBER`, which the workflow works out before it builds
and tags the release with): the console's `version`, the error screen and
the dedicated server say `OpenCE Windows | 1749-build-<n> (release)`, and
halo1749.exe adds `| halobeta xbox 01.08.15.1749`. Its crash reports are
named `...-1749build<n>-<flavor>`, and the crash lines in `debug.txt` and
`halo.log` name `1749-build-<n>`. It has no standard build number, so it
never looks for the standard line's updates (`halo.log` says so).

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
