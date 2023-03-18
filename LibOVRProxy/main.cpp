#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>
#include <string>
#include <detours/detours.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

static HMODULE(WINAPI* TrueLoadLibraryW)(LPCWSTR lpFileName) = LoadLibraryW;
static HMODULE(WINAPI* TrueLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) = LoadLibraryExW;

WCHAR revModuleName[MAX_PATH];
WCHAR ovrModuleName[MAX_PATH];

HMODULE WINAPI HookLoadLibraryW(LPCWSTR lpFileName)
{
	LPCWSTR name = PathFindFileNameW(lpFileName);
	LPCWSTR ext = PathFindExtensionW(name);
	size_t length = ext - name;

	// Load our own library again so the ref count is incremented.
	if (wcsncmp(name, ovrModuleName, length) == 0)
		return TrueLoadLibraryW(revModuleName);

	return TrueLoadLibraryW(lpFileName);
}

HMODULE WINAPI HookLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
	LPCWSTR name = PathFindFileNameW(lpLibFileName);
	LPCWSTR ext = PathFindExtensionW(name);
	size_t length = ext - name;

	// Load our own library again so the ref count is incremented.
	if (wcsncmp(name, ovrModuleName, length) == 0)
		return TrueLoadLibraryExW(revModuleName, hFile, dwFlags);

	return TrueLoadLibraryExW(lpLibFileName, hFile, dwFlags);
}

void AttachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach((PVOID*)&TrueLoadLibraryW, HookLoadLibraryW);
	DetourAttach((PVOID*)&TrueLoadLibraryExW, HookLoadLibraryExW);
	DetourTransactionCommit();
}

void DetachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach((PVOID*)&TrueLoadLibraryW, HookLoadLibraryW);
	DetourDetach((PVOID*)&TrueLoadLibraryExW, HookLoadLibraryExW);
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
