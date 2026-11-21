/*
WIN32_ACHIEVEMENTS.C

The achievements' companion program (tools/achievements): Achievements.exe
next to halo.exe, started in the background as the game starts, when
game.achievements and game.achievements_app say so. It watches the log the
game writes (port/linux/game/achievements.c) and shows each achievement as
it is earned; it closes itself once the game has gone. A second start of it
only finds the first.
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

int config_boolean(const char *name);
void platform_log(const char *format, ...);

void achievements_app_start(void)
{
	wchar_t path[MAX_PATH], command[MAX_PATH + 32], directory[MAX_PATH];
	wchar_t *slash;
	DWORD length;
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;

	if (!config_boolean("game.achievements") || !config_boolean("game.achievements_app"))
		return;
	length = GetModuleFileNameW(NULL, directory, MAX_PATH);
	if (!length || length >= MAX_PATH)
		return;
	slash = wcsrchr(directory, L'\\');
	if (!slash)
		return;
	*slash = 0;
	_snwprintf(path, MAX_PATH, L"%ls\\Achievements.exe", directory);
	path[MAX_PATH - 1] = 0;
	if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
		return;
	_snwprintf(command, MAX_PATH + 32, L"\"%ls\" --background", path);
	command[MAX_PATH + 31] = 0;
	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	/* (none of the game's handles: its sockets hold the game's ports) */
	if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, directory, &startup, &process))
	{
		platform_log("achievements: could not start Achievements.exe (error %lu)", (unsigned long)GetLastError());
		return;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
}
