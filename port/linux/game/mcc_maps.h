/* Halo 1 MCC map discovery. This list never includes the Xbox or CE folders. */
#ifndef __MCC_MAPS_H
#define __MCC_MAPS_H

#define MCC_MAPS_DIRECTORY "d:\\mcc_maps\\"
#define MCC_MAPS_LEVEL_PREFIX "mcc_maps\\"
#define MCC_MAPS_FIRST_DISPLAY_INDEX 0x1000
#define MCC_MAPS_MAXIMUM 1024
/* MCC maps are listed with the Custom Edition maps (CUSTOM EDITION and
CUSTOM CAMPAIGNS: mcc_maps_listed_index). One put in the custom_maps folder
loose is the level mcc_maps\custom_maps\<name>, one in a package folder of
custom_maps mcc_maps\custom_maps+<folder>\<name>. Those packages are read
from custom_maps (xbox_files.c's translate_workshop). */
#define MCC_MAPS_CUSTOM_PACKAGE "custom_maps"
#define MCC_MAPS_CUSTOM_FOLDER "d:\\custom_maps\\"

void mcc_maps_rescan(void);
short mcc_maps_count(boolean multiplayer_only);
short mcc_maps_index(short row, boolean multiplayer_only);
/* Exact scenario-type lists; display indices still use the full catalog. */
short mcc_maps_type_count(boolean campaign);
short mcc_maps_type_index(short row, boolean campaign);
/* those of the custom_maps folder, listed with the Custom Edition maps */
short mcc_maps_custom_count(boolean campaign);
short mcc_maps_custom_index(short row, boolean campaign);
/* the menus' of a kind: custom_maps' first, then the mcc_maps folder's and
the Steam Workshop's */
short mcc_maps_listed_count(boolean campaign);
short mcc_maps_listed_index(short row, boolean campaign);
short mcc_maps_find(char const *level_name);
char const *mcc_maps_level_name(short index);
wchar_t const *mcc_maps_name(short index);
wchar_t const *mcc_maps_description(short index);
boolean mcc_maps_campaign(short index);
boolean mcc_maps_level_campaign(char const *level_name);
/* 0 standalone/disabled, 1 next mission, 2 completed, -1 unavailable next
 * mission. Only the explicit manifest supplies an order. */
int mcc_maps_next_mission(char const *level_name, char next[64]);
/* The title of the Steam Workshop item with this id when it is subscribed here and has one. */
boolean mcc_maps_workshop_title(char const *item_id, char *title, unsigned size);

#endif
