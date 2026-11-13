/*
MCC_MAPS.C

The independent MCC menu catalog. Only version 13 Halo 1 MCC caches from
d:\custom_maps and d:\digsite_maps, listed with the Custom Edition maps
(MCC_MAPS_CUSTOM_PACKAGE, MCC_MAPS_DIGSITE_PACKAGE), are admitted: port: not
d:\mcc_maps' nor the Steam Workshop's. The on-disk filename is the level identity; the
cache's internal scenario name may differ. No Xbox or Custom Edition map
list, loader, resource directory or display index is used here.
*/

#include "cseries.h"
#include "errors.h"
#include "mcc_cache_format.h"
#include "mcc_maps.h"
#include "mcc_menu_preview.h"
#include "mcc_cache.h"
#include "mcc_identity.h"
#include "mcc_package.h"
#include "mcc_workshop.h"
#include "../src/steam_workshop.h"
#include "../src/port_config.h"
#include "../src/mcc_manifest.h"
#include "../src/mcc_menu_metadata.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

/* main_set_map_name retains 63 characters including the namespace. */
#define MCC_MAP_FILENAME_MAXIMUM (63 - (sizeof(MCC_MAPS_LEVEL_PREFIX) - 1))
#define MCC_MAP_DESCRIPTION_MAXIMUM 127

struct mcc_menu_map
{
	char file_name[MCC_MAP_FILENAME_MAXIMUM + 1];
	char level_name[64];
	char item_id[MCC_WORKSHOP_ID_SIZE];
	wchar_t name[64];
	wchar_t description[MCC_MAP_DESCRIPTION_MAXIMUM + 1];
	boolean campaign, titled;
	/* custom_maps' or digsite_maps' (MCC_MAPS_CUSTOM_PACKAGE,
	MCC_MAPS_DIGSITE_PACKAGE): listed with the Custom Edition maps */
	boolean custom, digsite;
	short mission_order, next_state;
	char next_mission[64];
	char campaign_title[64];
	char package_title[64];
};

static struct
{
	boolean scanned;
	boolean workshop_enabled, campaigns_enabled;
	short count;
	struct mcc_menu_map maps[MCC_MAPS_MAXIMUM];
} mcc_menu_catalog;

typedef char mcc_display_indices_are_separate[
	MCC_MAPS_FIRST_DISPLAY_INDEX + MCC_MAPS_MAXIMUM <= 0x3000 ? 1 : -1];

static void mcc_map_description_read(struct mcc_menu_map *map)
{
	char path[128];
	unsigned char bytes[1024];
	FILE *file;
	size_t length = 0, position;
	short used = 0;

	snprintf(path, sizeof(path), "%s%s.txt", MCC_MAPS_DIRECTORY, map->file_name);
	file = fopen(path, "rb");
	if (file)
	{
		length = fread(bytes, 1, sizeof(bytes), file);
		fclose(file);
	}
	position = length >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF ? 3 : 0;
	for (; position < length && used < MCC_MAP_DESCRIPTION_MAXIMUM; position++)
	{
		unsigned char value = bytes[position];

		if (value == '\n')
		{
			if (used + 2 > MCC_MAP_DESCRIPTION_MAXIMUM)
				break;
			map->description[used++] = '\r';
			map->description[used++] = '\n';
		}
		else if (value == '\t')
			map->description[used++] = ' ';
		else if (value >= 32 && value < 127)
			map->description[used++] = value;
		else if (value >= 0xC0)
			map->description[used++] = '?';
	}
	while (used && (map->description[used - 1] == ' ' || map->description[used - 1] == '\r' ||
		map->description[used - 1] == '\n'))
		used--;
	map->description[used] = 0;
	if (!used)
	{
		wchar_t const *fallback = map->campaign ? L"Halo 1 MCC\r\nCampaign map" : L"Halo 1 MCC\r\nMultiplayer map";

		for (used = 0; fallback[used]; used++)
			map->description[used] = fallback[used];
		map->description[used] = 0;
	}
}

/* whether a map's name (<package>\\<map>) is of the folder `package`'s
(custom_maps or digsite_maps): its package that, or that+<folder> */
static boolean mcc_map_name_of(char const *name, char const *package)
{
	size_t length = strlen(package);

	return !_strnicmp(name, package, length) && (name[length] == '\\' || name[length] == '+');
}

static void mcc_map_register(char const *name)
{
	char path[128], candidate[128], canonical[64];
	char const *display;
	unsigned char header[MCC_CACHE_HEADER_BYTES];
	struct mcc_cache_identity identity;
	struct mcc_menu_map *map;
	struct mcc_workshop_item item;
	FILE *file;
	long file_size;
	size_t name_length = strlen(name), index;
	boolean start_word = TRUE;

	if (!name_length || name_length > MCC_MAP_FILENAME_MAXIMUM || mcc_menu_catalog.count == MCC_MAPS_MAXIMUM)
        return;
    snprintf(candidate, sizeof(candidate), "%s%s", MCC_MAPS_LEVEL_PREFIX, name);
    if (!mcc_package_name(candidate, canonical)) return;
    name = canonical + sizeof(MCC_MAPS_LEVEL_PREFIX) - 1;
	for (index = 0; index < (size_t)mcc_menu_catalog.count; index++)
	{
		if (!csstrcasecmp(mcc_menu_catalog.maps[index].file_name, name))
			return;
	}
	if (!mcc_package_map_path(canonical,path)) return;
	file = fopen(path, "rb");
	if (!file)
		return;
	if (fseek(file, 0, SEEK_END) || (file_size = ftell(file)) < 0 || fseek(file, 0, SEEK_SET) ||
		fread(header, 1, sizeof(header), file) != sizeof(header))
	{
		fclose(file);
		return;
	}
	fclose(file);
	if (mcc_cache_identify(header, sizeof(header), (uint64_t)file_size, &identity) != MCC_CACHE_OK)
		return;
	map = &mcc_menu_catalog.maps[mcc_menu_catalog.count++];
	memset(map, 0, sizeof(*map));
	memcpy(map->file_name, name, name_length + 1);
	snprintf(map->level_name, sizeof(map->level_name), "%s%s", MCC_MAPS_LEVEL_PREFIX, name);
	if (mcc_workshop_local_item(map->level_name, &item))
		strcpy(map->item_id, item.id);
	map->campaign = identity.scenario_type == 0;
	map->digsite = mcc_map_name_of(name, MCC_MAPS_DIGSITE_PACKAGE);
	map->custom = map->digsite || mcc_map_name_of(name, MCC_MAPS_CUSTOM_PACKAGE);
	display = strrchr(name, '\\');
	display = display ? display + 1 : name;
	for (index = 0; display[index]; index++)
	{
		unsigned char character = (unsigned char)display[index];

		if (character == '_')
			character = ' ';
		else if (start_word && character >= 'a' && character <= 'z')
			character -= 'a' - 'A';
		map->name[index] = character >= 32 && character < 127 ? character : '?';
		start_word = character == ' ';
	}
	mcc_map_description_read(map);
}

static int mcc_map_order(void const *left, void const *right)
{
	struct mcc_menu_map const *a = left, *b = right;
	char package_a[64], package_b[64];
	if ((a->mission_order || b->mission_order) &&
		mcc_workshop_package_folder(a->level_name, package_a) &&
		mcc_workshop_package_folder(b->level_name, package_b) && !csstrcasecmp(package_a, package_b))
	{
		int order_a = a->mission_order ? a->mission_order : MCC_MANIFEST_MAXIMUM_MISSIONS + 1;
		int order_b = b->mission_order ? b->mission_order : MCC_MANIFEST_MAXIMUM_MISSIONS + 1;
		if (order_a != order_b) return order_a - order_b;
	}
	return csstrcasecmp(((struct mcc_menu_map const *)left)->file_name,
		((struct mcc_menu_map const *)right)->file_name);
}

static void mcc_maps_campaign_metadata(void)
{
	static struct mcc_manifest manifest;
	char previous[64] = "", package[64], reason[128];
	int result = 0;
	short index;
	if (!config_boolean("game.mcc_campaigns")) return;
	/* The first sort groups package paths. Read each manifest once, keeping
     * its missing binary entries so progression cannot jump a missing level. */
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		struct mcc_menu_map *map = &mcc_menu_catalog.maps[index];
		int mission;
		if (!map->campaign || !mcc_workshop_package_folder(map->level_name, package)) continue;
		if (csstrcasecmp(previous, package))
		{
			strcpy(previous, package);
			result = mcc_manifest_for_level(map->level_name, &manifest, reason);
			if (result < 0) error(_error_silent, "mcc campaign %s: %s", package, reason);
		}
		if (result != 1) continue;
		for (mission = 0; mission < manifest.count; mission++)
		{
			char const *title;
			size_t n;
			if (csstrcasecmp(strrchr(map->level_name, '\\') + 1, manifest.missions[mission].name)) continue;
			map->mission_order = (short)(mission + 1);
			strcpy(map->campaign_title, manifest.title);
			title = manifest.missions[mission].title;
			if (title[0])
			{
				for (n = 0; title[n] && n < 63; n++) map->name[n] = (unsigned char)title[n] >= 32 && (unsigned char)title[n] < 127 ? title[n] : '?';
				map->name[n] = 0;
				map->titled = TRUE;
			}
			map->next_state = 2;
			if (mission + 1 < manifest.count)
			{
				int written = snprintf(map->next_mission, sizeof(map->next_mission), "%s%s\\%s",
					MCC_MAPS_LEVEL_PREFIX, package, manifest.missions[mission + 1].name);
				map->next_state = written > 0 && written < (int)sizeof(map->next_mission) ? 1 : -1;
			}
			break;
		}
	}
	qsort(mcc_menu_catalog.maps, (size_t)mcc_menu_catalog.count, sizeof(mcc_menu_catalog.maps[0]), mcc_map_order);
}

static void mcc_maps_menu_metadata(void)
{
	static struct mcc_menu_metadata metadata[2];
	char previous[64] = "", package[64], root[1024];
	boolean loaded[2] = { FALSE, FALSE };
	short index;
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		struct mcc_menu_map *map = &mcc_menu_catalog.maps[index];
		struct steam_workshop_map_text const *text;
		int family = map->campaign ? 1 : 0;
		size_t n;
		if (map->campaign && !config_boolean("game.mcc_campaigns")) continue;
		if (!mcc_workshop_package_folder(map->level_name, package)) continue;
		if (csstrcasecmp(previous, package))
		{
			strcpy(previous, package);
			loaded[0] = loaded[1] = FALSE;
			memset(metadata, 0, sizeof(metadata));
		}
		if (!loaded[family])
		{
			loaded[family] = TRUE;
			if (mcc_manifest_package_root(map->level_name, root))
				mcc_menu_metadata_load(root, family, &metadata[family]);
		}
		strcpy(map->package_title, metadata[family].title);
		text = mcc_menu_metadata_find(&metadata[family], strrchr(map->level_name, '\\') + 1);
		if (!text || !text->title[0]) continue;
		for (n = 0; text->title[n] && n < 63; n++)
			map->name[n] = (unsigned char)text->title[n] >= 32 && (unsigned char)text->title[n] < 127 ? text->title[n] : '?';
		map->name[n] = 0;
		map->titled = TRUE;
	}
}

/* The MCC maps of the custom_maps (or digsite_maps) folder `root`: those
loose in it (the package `package`), and those of each folder in it
(<package>+<folder>). The Custom Edition maps beside them are not MCC's,
and are not admitted (mcc_map_register identifies each file). */
static void mcc_maps_scan_custom(char const *root, char const *package, char const *folder)
{
    WIN32_FIND_DATAA data;
    HANDLE find;
    char pattern[128];
    snprintf(pattern,sizeof(pattern),"%s%s%s*.*",root,folder,folder[0] ? "\\" : "");
    find=FindFirstFileA(pattern,&data);
    if (find==INVALID_HANDLE_VALUE) return;
    do {
        char name[sizeof(data.cFileName)+1], relative[128];
        char *extension;
        size_t length;
        memcpy(name,data.cFileName,sizeof(data.cFileName));
        name[sizeof(data.cFileName)]=0;
        length=strlen(name);
        if (!length || strpbrk(name,"/\\+") || length>40 || name[0]=='.') continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            /* (one folder deep, as mcc_maps' packages) */
            if (!folder[0]) mcc_maps_scan_custom(root,package,name);
            continue;
        }
        extension=strrchr(name,'.');
        if (!extension || csstrcasecmp(extension+1,"map")) continue;
        *extension=0;
        if (!csstrcasecmp(name,"sounds") || !csstrcasecmp(name,"bitmaps") || !csstrcasecmp(name,"loc")) continue;
        snprintf(relative,sizeof(relative),"%s%s%s\\%s",package,folder[0] ? "+" : "",folder,name);
        mcc_map_register(relative);
    } while (FindNextFileA(find,&data));
    CloseHandle(find);
}

static short mcc_maps_custom_count_scanned(boolean digsite)
{
    short index, count = 0;
    for (index = 0; index < mcc_menu_catalog.count; index++)
        count += mcc_menu_catalog.maps[index].custom && mcc_menu_catalog.maps[index].digsite == digsite ? 1 : 0;
    return count;
}

static void mcc_maps_scan(void)
{
    short i;
    boolean workshop = config_boolean("game.workshop_maps");
    boolean campaigns = config_boolean("game.mcc_campaigns");
    if (mcc_menu_catalog.scanned && mcc_menu_catalog.workshop_enabled == workshop &&
        mcc_menu_catalog.campaigns_enabled == campaigns) return;
    if (mcc_menu_catalog.scanned) mcc_maps_rescan();
    mcc_menu_catalog.workshop_enabled = workshop;
    mcc_menu_catalog.campaigns_enabled = campaigns;
    mcc_menu_catalog.scanned=TRUE;
    mcc_menu_catalog.count=0;
    /* port: custom_maps' and digsite_maps' alone; not the mcc_maps folder's
     * nor the Steam Workshop's */
    mcc_maps_scan_custom(MCC_MAPS_CUSTOM_FOLDER,MCC_MAPS_CUSTOM_PACKAGE,"");
    mcc_maps_scan_custom(MCC_MAPS_DIGSITE_FOLDER,MCC_MAPS_DIGSITE_PACKAGE,"");
    qsort(mcc_menu_catalog.maps,(size_t)mcc_menu_catalog.count,sizeof(mcc_menu_catalog.maps[0]),mcc_map_order);
    /* Read supplied titles independently of Steam (also for Android imports).
     * Identities stay in level_name, never in the level's display label. */
    for (i=0;i<mcc_menu_catalog.count;i++) {
        struct mcc_menu_map *map=&mcc_menu_catalog.maps[i];
        char const *base=strrchr(map->file_name,'\\');
        /* (custom_maps' and digsite_maps' loose maps by their own names; a
         * folder's, with the folder's name after, as mcc_maps' packages) */
        if (map->custom && !strchr(map->file_name,'+')) continue;
        if (base) {
            char package[64];
            size_t n,k,length=(size_t)(base-map->file_name);
            memcpy(package,map->file_name,length);package[length]=0;
            if (map->custom) {
                size_t skip=map->digsite ? sizeof(MCC_MAPS_DIGSITE_PACKAGE) : sizeof(MCC_MAPS_CUSTOM_PACKAGE);
                memmove(package,package+skip,strlen(package+skip)+1);
                length=strlen(package);
            }
            /* Preserve the established human-named package disambiguator. */
            for (n=0;map->name[n];n++) {}
            map->name[n++]=' ';map->name[n++]='[';
            for (k=0;k<length;k++) map->name[n++]=(unsigned char)package[k];
            map->name[n++]=']';map->name[n]=0;
        }
    }
    mcc_maps_campaign_metadata();
    mcc_maps_menu_metadata();
    error(_error_silent,"mcc maps: %d maps found in mcc_maps and its packages, %d of them in custom_maps, %d in digsite_maps",
        mcc_menu_catalog.count,mcc_maps_custom_count_scanned(FALSE),mcc_maps_custom_count_scanned(TRUE));
}

int mcc_maps_next_mission(char const *level_name, char next[64])
{
	short index, target;
	struct mcc_menu_map const *map;
	next[0] = 0;
	if (!config_boolean("game.mcc_campaigns")) return 0;
	index = mcc_maps_find(level_name);
	if (index == NONE) return 0;
	map = &mcc_menu_catalog.maps[index];
	if (!map->campaign || !map->mission_order) return 0;
	if (map->next_state != 1) return map->next_state;
	strcpy(next, map->next_mission);
	target = mcc_maps_find(next);
	return target != NONE && mcc_menu_catalog.maps[target].campaign ? 1 : -1;
}

void mcc_maps_rescan(void)
{
	mcc_menu_preview_clear();
	mcc_cache_precache_clear();
	steam_workshop_scan();
	mcc_menu_catalog.scanned = FALSE;
}

short mcc_maps_count(boolean multiplayer_only)
{
	short index, count = 0;

	mcc_maps_scan();
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		if (!multiplayer_only || !mcc_menu_catalog.maps[index].campaign)
			count++;
	}
	return count;
}

short mcc_maps_index(short row, boolean multiplayer_only)
{
	short index;

	mcc_maps_scan();
	if (row < 0)
		return NONE;
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		if ((!multiplayer_only || !mcc_menu_catalog.maps[index].campaign) && row-- == 0)
			return index;
	}
	return NONE;
}

short mcc_maps_type_count(boolean campaign)
{
	short index, count = 0;

	mcc_maps_scan();
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		if (mcc_menu_catalog.maps[index].campaign == campaign && !mcc_menu_catalog.maps[index].custom)
			count++;
	}
	return count;
}

short mcc_maps_type_index(short row, boolean campaign)
{
	short index;

	mcc_maps_scan();
	if (row < 0)
		return NONE;
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		if (mcc_menu_catalog.maps[index].campaign == campaign && !mcc_menu_catalog.maps[index].custom && row-- == 0)
			return index;
	}
	return NONE;
}

/* the MCC maps of a kind of the custom_maps folder (digsite FALSE) or of
the digsite_maps folder */
static short mcc_maps_folder_count(boolean campaign, boolean digsite)
{
	short index, count = 0;

	mcc_maps_scan();
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		struct mcc_menu_map const *map = &mcc_menu_catalog.maps[index];

		if (map->campaign == campaign && map->custom && map->digsite == digsite)
			count++;
	}
	return count;
}

static short mcc_maps_folder_index(short row, boolean campaign, boolean digsite)
{
	short index;

	mcc_maps_scan();
	if (row < 0)
		return NONE;
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		struct mcc_menu_map const *map = &mcc_menu_catalog.maps[index];

		if (map->campaign == campaign && map->custom && map->digsite == digsite && row-- == 0)
			return index;
	}
	return NONE;
}

short mcc_maps_custom_count(boolean campaign)
{
	return mcc_maps_folder_count(campaign, FALSE);
}

short mcc_maps_custom_index(short row, boolean campaign)
{
	return mcc_maps_folder_index(row, campaign, FALSE);
}

short mcc_maps_digsite_count(boolean campaign)
{
	return mcc_maps_folder_count(campaign, TRUE);
}

short mcc_maps_digsite_index(short row, boolean campaign)
{
	return mcc_maps_folder_index(row, campaign, TRUE);
}

/* the menus' MCC maps of a kind listed with the Custom Edition maps: those
of the custom_maps folder, (campaign maps) the digsite_maps folder's (its
multiplayer maps are DIGSITE's), then the mcc_maps folder's and the Steam
Workshop's */
short mcc_maps_listed_count(boolean campaign)
{
	return (short)(mcc_maps_custom_count(campaign) + (campaign ? mcc_maps_digsite_count(TRUE) : 0) +
		mcc_maps_type_count(campaign));
}

short mcc_maps_listed_index(short row, boolean campaign)
{
	short custom = mcc_maps_custom_count(campaign);
	short digsite = campaign ? mcc_maps_digsite_count(TRUE) : 0;

	if (row < 0)
		return NONE;
	if (row < custom)
		return mcc_maps_custom_index(row, campaign);
	if (row < custom + digsite)
		return mcc_maps_digsite_index((short)(row - custom), TRUE);
	return mcc_maps_type_index((short)(row - custom - digsite), campaign);
}

short mcc_maps_find(char const *level_name)
{
	short index;
    char canonical[64];

	if (!mcc_package_name(mcc_identity_local_name(level_name), canonical))
		return NONE;
	mcc_maps_scan();
	for (index = 0; index < mcc_menu_catalog.count; index++)
	{
		if (!csstrcasecmp(canonical, mcc_menu_catalog.maps[index].level_name))
			return index;
	}
	return NONE;
}

char const *mcc_maps_level_name(short index)
{
	mcc_maps_scan();
	return index >= 0 && index < mcc_menu_catalog.count ? mcc_menu_catalog.maps[index].level_name : NULL;
}

wchar_t const *mcc_maps_name(short index)
{
	mcc_maps_scan();
	return index >= 0 && index < mcc_menu_catalog.count ? mcc_menu_catalog.maps[index].name : L"";
}

wchar_t const *mcc_maps_description(short index)
{
	mcc_maps_scan();
	return index >= 0 && index < mcc_menu_catalog.count ? mcc_menu_catalog.maps[index].description : L"";
}

boolean mcc_maps_campaign(short index)
{
	mcc_maps_scan();
	return index >= 0 && index < mcc_menu_catalog.count && mcc_menu_catalog.maps[index].campaign;
}

boolean mcc_maps_level_campaign(char const *level_name)
{
	return mcc_maps_campaign(mcc_maps_find(level_name));
}

boolean mcc_maps_workshop_title(char const *item_id, char *title, unsigned size)
{
	return steam_workshop_item_title(item_id, title, size) && title[0];
}
