#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include "ReviveInject.h"

int wmain(int argc, wchar_t *argv[]) {
	if (argc < 2) {
		printf("usage: ReviveInjector.exe [/handle|/base] <process path/process handle>\n");
		return -1;
	}
	if (wcscmp(argv[1], L"/handle") == 0 && argc > 2)
		return OpenProcessAndInject(argv[2]);

	WCHAR path[MAX_PATH];
	if (wcscmp(argv[1], L"/base") == 0 && argc > 2)
	{
		// Replace all forward slashes with backslashes in the path
		for (wchar_t* ptr = argv[2]; *ptr != L'\0'; ptr++)
			if (*ptr == L'/') *ptr = L'\\';

		// Prepend the base path
		DWORD size = MAX_PATH;
		HKEY oculusKey;
		RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Oculus VR, LLC\\Oculus", 0, KEY_READ | KEY_WOW64_32KEY, &oculusKey);
		RegQueryValueEx(oculusKey, L"Base", NULL, NULL, (PBYTE)path, &size);
		RegCloseKey(oculusKey);
		wcsncat(path, argv[2], MAX_PATH);
	}
	else
	{
		wcsncpy(path, argv[1], MAX_PATH);
	}

	return CreateProcessAndInject(path);
}
