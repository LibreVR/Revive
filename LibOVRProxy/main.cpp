#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <string>

#include "MinHook.h"
#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"
#include "microprofile.h"

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);

_LoadLibrary TrueLoadLibrary;

WCHAR revModuleName[MAX_PATH];
WCHAR ovrModuleName[MAX_PATH];

HMODULE WINAPI HookLoadLibrary(LPCWSTR lpFileName)
{
	LPCWSTR name = PathFindFileNameW(lpFileName);
	LPCWSTR ext = PathFindExtensionW(name);
	size_t length = ext - name;

	// Load our own library again so the ref count is incremented.
	if (wcsncmp(name, ovrModuleName, length) == 0)
		return TrueLoadLibrary(revModuleName);

	return TrueLoadLibrary(lpFileName);
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
		GetModuleFileName((HMODULE)hModule, revModuleName, MAX_PATH);
		swprintf(ovrModuleName, MAX_PATH, L"LibOVRRT%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
		MH_Initialize();
		MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
		MH_EnableHook(MH_ALL_HOOKS);
		break;
	case DLL_PROCESS_DETACH:
		MH_Uninitialize();
		break;
	default:
		break;
	}
	return TRUE;
}
