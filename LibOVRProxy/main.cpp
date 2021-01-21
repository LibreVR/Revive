#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <string>
#include <detours/detours.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

static HMODULE(WINAPI* TrueLoadLibrary)(LPCWSTR lpFileName) = LoadLibraryW;

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

void AttachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)TrueLoadLibrary, HookLoadLibrary);
	DetourTransactionCommit();
}

void DetachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)TrueLoadLibrary, HookLoadLibrary);
	DetourTransactionCommit();
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	if (DetourIsHelperProcess())
		return TRUE;

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

		DetourRestoreAfterWith();
		AttachDetours();
		break;
	case DLL_PROCESS_DETACH:
		DetachDetours();
		break;
	default:
		break;
	}
	return TRUE;
}
