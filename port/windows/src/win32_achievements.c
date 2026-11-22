/*
WIN32_ACHIEVEMENTS.C

The achievements' companion program (tools/achievements): Achievements.exe
next to halo.exe, started in the background as the game starts, when
game.achievements and game.achievements_app say so. It watches the log the
game writes (port/linux/game/achievements.c) and shows each achievement as
it is earned; it closes itself once the game has gone. A second start of it
only finds the first. Edit Profile Settings' ACHIEVEMENTS starts it without
--background, to show its list (achievements_app_show).
*/

#include <windows.h>
#include <stdio.h>
#include <string.h>

int config_boolean(const char *name);
void platform_log(const char *format, ...);

/* Achievements.exe next to halo.exe, started with --background (only
watching), or to show its list (Edit Profile Settings' ACHIEVEMENTS: a
second start of it shows the first's). FALSE when it is not there or could
not be started. */
static int achievements_app_launch(int background)
{
	wchar_t path[MAX_PATH], command[MAX_PATH + 32], directory[MAX_PATH];
	wchar_t *slash;
	DWORD length;
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;

	length = GetModuleFileNameW(NULL, directory, MAX_PATH);
	if (!length || length >= MAX_PATH)
		return 0;
	slash = wcsrchr(directory, L'\\');
	if (!slash)
		return 0;
	*slash = 0;
	_snwprintf(path, MAX_PATH, L"%ls\\Achievements.exe", directory);
	path[MAX_PATH - 1] = 0;
	if (GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES)
	{
		if (!background)
			platform_log("achievements: %ls is not there", path);
		return 0;
	}
	_snwprintf(command, MAX_PATH + 32, background ? L"\"%ls\" --background" : L"\"%ls\"", path);
	command[MAX_PATH + 31] = 0;
	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
	/* (the list in front of the game: the one running already brings its
	window up, which Windows lets it do only with the game's leave) */
	if (!background)
		AllowSetForegroundWindow(ASFW_ANY);
	/* (none of the game's handles: its sockets hold the game's ports) */
	if (!CreateProcessW(path, command, NULL, NULL, FALSE, 0, NULL, directory, &startup, &process))
	{
		platform_log("achievements: could not start Achievements.exe (error %lu)", (unsigned long)GetLastError());
		return 0;
	}
	CloseHandle(process.hThread);
	CloseHandle(process.hProcess);
	return 1;
}

void achievements_app_start(void)
{
	if (!config_boolean("game.achievements") || !config_boolean("game.achievements_app"))
		return;
	achievements_app_launch(1);
}

int achievements_app_show(void)
{
	return achievements_app_launch(0);
}
