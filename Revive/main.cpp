#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"
#include "IAT_Hooking.h"

#include <openvr.h>
#include <string>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

#define GetIsViewerEntitled 0x186B58B1

typedef struct ovrMessage *ovrMessageHandle;

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);
typedef HANDLE(__stdcall* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef bool(__cdecl* _IsError)(const ovrMessageHandle obj);

_LoadLibrary TrueLoadLibrary;
_OpenEvent TrueOpenEvent;
_IsError TrueIsError;

HMODULE ReviveModule;
WCHAR revModuleName[MAX_PATH];
WCHAR ovrModuleName[MAX_PATH];
WCHAR ovrPlatformName[MAX_PATH];

bool __cdecl ovr_Message_IsError(const ovrMessageHandle obj)
{
	// Assuming the structure doesn't changed, the type begins at the third integer.
	PDWORD type = ((PDWORD)obj) + 2;

	// Never return an error for the entitlement check.
	// TODO: Detect whether the error is triggered by a missing headset.
	if (*type == GetIsViewerEntitled)
		return false;

	return TrueIsError(obj);
}

bool __cdecl ovr_IsEntitled()
{
	return true;
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
	LPCWSTR name = PathFindFileNameW(lpFileName);
	LPCWSTR ext = PathFindExtensionW(name);
	size_t length = ext - name;

	// Load our own library again so the ref count is incremented.
	if (wcsncmp(name, ovrModuleName, length) == 0)
		return TrueLoadLibrary(revModuleName);

	// The following functions will patch the module, so we have to load it here.
	HMODULE module = TrueLoadLibrary(lpFileName);

	// Patch the export table of Oculus Platform to point to our entitlement functions.
	if (wcsncmp(name, ovrPlatformName, length) == 0)
	{
		TrueIsError = (_IsError)DetourEATptr("ovr_Message_IsError", ovr_Message_IsError, module);
		DetourEATptr("ovr_IsEntitled", ovr_IsEntitled, module);
	}

	return module;
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
			swprintf(revModuleName, MAX_PATH, L"LibRevive%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
			swprintf(ovrPlatformName, MAX_PATH, L"LibOVRPlatform%hs_%d", pBitDepth, OVR_MAJOR_VERSION);
			TrueIsError = (_IsError)DetourIATptr("ovr_Message_IsError", ovr_Message_IsError, GetModuleHandle(NULL));
			DetourIATptr("ovr_IsEntitled", ovr_IsEntitled, GetModuleHandle(NULL));
			MH_Initialize();
			MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
			MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
			MH_EnableHook(LoadLibraryW);
			MH_EnableHook(OpenEventW);
			break;
		case DLL_PROCESS_DETACH:
			MH_RemoveHook(LoadLibraryW);
			MH_RemoveHook(OpenEventW);
			MH_Uninitialize();
			break;
		default:
			break;
	}
	return TRUE;
}
