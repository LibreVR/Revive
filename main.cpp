#include <Windows.h>
#include <stdio.h>
#include "mhook.h"
#include "Shlwapi.h"

#include <openvr.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);

_LoadLibrary TrueLoadLibrary = (_LoadLibrary)GetProcAddress(GetModuleHandle(L"kernel32"), "LoadLibraryW");

HANDLE ReviveModule;

HMODULE WINAPI HookLoadLibrary(LPCWSTR lpFileName)
{
#if defined(_WIN64)
	const char* pBitDepth = "64";
#else
	const char* pBitDepth = "32";
#endif

	LPCWSTR fileName = PathFindFileNameW(lpFileName);
	WCHAR libraryName[MAX_PATH];
	swprintf(libraryName, MAX_PATH, L"LibOVRRT%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
	if (wcscmp(fileName, libraryName) == 0)
		return (HMODULE)ReviveModule;

	return TrueLoadLibrary(lpFileName);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	ReviveModule = hModule;

    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
			Mhook_SetHook((PVOID*)&TrueLoadLibrary, HookLoadLibrary);
			::CreateEventW(NULL, FALSE, vr::VR_IsHmdPresent(), OVR_HMD_CONNECTED_EVENT_NAME);
            break;

		case DLL_PROCESS_DETACH:
			Mhook_Unhook((PVOID*)&HookLoadLibrary);

        default:
            break;
    }
    return TRUE;
}
