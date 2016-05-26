#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"
#include "IAT_Hooking.h"

#include <openvr.h>
#include <string>

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
WCHAR ovrPlatformName[MAX_PATH];

// Use Dbgview.exe to view these logs
void LOG(const wchar_t *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::wstring adjustedFmt(fmt);
	wchar_t buff[255];
	adjustedFmt.insert(0, L"Revive: ");
	wvsprintf(buff, adjustedFmt.c_str(), args);
	OutputDebugString(buff);
	va_end(args);
}

void LOG(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	std::string adjustedFmt(fmt);
	char buff[255];
	adjustedFmt.insert(0, "Revive: ");
	vsprintf(buff, adjustedFmt.c_str(), args);
	OutputDebugStringA(buff);
	va_end(args);
}

ULONG ovr_Entitlement_GetIsViewerEntitled() {
	return 0; // this part doesn't work
}

FARPROC HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName) 
{
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (strcmp(lpProcName, "ovr_Entitlement_GetIsViewerEntitled") == 0) {
		LOG("ovr_Entitlement_GetIsViewerEntitled hooked");
		return (FARPROC)ovr_Entitlement_GetIsViewerEntitled;
	}

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

	if (wcscmp(moduleName, L"mono.dll") == 0) {
		if (DetourIATptr("GetProcAddress", HookGetProcAddress, hModule) != 0) {
			LOG("Hooked GetProcAddress in mono.dll IAT");
		}
	}
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
			swprintf(ovrPlatformName, MAX_PATH, L"LibOVRPlatform%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
			DetourIATptr("ovr_Entitlement_GetIsViewerEntitled", ovr_Entitlement_GetIsViewerEntitled, GetModuleHandle(NULL));
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
