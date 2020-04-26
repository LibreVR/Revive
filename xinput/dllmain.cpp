#include <Windows.h>

bool GetReviveBasePath(PWCHAR path, DWORD length)
{
	return RegGetValueA(HKEY_LOCAL_MACHINE, "Software\\Revive", "", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, NULL, path, &length) == ERROR_SUCCESS;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
		wchar_t basePath[MAX_PATH];
		if (GetReviveBasePath(basePath, MAX_PATH))
		{
#if _WIN64
			wcscat_s(basePath, MAX_PATH, L"\\openvr_api64.dll");
			LoadLibrary(basePath);
			wcscpy_s(wcsrchr(basePath, '\\'), MAX_PATH, L"\\LibRevive64.dll");
			LoadLibrary(basePath);
#else
			wcscat_s(basePath, MAX_PATH, L"\\openvr_api32.dll");
			LoadLibrary(basePath);
			wcscpy_s(wcsrchr(basePath, '\\'), MAX_PATH, L"\\LibRevive32.dll");
			LoadLibrary(basePath);
#endif
		}
		break;
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
