# OpenCE Achievements

The companion program of the campaign's achievements
(`port/linux/game/achievements.c`): a Windows tray program that shows an
"Achievement unlocked" pop-up, with a sound, as the game earns each one, and
the list of all 37 (780 points) when it is opened.

The game writes each achievement once to `achievements.log` in its save
root's `u` folder (`savegame\u\` next to `halo.exe`, `paths.saves`, or
`%APPDATA%\halo\u\`), 16 bytes a record: the achievement's number (`Data.cs`),
0, and a Windows FILETIME. The program reads every one of those places. The
list's header is titled with the name of the profile player 1 plays on, which
the game writes to `achievements_profile.txt` (UTF-8) in the same `u` folder
as it changes ("Achievements" until then). A `Gamerpic.png` next to the
program is the picture in the list's header.

The list works with the mouse, the keyboard and an Xbox controller: up and
down (arrow keys, D-pad, left stick) move the selection, Page Up / Page Down
and the triggers a page, left and right (arrow keys, D-pad) or Y the order
(ALL, BY DATE, BY GAMERSCORE), Tab and the bumpers the filter (SHOW ALL, UNLOCKED, LOCKED),
and Escape, B or Back close it. `scroll.wav` plays as the selection moves
and `filter.wav` as the order or the filter changes.

With `game.achievements_app` on (the default), the game starts
`Achievements.exe --background` when it is next to `halo.exe`; it closes
itself after the game. Started without `--background` (or from its tray
icon) it shows the list.

## Building

`Build.bat` builds it with the C# compiler that ships with Windows (.NET
Framework 4) and puts `Achievements.exe` next to `halo.exe` when that is this
folder or up to three folders above it.

The pictures and the sound are not in the repository. Put them in an
`assets` folder here before building; every one is optional, and a missing
one is drawn plainly:

| File | What |
| --- | --- |
| `assets\icons\<number>.png` | Each achievement's picture, by its number in `Data.cs` (1 to 37), with a transparent background (`tools\achievements\clear_icons.py` takes the 360 icons' black off) |
| `assets\toast.png`, `assets\toast_globe.png` | The pop-up's bar |
| `assets\gscore.png` | The points symbol |
| `assets\min_*.png`, `assets\close_*.png` | The list window's buttons (`normal`, `focus`, `press`) |
| `assets\achievement.mp3` | The pop-up's sound |
| `assets\header.png` | The header's background: as wide as the header, a band near its foot shown |
| `assets\tile.png` | The square picture in the header, when there is no `Gamerpic.png` |
| `assets\scroll.wav`, `assets\filter.wav` | The list's sounds (WAV); files of those names next to the program win |
| `assets\cursor.cur` | The list window's cursor (else `cursor.cur` here: the game's menu cursor) |
| `assets\app.ico` | The program's icon (else `port\windows\opence-icon.ico`) |

Pictures named after an achievement in an `Achievement Icons` folder next to
the program or the game are used in place of the built-in ones.
