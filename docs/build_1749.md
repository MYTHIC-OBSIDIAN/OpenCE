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
  what they scale as it is (homing against the player and the melee delay
  had numbers of their own, below).
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
- **No taking a seat someone holds.** 1749's executable has no eviction:
  its player actions are reload, pick up, exit, swap, enter, touch and
  flip, so a marine at the wheel keeps it and the player is offered the
  free seats. It flips an overturned vehicle whoever is at its wheel.
  halo1749.exe does the same (`unit_find_nearby_seat`,
  `player_examine_nearby_vehicle`).
- **Flipping a vehicle.** This build turns an overturned vehicle by a
  roll it pins to the vehicle tag's minimum and maximum flipping angular
  velocities, which 1749's tags do not have, so the roll came out nothing
  and the vehicle never moved. 1749's code pins the roll itself, to 0.2–1
  (0.2–0.75 for a ghost), and turns the vehicle for its 30 ticks however
  upright it gets, about the plain axis, without touching its velocity.
  halo1749.exe does the same (`vehicle_update`).
- **Stepping off a ladder.** When a biped walks off the edge of the
  surface it stands on onto a steep surface beyond (the top or bottom of a
  ladder), this build snapped it onto that surface's plane, took the
  velocity it had off the plane and faked a collision to hold it there.
  1749's executable instead moves the biped on, as an ordinary collision
  move, to the nearest point of the edge it is crossing, keeps what that
  move hit as its collisions, and turns its velocity from the old
  surface's plane onto the new one, keeping all of it up to a 45 degree
  bend, none past 135, and a share in between. If the move hits nothing
  the biped is not stuck to the new surface. halo1749.exe does the same
  (`biped_update_physics`).
- **Vehicle physics.** Compared function by function with 1749's
  `physics.c` and `vehicles.c`, these differ, and halo1749.exe does as
  1749 does:
  - *The ground.* 1749 reads none of a physics tag's ground values: every
    vehicle on the newer physics pushes off the ground with friction 0.2,
    depth 0.15, damping 0.05 and normal limits 0.5 to 0.707
    (`physics_compute_new`). The warthog's tag says friction 0.1 and the
    ghost's has zeros, which this build divided by. A material scales the
    friction of a vehicle of any mass, where this build skips those over
    7,500.
  - *Hitting things.* Where a mass point would pass through something,
    this build cuts the velocity into the surface, up to four times a
    tick. 1749 instead leaves the vehicle where it is for that tick with
    its velocity whole, and the ground's push then sends it back up: the
    warthog's hop off bumps and ledges (`physics_update_new`).
  - *Other vehicles.* 1749's mass points, wheels and antigravity test
    other vehicles too, by those vehicles' physics, so vehicles meet as
    they meet the ground. They push each other with no collision forces
    (`COLLISION_TEST_FOR_VEHICLE_PHYSICS_FLAGS`, `physics_update`). What a
    vehicle stands on keeps its shield (`compute_ground_plane`).
  - *The ghost.* It holds itself up by its antigravity alone, at 30%
    while jump is held. It pushes toward the speed the stick asks for and
    turns as the stick steers, only while fairly upright (up.k of 0.2 or
    more) and out of deep water. It has none of this build's hover,
    levelling, boost or recovery forces
    (`update_alien_scout_physics_build_1749`). Its hover is still worked
    out, as this build's AI drives by it.
  - *The banshee.* It accelerates by how far the stick is pushed rather
    than by its speed, and keeps no thrust. The pelican's hover is set at
    once rather than eased.
  - *Crashes.* A knock plays the crash sound from 0.026 a tick (full at
    0.042) and does no damage. Ghosts and banshees throw up no thruster
    dust. A stopping vehicle's wheels stay as they are, and a hovering
    one is animated as hovering.

  A vehicle still knocks bipeds aside and runs them over as this build's
  does, from the vehicle's side. 1749 does it from the biped's side
  (`biped_update`), which is not ported.
- **The end of a game.** 1749's carnage report has its own line, "X:
  next game, A: replay, B: quit", for every machine: A takes the host back
  to the pregame (as this build's does), B leaves, and X has the host take
  its playlist's next stage (`build_1749_playlist_next`). Its title is the
  score to the end, with ": 2 lives left" where the game has lives, and no
  "You won" or "Game ends in a draw". In the pregame a game type is named by
  its engine alone (1749's text has no Assault, Rally or Juggernaut), and
  there are no "waiting for" or "teams ready" directions
  (`game_engine.c`, `ui_widget_game_data_input_functions.c`).
- **Damage.** 1749's executable dealt damage by older rules, which
  halo1749.exe follows (`damage.c`, `units.c`, `unit_dialogue.c`):
  - A headshot from a weapon that can cause one kills any body, a player's
    in the campaign too; there is no multiplayer headshot (double damage).
  - A fall hurts the player as the difficulty scales any damage; a vehicle
    takes damage with no driver ("only hurt while occupied" is unknown).
  - A vehicle with no child damage fraction passes no hit to its riders.
  - The flamethrower sets a unit that runs around flaming (grunts, jackals)
    alight: it runs, burning, and dies when it has burned.
  - A player's melee hurts what it hits, vehicles and scenery too, the
    nearest object (a biped before anything else), and the rider rather
    than his vehicle. It comes from the head with no direction, so the body
    it kills flies up. It pushes no vehicle, breaks no glass and opens no
    door; an AI's blow, always its own, opens a door that opens to a melee
    attack.
  - A shield cut short while charging goes on sounding charging until it is
    full. A unit hit while zoomed unzooms without the zoom out sound.
  - The AI's pain and death cries: a hit on the head (three times the
    damage) brings the instant death cry, major pain four times in five,
    minor pain two in five, speaking or not, and the waits between them are
    45 and 15 ticks.
- **The sniper scope's side ticks.** 1749's sniper rifle HUD has two blue
  tick scales at the sides of the scope, which follow the view's pitch and
  fade in as it zooms. Their fade runs from 0 to 255 in 1749's tags, which
  its executable drew as full, where this build's run 0 to 1: read so, 255
  was packed into an alpha of 1/255 and the ticks never showed.
  halo1749.exe holds a HUD fade to 0 to 1 (`hud_draw.c`).
- **The AI.** halo1749.exe's AI is being compared with 1749's executable,
  function by function; where they differ it acts as 1749's did:
  - A melee charge closes to within 4 world units of its target and goes
    if it can move there (`action_charge.c`). It is not made at a target
    in a vehicle or a swarm, whatever the weapon; between attacks only an
    armed, unberserk actor waits, by its own time at any difficulty; with
    charging disabled the margin is half a unit (`actions.c`).
  - A flying vehicle strafes a target within its combat range, and stops
    only within its stop range (`actions.c`).
  - Fleeing: no standing still in a blind panic, one cry at the start (only
    the burning cry again), no idle flee cries (`action_flee.c`).
  - A path keeps clear of no danger zone, and its attractor is no obstacle
    (`actor_firing_position.c`, `path_obstacle_avoidance.c`).
  - A vehicle the scripts make enterable is so within 5 world units, not 8
    (`ai_script.c`).
  - Homing projectiles turn at a player's biped, outside multiplayer, at
    half their rate on Normal and three quarters on Hard, by 1749's own
    numbers rather than the globals' row (`projectiles.c`).
  - Noticing. This build lets an actor notice what it perceives at a
    speed from a table of what it knows by how well it perceives. Each
    speed is an awareness rate in the actor's tag. 1749's tags keep only
    two rates, at the place of this build's non-combat awareness time and
    combat rate. The guard and non-combat rates this build reads are zero,
    so an actor not already searching never noticed the player and woke
    only when shot. 1749 has no table. An actor notices at once what it
    sees or hears fully. It notices what it sees partly over the first of
    its two rates, or the second once it is wary (`actor_perception_update`).
  - Sight. 1749's actors see within a box ahead of their eyes that widens
    with distance (its tag's vision distance, and two widths and two
    slopes where this build reads vision angles). They see fully within
    its nearer half, or partly if half hidden. Out of combat the box is
    half as long and wide, so an actor not yet fighting sees a player
    fully only within about 10 world units ahead of it (with the usual
    40-unit sight). Light, fog and what the actor knows
    change nothing, and nothing seen is unmistakable
    (`actor_visibility_at_point`).
  - Hearing. An actor hears anything it hears at all fully, never
    unmistakably (`actor_audibility_at_point`).
  - Actor flags. 1749's actor tags hold the actor's unit reference where
    this build's hold a second word of flags. Read as flags, its group
    ('bipd') made every actor stop to fire, panic with its group, never
    shoot corpses and more. 1749 has none of these flags, so halo1749.exe
    clears that word as a map loads (`cache_files.c`).
  - Special fire (overcharge or the secondary trigger). It is chosen by
    the variant's chance once its delay runs out, whatever the difficulty.
    A strafing pass fires specially only at a target in a vehicle, while
    charging it, and only after three tries that did not; a secondary
    burst once chosen is fired whatever the target
    (`actor_combat_special_fire_build_1749`).
  - Grenades. An actor whose unit has no grenades never decides to throw
    one, and none is given to it when it does (`actions.c`,
    `actor_combat.c`). It throws as soon as it decides to, without first
    turning to face the target (`actor_action_try_to_throw_grenade`).
  - Steering round things (flying vehicles: banshees, dropships). 1749
    looks 7 units ahead along five sense rays and three rings of eight
    avoidance rays (this build: 12 units, nine and two), and counts
    vehicles and scenery but not machines. It turns the vehicle's
    movement itself, up to 60 degrees, and hands how urgent that is
    straight to the steering, where this build works out a rotation and
    blends it in over several ticks. In a tight spot facing away from
    the clearest way, it sets the movement to that way in the vehicle's
    own axes, as 1749 did (`actor_move_vector_avoidance_build_1749`).
  - Moving. An actor with a projectile stuck to it runs about as if on
    fire. Anyone in a vehicle, passengers too, takes the vehicle branch,
    which reads only the vehicle's oversteer bounds (no steering limit or
    throttle limit), has no hovering drivers and no stuck check for
    fliers. Only gunners turn freely in a vehicle. Asleep actors and
    actors about to throw a grenade are not held facing
    (`actor_move_update`). Actors don't slow down as they near their
    destination. Oversteer acts only when an actor must face exactly: the
    turn is kept within the upper bound, raised by the avoidance's
    urgency, and once past the lower bound held at the largest turn seen
    (`actor_move_calculate_movement`).
  - Who an actor keeps track of. An actor at peace keeps track of a
    friend's body within 8 units (4 in combat), never an enemy's body,
    whatever its encounter is doing; this build tracked enemy bodies for
    five seconds and any body within 15 units while the encounter was
    idle. A pursuit coordinator's friends don't count as closer
    (`actor_perception_desire_prop`).
  - Chatter. 1749 replies from its own table of 39 replies (this build's
    has 45), mostly less often, with no reply delays and no timers that
    stop a reply being repeated. Repliers are sought within 7 units (this
    build: 9), "close" means within 3 units (this build: 5), and the
    reply that needs a fighting elite also needs the elite close. An
    actor joins in on what it was told only about someone it is noticing
    or knows of; a death is reported only by an actor that can see, hear
    or otherwise sense the body. Actors make no idle noises
    (`ai_communication.c`, `actors.c`).
  - What sets actors talking. 1749's own table of 93 lines is used (this
    build's has 104). It has no lines for a lone flee, or after combat
    (alone, unscathed, wounded, massacre, triumph, checking bodies,
    shooting corpses, celebrating). Its actors make no hand signals when
    they sight or find an enemy, are blocked, uncover, advance or take
    cover. Five lines on a death weigh a quarter of this build's. The
    line on being damaged is said at a higher priority and may repeat at
    once; the search lines repeat sooner (0 to 5 seconds, not 8 to 25),
    and uncovering and cover lines may repeat at once
    (`global_dialogue_table_build_1749`).
  - How often actors talk. The timers that keep a team from talking over
    itself hold back talk, communicate and shout lines too (this build's
    minimum delays let those through every time), and a talk line near a
    player waits 3 seconds after the last talk, not 2. Exclamations are
    also weighted down when said recently. A unit that spoke recently is
    not faded back in over two seconds, and filler and chatter are spoken
    at idle priority, communicate at talk priority (1749's unit speech has
    no communicate priority). Any actor may make a hand signal, even one
    asleep or in the middle of a dive or similar (`ai_communication.c`).
  - Getting into vehicles. 1749 gives up on a vehicle tilted more than
    about 37 degrees (this build: 60), always prefers a gunner's seat
    (this build lets a variant prefer passenger seats), waits a second
    and a half rather than one before faking its way into a seat it is
    stuck beside, and has no check for being stuck on the way
    (`action_vehicle.c`).
  - Glancing at things. 1749 has no glance at a detonation (its look
    types have no such entry), lets a sleeping actor glance, and draws an
    actor's glance-length modifier straight from its tag's bounds, used
    when positive, where this build held it within half and double
    (`actor_look_secondary`).
  - Standing down. An encounter stays up as long as any of its actors
    knows of a living enemy, however long since it was seen, and stands
    down as soon as none does; this build stood down 15 seconds after
    losing sight of its enemies and went through post-combat first.
    Bodies of those killed before the stand-down are then ignored
    (`encounter_update_status`, `encounter_stand_down`).
  - Following a path. A step is passed once its point still lies ahead
    along the next step and the actor is within a quarter unit of that
    step's line; a path always ends when its last step is reached; and a
    flier with no path stops rather than flying on ahead
    (`actor_destination_update`).
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
