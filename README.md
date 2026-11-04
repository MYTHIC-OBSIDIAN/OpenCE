# OpenCE - Halo: Combat Evolved for Windows

> **This is a LAN-only fork.** Internet play is removed: no invite links,
> server browser, master servers, Discord, UPnP, STUN or self-updater. The
> settings that would reach the internet are held off in
> `port/linux/src/port_config.c` (`lan_only_off_booleans`) whatever
> `config.toml` says; the Multiplayer menu's INTERNET items do as LAN does.
> The main menu has CREDITS in MODS' place, which plays the credits chosen
> in Video Setup > Extra Effects (CREDITS: XBOX or PC): the Xbox's credits
> movie in the game (`bink\credits.bik` beside `halo.exe`, in the data root),
> or Halo PC's credits scrolling over the main menu as Halo PC does (yellow
> headers, blue names, round and round until A, B or Escape); after The Maw
> the Xbox's movie always plays. INTRO there (XBOX or PC) picks the movie before the main menu:
> `bink\intro_xbox.bik` (else the Xbox's `intro.bik`) or `bink\intro_pc.bik`.
> Every mod in the `mods` folder is played. The Xbox's videos (the disc's `bink` folder, extracted
> with the maps) play in the game as on the Xbox: the opening titles before
> the main menu, the credits after The Maw (`port/linux/src/bink_play.c`,
> on a Bink decoder adapted from FFmpeg's, LGPL 2.1 or later). The map lists show Halo PC's maps (the multi_maps folder's) in
> MULTIPLAYER and a DIGSITE list in place of CUSTOM SINGLEPLAYER (see
> [Custom Edition Maps](#custom-edition-maps)); the gametype list has
> STANDARD (the Xbox's built-in gametypes) and CUSTOM (those saved); OpenCE's own built-in gametypes are not listed. Its
> GitHub Actions build Windows only: each push uploads
> `halo-windows-release`, and a push to `main` or `lan-digsite` publishes
> it as a release. This fork is Windows only: the Android and web builds are
> removed, and the Linux build (`ninja linux`) is kept only as a developer's
> compile check, never released.

<p align="center">
  <img src="port/assets/icon/opence-icon.png" alt="OpenCE" width="128">
</p>


<p align="center">
  <a href="https://github.com/OpenCommunityEdition/OpenCE/releases/latest"><img src="https://img.shields.io/github/v/release/OpenCommunityEdition/OpenCE?label=Download&style=for-the-badge" alt="Download"></a>
  <a href="https://discord.gg/9gqcHyr5km"><img src="https://img.shields.io/badge/Discord-join-5865F2?style=for-the-badge&logo=discord&logoColor=white" alt="Discord"></a>
</p>

**OpenCE** (Open Community Edition) is an open source port of the Halo: Combat Evolved decompilation (Halo CE) based on an early Xbox development build (2342) and forked from
[bnunu/halo-1](https://github.com/bnunu/halo-1). The project aims to enhance the existing game and adds many features, such as 128-player multiplayer, online campaign co-op, mod packs, field-of-view settings, a server browser, peer-to-peer hosting, automatic map downloads, voice chat, support for Custom Edition maps and much more! 

The game runs natively on **Windows**. Worth noting that it is not an emulator, **the game's own code is compiled for Windows** with a new layer in place of the Xbox's hardware. Runs on OpenGL. Sound, input, gamepads are all handled through SDL3. The game currently draws a frame every refresh of your display with Smooth Motion enabled, while the simulation runs at 30 ticks just like the original Xbox. 

<img width="1284" height="1007" alt="The game running on Windows" src="https://github.com/user-attachments/assets/0a4412e1-f6d3-48ed-8e17-0a374c2122f1" />
<p align="center"><em>OpenCE running on Windows (release build)</em></p>

## A copy of Halo: Combat Evolved is required

OpenCE does not include the game data. You **must provide your own Xbox disc image** (`.xiso` or `.iso`) of Halo: Combat Evolved. Every version of the game works. The maps of the European (PAL) version were made for a slower console - the game changes them to play as the North American (NTSC) maps do, so players of the two versions can play together.

1. Start the game (`halo.exe`).
2. At the first start you will get a pop up message asking for the disc image. Select it in the
   file picker. A disc image (`.iso` or `.xiso`) next to the executable is
   used without the file picker.
3. The game extracts the `maps` folder. Then the game starts.

The `maps` folder holds the Xbox maps only. If it holds Halo PC's maps
instead (a Halo PC or Custom Edition install's `maps` folder), the game says
so and stops: Custom Edition maps go in `custom_maps` folder, beside `maps` folder.

The game puts `maps` next to the executable.

## Download

GitHub Actions builds the game for each commit. 

Builds for latest release:

| Platform | Release | Debug | Guide |
| --- | --- | --- | --- |
| Windows | [halo-windows-release.zip](https://github.com/OpenCommunityEdition/OpenCE/releases/latest/download/halo-windows-release.zip) | [halo-windows-debug.zip](https://github.com/OpenCommunityEdition/OpenCE/releases/latest/download/halo-windows-debug.zip) | [Windows](port/windows/README.md) |


Release build is recommended for regular play. 

The debug build stops at the first failed check (assertion) and writes it to the log, so use it when you want to find and report problems.

Each build of the `main` or `lan-digsite` branch that passes is a new
release. The [Releases](https://github.com/OpenCommunityEdition/OpenCE/releases)
page keeps the last five releases. If the latest build has a problem, get
an older build from that page.

Each release also has `SHA256SUMS`, the SHA-256 of each of its files, and
`SHA256SUMS.sig`, that file's Ed25519 signature by the project's release
key, whose public key is [deploy/release-key.pub](deploy/release-key.pub).
`sha256sum -c --ignore-missing SHA256SUMS` checks the downloads, and
`tools/release_sign.sh verify SHA256SUMS` (OpenSSL 3) the signature.

[port/linux/README.md](port/linux/README.md) (the shared platform layer's guide, named for its folder) covers the controls, every setting and the multiplayer features; [port/windows/README.md](port/windows/README.md) the Windows build. The settings are in `config.toml` next to the game, and many are in the menus too.

## Multiplayer 

- **Supports up to 128 players** on up to 128 machines and up to 4 players on each device in split screen.
- **Peer-to-peer internet hosting:** join through the server browser or an invite link. A host lists a game as public in the server browser, or keeps it private for players with the invite link. With the Discord integration, the host can also send the invite through Discord.
- **Co-op campaign:** split screen on one computer (Multiplayer > CO-OP CAMPAIGN); the campaign is not played over the network.
- **Voice Chat:** push to talk (V) or an open microphone (can be changed in the menu's audio settings)
- **Map sharing:** a player who does not have the game's Custom Edition map downloads it from the other players.
- **Moderation:** the host can kick and ban players from the scoreboard menu or the `kick` and `ban` console commands. Players can vote to kick. 
- **Scrollable Scoreboard:** the mouse wheel or PageUp / PageDown scrolls through list of players in large multiplayer lobbies. 
- **A new netcode:** each machine moves its own player at once, and the host makes the decision for the game. Refer to [port/linux/NETCODE.md](port/linux/NETCODE.md).

## Custom Edition Maps

Halo Custom Edition maps are placed in `custom_maps` folder, not in the `maps` folder itself. It requires `bitmaps.map`, `sounds.map`, and `loc.map` in the `custom_maps` folder, which you must copy over from your existing installed copy of Halo Custom Edition. The maps folder is only for Xbox maps that's extracted from your disc image, not Halo Custom Edition. 

Halo PC's maps **are not included by default**: the Gearbox exclusive maps (Ice Fields, Death Island, Danger Canyon, Infinity, Timberland and Gephyrophobia), and Halo PC's own versions of the Xbox maps (Blood Gulch and the rest, with Halo PC's vehicle spawns: rocket warthogs, banshees and turrets). Copy them, with Halo PC's own `bitmaps.map` and `sounds.map`, from your Halo PC install's `maps` folder into a **`multi_maps`** folder beside `maps` (Halo PC's `ui.map` stays out: the game's `ui.map` must be the Xbox's). Halo PC's maps in the `maps` folder itself are not played (the game warns of them when it starts). Both Halo PC's own maps (version 7) and Custom Edition's copies of them work. They are **MULTIPLAYER**'s maps, by their names as Halo PC's list has them, with the PC menus' descriptions (the Xbox's own multiplayer maps are not listed); a map of `multi_maps` plays from there whatever other folder has one of its name. The Gearbox maps' pictures, and the menus' pictures that have no redraw, come from your own install (Halo PC's versions of the Xbox maps show the Xbox maps' pictures):

```sh
python3 tools/pc_map_pictures.py "<Halo PC or Halo CE install>/maps" <game>/multi_maps --menus <game>
```

writes `icefields.bmp`, `deathisland.bmp` and so on beside the maps, and the menus' pictures into the game's `menus` folder (any `<name>.bmp` or `<name>.txt` you put beside a map yourself wins).

The Xbox's own multiplayer maps can be played too, in the map lists' **ORIGINAL MAPS** (after MULTIPLAYER): copy any of the Xbox's 13 from `maps` into `multi_maps` with `_xbox` added to the name (`bloodgulch_xbox.map`, `sidewinder_xbox.map` and so on). They are listed by the Xbox's names, pictures and descriptions, and play as the Xbox's maps (Halo PC's versions in MULTIPLAYER stay as they are); a game on one needs each player to have it. A dedicated server's rotation entry by name (`bloodgulch:ctf`) plays `multi_maps\bloodgulch_xbox.map` too when it is there, else `maps\bloodgulch.map`.

With HIGH-RES HUD on (the default), Halo PC's own maps draw the Xbox's HUD throughout: the shield and health meters, the ammo meters, counter digits and damage arrows too, from the Xbox redraws laid out as Halo PC's sheets (`port/assets/hud/pc_layout.json`, made by `tools/pc_hud_assets.py`).

With HIGH-RES HUD on, the campaign (and every Xbox map) draws the weapons' pickup icons from Halo PC's own, four times the Xbox's resolution: they are read from the first Halo PC map in `multi_maps`, with its `bitmaps.map` (`port/linux/game/hud_pc_icons.c`). Without one, the Xbox's icons are drawn, upscaled.

The **DIGSITE** list (in place of CUSTOM SINGLEPLAYER) always shows the 16 Digsite maps: Abyss, B20, City, Destructo, Dusk, Indoor, M1 Assault, M1 Defense, M1 Exploration Moonbase, Ruined Pain, Space Station, Spasm, Swamp, Swamp Thing, Tutorial and Underground. Put their `.map` files (`abyss.map`, `b20.map`, ... `underground.map`) in a **`digsite_maps`** folder beside `maps`. They are Custom Edition maps, so they need Custom Edition's `bitmaps.map`, `sounds.map` and `loc.map` in `custom_maps` (or in `digsite_maps`): Halo PC's are not the same files. A map that is missing, or that this build cannot load, is listed as "Not installed". Each plays as its file says: a campaign scenario as a campaign level (alone, or as LAN co-op from Create Game), a multiplayer one as any multiplayer map. `<name>.bmp` and `<name>.txt` beside one give it a picture and a description.

All other Custom Edition multiplayer maps are in **CUSTOM EDITION**, in the map list in Campaign's New Game and on the Map screen of Create Game. On these maps every vehicle and weapon the map places always spawns, whatever the gametype: its vehicle sets and MAP WEAPONS setting, and the map's own per-gametype spawn flags, are not applied (a weapon set still swaps weapons, but never removes one).

Maps that require OpenSauce are not compatible.

The LAN browser supports automatic map downloads - when you join a game it will download the custom maps from other players, with any of `bitmaps.map`, `sounds.map` and `loc.map` you are missing, when the host has map sharing enabled (on by default).
Refer to [docs/custom_edition_caches.md](docs/custom_edition_caches.md).

## Mods

Mods go in a `mods` folder next to `maps`. Each mod is a folder of Halo PC tag files, as the Halo Editing Kit and Invader make them, with a `mod.toml` describing it. Every mod in the folder is on by default. Mods in the main menu turns them on and off and sets their order. Think of it like a resource pack list, the mod at the top plays over the ones below it. The game also refuses mods that say they cannot be played together. 

Refer to [docs/mods.md](docs/mods.md) to make your own.


## Graphics

The game retains the original look and feel as it did on the Xbox by default. The video settings menus have additional options that go beyond it.

**Display**

| Option | What it does | Default |
| --- | --- | --- |
| DISPLAY MODE, RESOLUTION, WINDOW SIZE | Fullscreen, borderless or windowed, at any resolution (F11 switches) | Borderless, at the display's resolution |
| RESOLUTION SCALING | Draw at the window's resolution, or at the Xbox's 640x480 and scale it up | Native resolution |
| V-SYNC, FRAME RATE LIMIT | Wait for the display, or run free up to a frame cap; works with G-Sync and FreeSync | V-sync on |
| SMOOTH MOTION | A frame for every refresh of the display (60, 120, 240 Hz and more), blended between the game's 30 ticks a second | On |
| INSTANT AIM | In first person, the view turns the frame the mouse moves, not up to two ticks later | On |
| FOV, VIEWMODEL FOV | Field of view of the view and of the weapon and hands, from 80 to 150 degrees | Stock |

**Image quality**

| Option | What it does | Default |
| --- | --- | --- |
| ANTI-ALIASING | FXAA, SMAA, MSAA or SSAA | Off, as on the Xbox |
| TEXTURE FILTERING | Anisotropic filtering, from 2 to 16 - provides sharper floors, walls and terrain at an angle | Off, as on the Xbox |
| SHADOW RESOLUTION | Object shadows at 128 (the Xbox's), 256, 512 or 1024 pixels | 128 |
| PER-PIXEL LIGHTING | Lights models for each pixel, without the facets of the Xbox's per-vertex lighting | Off |
| MODEL DETAIL | Keep more model detail at a distance at high resolutions, or always the most detailed | Stock |
| HDR, HDR DISPLAY | Keeps explosions, plasma and the sun's glare past white and tone maps them; on an HDR monitor, shows them brighter than white | Off |
| HIGH-RES HUD, HIGH-RES TEXT | The HUD from hand-drawn redraws at 8x the size, and text in the Overpass font, sharp at any resolution | On |

**Extras**

| Option | What it does | Default |
| --- | --- | --- |
| LEGS | See your own legs when you look down in first person | Off |
| ARM SHIELDS | The energy shield's flare on your first-person arms too | Off |
| SHIELD GLOW, GLOW INTENSITY | Energy shields light up what is around them as they flare | Off |
| SHIELD COLOR | Your own shield in a multiplayer armor color, as your machine draws it | Default |
| OVERSHIELD | The overshield's own color, rising up the body as it charges | Off |
| PASSENGER THIRD PERSON | Third-person view for Warthog and Scorpion passengers (Campaign-only) | Off |


## How to build the game

You do not need the Xbox SDK. The port supplies the SDK declarations that
the game uses. Refer to [port/include/xdk](port/include/xdk/README.md).

To build the game:

1. Install Python and [ninja](https://ninja-build.org/).
2. Install the tools for your platform. Refer to the README for the
   platform.
3. In the root folder of the repository, enter `python configure.py`.
4. Enter `ninja` with the target for the platform:

| Target | Result |
| --- | --- |
| `ninja windows` (on Windows, or on Linux after `configure.py --windows-cross`) | `build/windows/halo.exe` and `SDL3.dll` |
| `ninja linux` | `build/linux/halo`: a developer's compile check of the shared code on Linux, never released |

If you enter `ninja` without a target, ninja builds the game for the
computer that you use.

`tools/ci_build.py` makes the same builds as GitHub Actions. For example,
enter `python tools/ci_build.py windows release`.

### Build options

Give these options to `configure.py`:

| Option | Result |
| --- | --- |
| (none) | A debug build. A failed assertion stops the game. |
| `--release` | A release build. The game does not examine assertions, as in the retail game. |
| `--portable` | The build operates on all x86-64 processors. Use this option for builds that you give to other persons. |
| `--lto=thin`, `--lto=off` | Less link-time optimization. The link is faster. |
| `--pgo=off` | No profile-guided optimization. |
| `--pgo=train` | Records a new optimization profile. Refer to "Optimization profiles". |
| `--profile` | A profiling build. It records CPU times. Refer to "Profiling builds". |

Without `--portable`, the builds use all the instructions
of the processor that builds them (`-march=native`). Such a build does not
always start on a different computer.

## For developers

### Testing

The tests are listed in [AGENTS.md](AGENTS.md) ("Testing"). Most run
without the game data, in GitHub Actions too.

One check needs the retail maps, so it runs on your own computer (or on a
self-hosted runner), not in GitHub Actions: `tools/net_compare.py` plays
two builds together in a network game on one computer and checks that they
stay in step. Run it when a change can alter how a build calculates the
game, as a change to the simulation, the compiler or the maths does,
before it is merged. For example, to check a build with such a change
against a build without it:

```sh
python tools/net_compare.py --a build/linux/halo --b <other checkout>/build/linux/halo \
    --data <folder with maps/> --maps bloodgulch,beavercreek,hangemhigh --swap
```

Each game prints PASS or FAIL. A FAIL names the first player, tick and
value that differ. Refer to "Comparing two builds" in
[port/linux/NETCODE.md](port/linux/NETCODE.md).

### Optimization profiles

The builds use profiles of the game to optimize the code:

- `pgo/halo_linux.profdata` for the Linux developer build (and Windows, where its own has none of a unit).
- `pgo/halo_windows.profdata` for Windows.

The profiles need clang 22 or later. With an older clang, the builds do not
use the profiles.

To record a new profile:

1. Delete the profile.
2. Enter `python configure.py --pgo=train`.
3. Enter `ninja linux` or `ninja windows`.

The build then plays the main menu and the first minute of each campaign
level. This procedure continues for approximately 15 minutes. The game
data must be in `assets/`.

### Profiling builds

A profiling build records where the game spends its time.

1. Enter `python configure.py --profile`, then build as usual.
2. Start a recording. Enter `profile_record` in the console (or the telnet
   console), or set `debug.profile_record = true` in `config.toml`. To
   record each game of a session, also set
   `debug.profile_record_when = "game"`.
3. Stop the recording. Enter `profile_stop`, load another map, or quit.

The game writes numbered `.part<n>.json` files under `profiles/` in its data root
and logs their paths. Parts are retained after the recording stops. Logs go to
standard error; a Windows release build without standard error writes them to
`halo.log` next to `halo.exe`.

`tools/net_report.py` is a helper for developers. Enter
`python tools/net_report.py <recording.part1.json>` to read a recording
as tables. The command also writes `<recording>.summary.txt`, a short text
file.

| Command | Result |
| --- | --- |
| `profile_record [seconds]` | Starts a recording. With a number of seconds, the recording stops after that time. |
| `profile_stop` | Stops the recording. |

`debug.profile_memory` sets the memory of a recording in MB (default 256,
from 4 to 1024). A recording has no length limit:
the game writes a part each time the memory is full. Refer to "Settings" in
[port/linux/README.md](port/linux/README.md).

A profiling build plays with normal builds. It does not use
`--pgo=train`.


## Credits

The decompilation is of the Xbox build 2342 (`cachebeta.exe`, SHA-256
`4cc87b45f721270392a96f1674ed2b5cd4a7bb4355faeab4531d1cf1884d9520`).
OpenCE starts from the decompilation of
[bnunu/halo-1](https://github.com/bnunu/halo-1), which is a fork of
[punpckhdq/halo](https://github.com/punpckhdq/halo). The artists of the
high-res redraws are in [AUTHORS.md](AUTHORS.md). The main contributors are Jonas Volman (@bnunu) and MrBruh (@cybersecurity). Everyone else who has contributed is on the
[contributors page](https://github.com/OpenCommunityEdition/OpenCE/graphs/contributors).

## Licence

OpenCE is free software: you can give it to others and change it under the
terms of the GNU Affero General Public License as published by the Free
Software Foundation, version 3 of the License or (at your option) any later
version (`AGPL-3.0-or-later`). It comes with no warranty. The full text is in
[LICENSE](LICENSE).

- **From the change.** The AGPL applies from the commit that made the change,
  and to each release built from that commit or a later one. Earlier
  releases, and the source at every earlier commit, were released under
  [CC0 1.0](LICENSES/CC0-1.0.txt) and stay that way.
- **What OpenCE builds on.** The decompilation of
  [punpckhdq/halo](https://github.com/punpckhdq/halo) is under the AGPL 3.0
  from 17 October 2026, and under CC0 1.0 before that date.
  [bnunu/halo-1](https://github.com/bnunu/halo-1) is under CC0 1.0.
- **Contributions** are under `AGPL-3.0-or-later`. A change taken from a
  project under GPL 3.0 keeps that licence and its notices; the GPL 3.0 and
  the AGPL 3.0 let the two be combined (section 13 of each).
- **Hosts and servers.** If you change the game and let other persons play
  with your version over a network (for example, as the host of an internet
  game, or on a web site), the AGPL asks you to offer them the source of
  your version (section 13). The game as released has its source here, and
  each release's source is its tag (`build-<n>`).
- **Third-party code, fonts and art** keep their own licences, which are
  next to them (`port/third_party`, `port/assets/fonts`,
  `port/assets/icons`, `port/assets/text_filter`). The release downloads
  carry their notices.
- **Halo is not covered.** The licence covers the work of OpenCE's authors.
  It gives no rights in Halo, its maps or anything else that Microsoft owns.


## Disclaimer
OpenCE (Open Community Edition) is an unofficial fan project. It is not endorsed by or affiliated with Microsoft, Xbox Game Studios, Halo Studios, Activision or Bungie. Halo and Halo: Combat Evolved are trademarks of Microsoft Corporation; Halo © Microsoft Corporation.
No game data is included. You must provide your own copy of the game.
