#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include "ReviveInject.h"

FILE* g_LogFile = NULL;

int wmain(int argc, wchar_t *argv[]) {
	if (argc < 2) {
		printf("usage: ReviveInjector.exe [/handle] <process path/process handle>\n");
		return -1;
	}

	WCHAR LogPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, LogPath)))
	{
		wcsncat(LogPath, L"\\Revive", MAX_PATH);
		
		BOOL exists = PathFileExists(LogPath);
		if (!exists)
			exists = CreateDirectory(LogPath, NULL);

		wcsncat(LogPath, L"\\ReviveInjector.log", MAX_PATH);
		if (exists)
			g_LogFile = _wfopen(LogPath, L"w");
	}

	LOG("Launched injector with: %ls\n", GetCommandLine());

	for (int i = 0; i < argc - 1; i++)
	{
		if (wcscmp(argv[i], L"/handle") == 0)
			return OpenProcessAndInject(argv[i + 1]);

		// Prepend the base path
		if (wcscmp(argv[i], L"/base") == 0)
		{
			// Replace all forward slashes with backslashes in the argument
			wchar_t* arg = argv[i + 1];
			for (wchar_t* ptr = arg; *ptr != L'\0'; ptr++)
				if (*ptr == L'/') *ptr = L'\\';

			DWORD size = MAX_PATH;
			WCHAR path[MAX_PATH];
			HKEY oculusKey;
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Oculus VR, LLC\\Oculus", 0, KEY_READ | KEY_WOW64_32KEY, &oculusKey);
			RegQueryValueEx(oculusKey, L"Base", NULL, NULL, (PBYTE)path, &size);
			wcsncat(path, arg, MAX_PATH);
			RegCloseKey(oculusKey);
			return CreateProcessAndInject(path);
		}
	}

	return CreateProcessAndInject(argv[argc - 1]);
}
