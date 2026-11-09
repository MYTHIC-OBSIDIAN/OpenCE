/*
BUILD_1749.C

halo1749.exe: this program run as Halo's build 01.08.15.1749 (15 August
2001, an Xbox build before the retail game), on 1749's own maps, as stock.

The release ships the same program twice: halo.exe, and halo1749.exe, which
runs in this mode (so does any executable whose name begins halo1749, or
HALO_BUILD=1749 in the environment). The mode decides only what is read and
where; the game's code is the same.

- Its files are the 1749 folder beside the executable: 1749\maps (1749's
  ui.map and levels), its own config.toml and its own savegame folder
  (port_config.c, xbox_files.c). paths.data and paths.saves in that
  config.toml move them.
- 1749's maps were linked to a tag cache at 0x803AF000, 36 KB above this
  build's (2342's) 0x803A6000; its tag cache is put there, and its maps load
  as they are (physical_memory_map.c). Their tags are laid out as this
  build's: every group in them passes the tag validator.
- 1749 numbers its script functions differently; each call is given this
  build's number by the name the scripts keep (custom_edition_scripts.c,
  build_1749_scripts_convert).
- Stock: the Xbox's menus out of 1749's ui.map, and every OpenCE addition
  that changes what is seen, heard or played held off (port_config.c,
  build_1749_* tables), as the LAN-only settings are.

Read in docs/build_1749.md.
*/

#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---------- constants */

/* the tag cache's address: this build's maps', and 1749's */
#define TAG_CACHE_BASE_ADDRESS 0x803A6000UL
#define TAG_CACHE_BASE_ADDRESS_1749 0x803AF000UL

#define EXECUTABLE_NAME_1749 "halo1749"
#define FOLDER_1749 "1749"

/* ---------- private code */

static int name_begins(const char *name, const char *prefix)
{
	for (; *prefix; name++, prefix++)
	{
		char character = *name;

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		if (character != *prefix)
			return 0;
	}
	return 1;
}

/* the executable's path, with forward slashes; 0 if unknown */
static int executable_path(char *path, size_t size)
{
	ssize_t length = readlink("/proc/self/exe", path, size - 1);
	char *character;

	if (length <= 0)
		return 0;
	path[length] = '\0';
	for (character = path; *character; character++)
	{
		if (*character == '\\')
			*character = '/';
	}
	return 1;
}

/* ---------- public code */

int halo_build_1749(void)
{
	static int build_1749 = -1;

	if (build_1749 < 0)
	{
		const char *environment = getenv("HALO_BUILD");
		char path[1024];

		build_1749 = 0;
		if (environment && *environment)
		{
			build_1749 = !strcmp(environment, "1749");
		}
		else if (executable_path(path, sizeof(path)))
		{
			const char *name = strrchr(path, '/') ? strrchr(path, '/') + 1 : path;

			build_1749 = name_begins(name, EXECUTABLE_NAME_1749);
		}
	}
	return build_1749;
}

const char *halo_build_1749_root(void)
{
	static char root[1024];

	if (!root[0])
	{
		char path[1024];
		char *slash;

		if (executable_path(path, sizeof(path)) && (slash = strrchr(path, '/')) != NULL)
		{
			*slash = '\0';
			snprintf(root, sizeof(root), "%s/" FOLDER_1749, path);
		}
		else
		{
			snprintf(root, sizeof(root), FOLDER_1749);
		}
	}
	return root;
}

unsigned long halo_tag_cache_base_address(void)
{
	return halo_build_1749() ? TAG_CACHE_BASE_ADDRESS_1749 : TAG_CACHE_BASE_ADDRESS;
}
