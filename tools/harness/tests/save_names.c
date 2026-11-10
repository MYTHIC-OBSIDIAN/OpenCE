/*
SAVE_NAMES.C (test)

The Xbox's save game containers (port/linux/src/xbox_xapi.c) over a small
file system in memory. This build names a save's directory after its name's
hash and writes the name alone, in UTF-16, to SaveMeta.xbx. Build 1749's own
profile in its dump (u:\UDATA\122A17771B9E\) was named by the Xbox, and its
SaveMeta.xbx is a "Name=New001" line: the name is read from either, and a
save is found by its name whatever its directory, so renaming 1749's profile
deletes its old directory instead of leaving the profile listed twice.
test_save_names.py takes the code (under_test.inc).
*/

#include "harness.h"

typedef unsigned short WCHAR;
typedef const WCHAR *LPCWSTR;
typedef const char *LPCSTR;
typedef char *LPSTR;
typedef unsigned long DWORD;
typedef unsigned int UINT;
typedef int BOOL;
typedef void *HANDLE;
typedef struct
{
	DWORD dwFileAttributes;
	char cFileName[260];
} WIN32_FIND_DATAA;

#define WINAPI
#define MAX_PATH 260
#define MAX_GAMENAME 128
#define INVALID_HANDLE_VALUE ((HANDLE)(long)-1)
#define FILE_ATTRIBUTE_DIRECTORY 0x10
#define GENERIC_READ 0x80000000UL
#define GENERIC_WRITE 0x40000000UL
#define CREATE_NEW 1
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define OPEN_ALWAYS 4
#define ERROR_SUCCESS 0
#define ERROR_PATH_NOT_FOUND 3
#define ERROR_ALREADY_EXISTS 183
#define ERROR_INSUFFICIENT_BUFFER 122
#define ERROR_CANNOT_MAKE 82

/* ---------- the file system: paths as the game gives them, a directory ending in '\' */

#define MAXIMUM_FILES 32

static struct test_file
{
	char path[MAX_PATH];
	unsigned char bytes[512];
	DWORD size;
	BOOL used;
} files[MAXIMUM_FILES];

static struct test_file *file_find(const char *path)
{
	int index;

	for (index = 0; index < MAXIMUM_FILES; index++)
		if (files[index].used && !strcmp(files[index].path, path))
			return &files[index];
	return NULL;
}

static struct test_file *file_add(const char *path, const void *bytes, DWORD size)
{
	int index;

	for (index = 0; index < MAXIMUM_FILES; index++)
	{
		if (!files[index].used)
		{
			files[index].used = TRUE;
			strcpy(files[index].path, path);
			memcpy(files[index].bytes, bytes, size);
			files[index].size = size;
			return &files[index];
		}
	}
	CHECK(FALSE, "the file system is full");
	return NULL;
}

/* a directory without its last '\', as RemoveDirectoryA and CreateDirectoryA might be given it */
static void directory_path(const char *path, char *directory)
{
	unsigned long length = (unsigned long)strlen(path);

	strcpy(directory, path);
	if (length && directory[length - 1] != '\\')
		strcat(directory, "\\");
}

static BOOL WINAPI CreateDirectoryA(LPCSTR path, void *attributes)
{
	char directory[MAX_PATH];

	(void)attributes;
	directory_path(path, directory);
	if (file_find(directory))
		return FALSE;
	file_add(directory, "", 0);
	return TRUE;
}

static DWORD WINAPI GetFileAttributesA(LPCSTR path)
{
	char directory[MAX_PATH];

	if (file_find(path))
		return path[strlen(path) - 1] == '\\' ? FILE_ATTRIBUTE_DIRECTORY : 0;
	directory_path(path, directory);
	return file_find(directory) ? FILE_ATTRIBUTE_DIRECTORY : (DWORD)-1;
}

static DWORD last_error;
static DWORD WINAPI GetLastError(void) { return last_error ? last_error : ERROR_CANNOT_MAKE; }

static HANDLE WINAPI CreateFileA(LPCSTR path, DWORD access, DWORD share, void *security, DWORD disposition,
	DWORD flags, HANDLE template_file)
{
	struct test_file *file = file_find(path);

	(void)access; (void)share; (void)security; (void)flags; (void)template_file;
	if (!file && disposition == OPEN_EXISTING)
		return INVALID_HANDLE_VALUE;
	if (!file)
		file = file_add(path, "", 0);
	if (disposition == CREATE_ALWAYS)
		file->size = 0;
	return file;
}

static BOOL WINAPI ReadFile(HANDLE handle, void *buffer, DWORD size, DWORD *read, void *overlapped)
{
	struct test_file *file = handle;

	(void)overlapped;
	*read = size < file->size ? size : file->size;
	memcpy(buffer, file->bytes, *read);
	return TRUE;
}

static BOOL WINAPI WriteFile(HANDLE handle, const void *buffer, DWORD size, DWORD *written, void *overlapped)
{
	struct test_file *file = handle;

	(void)overlapped;
	CHECK(file->size + size <= sizeof(file->bytes), "a file too big for the test");
	memcpy(file->bytes + file->size, buffer, size);
	file->size += size;
	*written = size;
	return TRUE;
}

static BOOL WINAPI DeleteFileA(LPCSTR path)
{
	struct test_file *file = file_find(path);

	if (!file || path[strlen(path) - 1] == '\\')
		return FALSE;
	file->used = FALSE;
	return TRUE;
}

static BOOL directory_has_children(const char *directory)
{
	int index;

	for (index = 0; index < MAXIMUM_FILES; index++)
	{
		if (files[index].used && strcmp(files[index].path, directory) &&
			!strncmp(files[index].path, directory, strlen(directory)))
		{
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL WINAPI RemoveDirectoryA(LPCSTR path)
{
	char directory[MAX_PATH];
	struct test_file *file;

	directory_path(path, directory);
	file = file_find(directory);
	if (!file || directory_has_children(directory))
		return FALSE;
	file->used = FALSE;
	return TRUE;
}

/* a find: the entries directly in a directory ("<directory>\*"), dots left out as the port's are */
static struct test_find
{
	char directory[MAX_PATH];
	int next;
	BOOL used;
} finds[4];

static BOOL find_next(struct test_find *find, WIN32_FIND_DATAA *data)
{
	unsigned long length = (unsigned long)strlen(find->directory);

	for (; find->next < MAXIMUM_FILES; find->next++)
	{
		const char *path = files[find->next].path;
		const char *rest = path + length;
		const char *separator;

		if (!files[find->next].used || strncmp(path, find->directory, length) || !*rest)
			continue;
		separator = strchr(rest, '\\');
		if (separator && separator[1])
			continue;
		memset(data, 0, sizeof(*data));
		memcpy(data->cFileName, rest, separator ? (size_t)(separator - rest) : strlen(rest));
		data->dwFileAttributes = separator ? FILE_ATTRIBUTE_DIRECTORY : 0;
		find->next++;
		return TRUE;
	}
	return FALSE;
}

static HANDLE WINAPI FindFirstFileA(LPCSTR pattern, WIN32_FIND_DATAA *data)
{
	int index;

	for (index = 0; index < 4; index++)
	{
		if (!finds[index].used)
		{
			unsigned long length = (unsigned long)strlen(pattern);

			CHECK(length && pattern[length - 1] == '*', "a pattern other than <directory>\\*: %s", pattern);
			finds[index].used = TRUE;
			memcpy(finds[index].directory, pattern, length - 1);
			finds[index].directory[length - 1] = 0;
			finds[index].next = 0;
			if (!find_next(&finds[index], data))
			{
				finds[index].used = FALSE;
				return INVALID_HANDLE_VALUE;
			}
			return &finds[index];
		}
	}
	CHECK(FALSE, "too many finds open");
	return INVALID_HANDLE_VALUE;
}

static BOOL WINAPI FindNextFileA(HANDLE handle, WIN32_FIND_DATAA *data)
{
	return find_next(handle, data);
}

static BOOL WINAPI CloseHandle(HANDLE handle)
{
	if (handle >= (HANDLE)&finds[0] && handle < (HANDLE)&finds[4])
		((struct test_find *)handle)->used = FALSE;
	return TRUE;
}

static unsigned long wcslen(const WCHAR *text)
{
	unsigned long length = 0;

	while (text[length])
		length++;
	return length;
}

#include "under_test.inc"

/* ---------- the cases */

static void wide(const char *text, WCHAR *out)
{
	while ((*out++ = (WCHAR)(unsigned char)*text++) != 0)
		;
}

static int open_finds(void)
{
	int index, count = 0;

	for (index = 0; index < 4; index++)
		count += finds[index].used;
	return count;
}

/* build 1749's profile as its dump has it, named by the Xbox, its name in single bytes */
#define DUMP_DIRECTORY "u:\\UDATA\\122A17771B9E\\"

static void dump_profile(const char *meta, DWORD meta_size)
{
	CreateDirectoryA("u:\\", NULL);
	CreateDirectoryA("u:\\UDATA", NULL);
	CreateDirectoryA(DUMP_DIRECTORY, NULL);
	file_add(DUMP_DIRECTORY "SaveMeta.xbx", meta, meta_size);
	file_add(DUMP_DIRECTORY "blam.sav", "profile", 7);
	file_add(DUMP_DIRECTORY "SaveImage.xbx", "image", 5);
}

int main(int argc, char **argv)
{
	const char *case_name = argc > 1 ? argv[1] : "";
	WCHAR name[MAX_GAMENAME];
	WCHAR wanted[MAX_GAMENAME];
	char path[MAX_PATH];

	CASE("dump-name-read")
	{
		dump_profile("Name=New001\r\n", 13);
		CHECK(save_read_name(DUMP_DIRECTORY, name, MAX_GAMENAME), "no name read");
		wide("New001", wanted);
		CHECK(save_names_equal(name, wanted), "1749's \"Name=\" line not read as New001");
		return 0;
	}
	CASE("xbox-name-read")
	{
		/* the Xbox's own: UTF-16 after a byte order mark, a "Name=" line among others */
		static const unsigned char meta[] = { 0xFF, 0xFE, 'T', 0, '=', 0, 'x', 0, '\r', 0, '\n', 0,
			'N', 0, 'a', 0, 'm', 0, 'e', 0, '=', 0, 'C', 0, 'h', 0, 'i', 0, 'e', 0, 'f', 0, '\r', 0, '\n', 0 };

		dump_profile((const char *)meta, sizeof(meta));
		CHECK(save_read_name(DUMP_DIRECTORY, name, MAX_GAMENAME), "no name read");
		wide("Chief", wanted);
		CHECK(save_names_equal(name, wanted), "the Xbox's UTF-16 \"Name=\" line not read as Chief");
		return 0;
	}
	CASE("own-name-read")
	{
		/* this build's: the name alone, and one that only looks like the Xbox's line is still a name */
		DWORD result;

		wide("Name=OBSIDIAN", wanted);
		result = XCreateSaveGame("u:\\", wanted, OPEN_ALWAYS, 0, path, sizeof(path));
		CHECK(result == ERROR_SUCCESS, "not made (%lu)", result);
		CHECK(save_read_name(path, name, MAX_GAMENAME), "no name read");
		CHECK(save_names_equal(name, wanted), "this build's own name not read whole");
		return 0;
	}
	CASE("rename-dump-profile")
	{
		/* synchronize_metadata_display_name_with_profile_name: the new name made, the old deleted */
		DWORD result;

		dump_profile("Name=New001\r\n", 13);
		wide("OBSIDIAN", wanted);
		result = XCreateSaveGame("u:\\", wanted, CREATE_NEW, 0, path, sizeof(path));
		CHECK(result == ERROR_SUCCESS, "the new name not made (%lu)", result);
		wide("New001", name);
		result = XDeleteSaveGame("u:\\", name);
		CHECK(result == ERROR_SUCCESS, "the old name not found (%lu): the profile would be listed twice", result);
		CHECK(GetFileAttributesA(DUMP_DIRECTORY) == (DWORD)-1, "1749's directory left");
		CHECK(GetFileAttributesA(path) == FILE_ATTRIBUTE_DIRECTORY, "the new directory deleted too");
		CHECK(!open_finds(), "a find left open");
		return 0;
	}
	CASE("dump-profile-exists")
	{
		/* a new profile of 1749's profile's name is refused, opening it finds 1749's directory */
		DWORD result;

		dump_profile("Name=New001\r\n", 13);
		wide("New001", name);
		result = XCreateSaveGame("u:\\", name, CREATE_NEW, 0, path, sizeof(path));
		CHECK(result == ERROR_ALREADY_EXISTS, "a second New001 made (%lu)", result);
		result = XCreateSaveGame("u:\\", name, OPEN_EXISTING, 0, path, sizeof(path));
		CHECK(result == ERROR_SUCCESS && !strcmp(path, DUMP_DIRECTORY), "1749's directory not opened: %s", path);
		CHECK(!open_finds(), "a find left open");
		return 0;
	}
	CASE("unknown-name")
	{
		DWORD result;

		dump_profile("Name=New001\r\n", 13);
		wide("Nobody", name);
		CHECK(XDeleteSaveGame("u:\\", name) == ERROR_PATH_NOT_FOUND, "a save that isn't there deleted");
		CHECK(GetFileAttributesA(DUMP_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY, "1749's directory deleted");
		result = XCreateSaveGame("u:\\", name, OPEN_EXISTING, 0, path, sizeof(path));
		CHECK(result == ERROR_PATH_NOT_FOUND, "a save that isn't there opened (%lu)", result);
		CHECK(!open_finds(), "a find left open");
		return 0;
	}
	fprintf(stderr, "unknown case '%s'\n", case_name);
	return 2;
}
