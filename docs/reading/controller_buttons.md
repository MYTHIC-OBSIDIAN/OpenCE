# Button styles

Settings > Preferences has BUTTON STYLE: the Xbox's Classic, Gems, Dark Gems
and Modern. It is one style for the whole computer (`display.hud_button_style`
in `config.toml`), whichever player or controller a prompt is for. There are
no PlayStation or Switch styles; a value of one that an earlier build saved
draws Classic.

BUMPER STYLE appears for Xbox Classic, Gems and Dark Gems. Original Xbox, the
default, keeps black/white buttons; Modern Xbox uses the existing LB/RB artwork.
It too is one for the computer and changes no bindings. Xbox Modern keeps its
own bumpers. The settings remain available with High-Res HUD off; the artwork
changes apply when High-Res HUD is enabled in Graphics.

## Reading route

1. [controller_buttons.c](../../port/linux/src/controller_buttons.c) reads the
   style, and classifies SDL devices for Button Remapping's names.
   [xinput_sdl.c](../../port/linux/src/xinput_sdl.c) supplies device identity.
2. [controller_button_menu.inl](../../port/linux/game/controller_button_menu.inl)
   shows BUMPER STYLE as the pending BUTTON STYLE allows.
3. [controller_glyphs.c](../../port/linux/game/controller_glyphs.c) draws Xbox
   Modern's artwork, and the modern bumpers for the older styles, within the
   existing prompt cells, preserving advances, centred anchoring and
   split-screen scale.
4. [hud_hires_tags.c](../../port/linux/game/hud_hires_tags.c) identifies native and
   stock CE button entries before calling that renderer. The Classic, Gems and
   Dark Gems styles and the low-resolution fallbacks keep their paths. The
   cache is keyed by artwork and disposed at normal map teardown.
5. [controller_button_assets.py](../../tools/controller_button_assets.py) generates
   Xbox Modern's artwork from the retained sources. See its
   [provenance](../../port/assets/controller_buttons/README.md). Classic Xbox
   letters use transparent cutouts in [button_assets.py](../../tools/button_assets.py).

The settings table is in [the port README](../../port/linux/README.md).
[Button remapping](controller_remapping.md) is a separate Gamepad Setup submenu.
The checks in [test_controller_buttons.py](../../tools/test_controller_buttons.py)
and [test_hud_button_style.py](../../tools/test_hud_button_style.py) cover the
style's reading, the pending menu, glyph geometry, animated controls, opacity,
bumper choices and cache cleanup.
