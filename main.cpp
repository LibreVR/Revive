#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"

#include <openvr.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);
typedef HANDLE(__stdcall* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef FARPROC(__stdcall* _GetProcAddress)(HMODULE hModule, LPCSTR  lpProcName);

_LoadLibrary TrueLoadLibrary;
_OpenEvent TrueOpenEvent;
_GetProcAddress TrueGetProcAddress;

HMODULE ReviveModule;
WCHAR ovrModuleName[MAX_PATH];

FARPROC HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName) 
{
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (wcscmp(moduleName, ovrModuleName) == 0) {
		FARPROC reviveFuncAddress = TrueGetProcAddress(ReviveModule, lpProcName);
		if (reviveFuncAddress) {
			return reviveFuncAddress;
		}
	}

	return TrueGetProcAddress(hModule, lpProcName);
}

HANDLE WINAPI HookOpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	// Don't touch this, it heavily affects performance in Unity games.
	if (wcscmp(lpName, OVR_HMD_CONNECTED_EVENT_NAME) == 0)
		return ::CreateEventW(NULL, TRUE, TRUE, NULL);

	return TrueOpenEvent(dwDesiredAccess, bInheritHandle, lpName);
}

HMODULE WINAPI HookLoadLibrary(LPCWSTR lpFileName)
{
	HMODULE hModule = TrueLoadLibrary(lpFileName);

	// Only enable the GetProcAddress hook when the Oculus Runtime was loaded.
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (wcscmp(moduleName, ovrModuleName) == 0)
		MH_EnableHook(GetProcAddress);

	return hModule;
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
#if defined(_WIN64)
	const char* pBitDepth = "64";
#else
	const char* pBitDepth = "32";
#endif
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			ReviveModule = (HMODULE)hModule;
			swprintf(ovrModuleName, MAX_PATH, L"LibOVRRT%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
			MH_Initialize();
			MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
			MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
			MH_CreateHook(GetProcAddress, HookGetProcAddress, (PVOID*)&TrueGetProcAddress);
			MH_EnableHook(LoadLibraryW);
			MH_EnableHook(OpenEventW);
			break;
		case DLL_PROCESS_DETACH:
			MH_RemoveHook(LoadLibraryW);
			MH_RemoveHook(OpenEventW);
			MH_RemoveHook(GetProcAddress);
			MH_Uninitialize();
			break;
		default:
			break;
	}
	return TRUE;
}
