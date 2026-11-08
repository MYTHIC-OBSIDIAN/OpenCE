/* The Xbox's loading screen for a map already in the cache files
(loading_screen.c; source/game/game.c's game_precache_new_map). */
#ifndef LOADING_SCREEN_H
#define LOADING_SCREEN_H

/* whether the loading screen shows for `map_name`, a level the cache files
already hold, and its progress, 0 to 1, by the time since `started` */
boolean loading_screen_for_cached_map(char const *map_name);
real loading_screen_progress(unsigned long started);

#endif
