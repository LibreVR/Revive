#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"
#include <map>

#include <openvr.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"
#include "OVR_CAPI_D3D.h"
#include "OVR_CAPI_Audio.h"

/// This is the Windows Named Event name that is used to check for HMD connected state.
#define REV_HMD_CONNECTED_EVENT_NAME L"ReviveHMDConnected"

typedef HANDLE(__stdcall* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef FARPROC(__stdcall* _GetProcAddress)(HMODULE hModule, LPCSTR  lpProcName);

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
	if (wcscmp(lpName, OVR_HMD_CONNECTED_EVENT_NAME) == 0)
		return ::CreateEventW(NULL, TRUE, TRUE, REV_HMD_CONNECTED_EVENT_NAME);

	return TrueOpenEvent(dwDesiredAccess, bInheritHandle, lpName);
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
		MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
		MH_CreateHook(GetProcAddress, HookGetProcAddress, (PVOID*)&TrueGetProcAddress);
		MH_EnableHook(OpenEventW);
		MH_EnableHook(GetProcAddress);
		break;
	case DLL_PROCESS_DETACH:
		MH_RemoveHook(OpenEventW);
		MH_RemoveHook(GetProcAddress);
		MH_Uninitialize();
		break;
	default:
		break;
	}
	return TRUE;
}
