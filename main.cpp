#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"

#include <openvr.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

/// This is the Windows Named Event name that is used to check for HMD connected state.
#define REV_HMD_CONNECTED_EVENT_NAME L"ReviveHMDConnected"

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);
typedef HANDLE(__stdcall* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef BOOL(__stdcall* _FreeLibrary)(HMODULE hModule);

_LoadLibrary TrueLoadLibrary;
_OpenEvent TrueOpenEvent;
_FreeLibrary TrueFreeLibrary;

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

HANDLE WINAPI HookOpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	if (wcscmp(lpName, OVR_HMD_CONNECTED_EVENT_NAME) == 0)
		return ::CreateEventW(NULL, TRUE, TRUE, REV_HMD_CONNECTED_EVENT_NAME);

	return TrueOpenEvent(dwDesiredAccess, bInheritHandle, lpName);
}

BOOL HookFreeLibrary(HMODULE hModule) 
{
	if (hModule == ReviveModule) {
		return true;
	}

	return TrueFreeLibrary(hModule);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	ReviveModule = hModule;

    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
			MH_Initialize();
			MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
			MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
			MH_CreateHook(FreeLibrary, HookFreeLibrary, (PVOID*)&TrueFreeLibrary);
			MH_EnableHook(LoadLibraryW);
			MH_EnableHook(OpenEventW);
			MH_EnableHook(FreeLibrary);
            break;

		case DLL_PROCESS_DETACH:
			MH_RemoveHook(LoadLibraryW);
			MH_RemoveHook(OpenEventW);
			MH_RemoveHook(FreeLibrary);
			MH_Uninitialize();

        default:
            break;
    }
    return TRUE;
}
