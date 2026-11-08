/*
LOADING_SCREEN.C

The Xbox shows its loading screen (progress_bar.c: the glowing ring, the
picture loading.tga, the chant) while a map is copied from the disc into
the cache files, and a level not yet copied always was. Here the cache files
are kept on the disk from run to run, so a level loaded before is never
copied again and the screen never showed: the last menu stayed on the screen
while the level loaded. With display.loading_screen (on by default), loading
a level the cache files already hold shows it as the copy would, its ring
filling for LOADING_SCREEN_MILLISECONDS; its last frame stays on the screen
while the level itself loads, as the Xbox's did.

Not for the main menu's map (levels\ui\ui), a dedicated server, nor with the
null renderer.
*/

#include "cseries.h"
#include "loading_screen.h"

int config_boolean(const char *name); /* port_config.c */

#define LOADING_SCREEN_MILLISECONDS 2000
#define MAIN_MENU_MAP_NAME "levels\\ui\\ui"

boolean loading_screen_for_cached_map(char const *map_name)
{
	return map_name && csstrcasecmp(map_name, MAIN_MENU_MAP_NAME) != 0 &&
		config_boolean("display.loading_screen") && !config_boolean("debug.null_renderer") &&
		!halo_server_mode();
}

real loading_screen_progress(unsigned long started)
{
	unsigned long elapsed = system_milliseconds() - started;

	return elapsed >= LOADING_SCREEN_MILLISECONDS ? 1.0f : (real)elapsed / (real)LOADING_SCREEN_MILLISECONDS;
}
