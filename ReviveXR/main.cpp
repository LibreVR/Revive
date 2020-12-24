#include <Windows.h>
#include <stdio.h>
#include <dxgi.h>
#include <d3d11.h>
#include <Shlwapi.h>
#include <string>
#include <detours/detours.h>

#include "OVR_CAPI.h"
#include "OVR_Version.h"

static HMODULE(WINAPI* TrueLoadLibrary)(LPCWSTR lpFileName) = LoadLibraryW;
static HANDLE(WINAPI* TrueOpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName) = OpenEventW;

HMODULE revModule;
WCHAR revModuleName[MAX_PATH];
WCHAR ovrModuleName[MAX_PATH];

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

	return TrueLoadLibrary(lpFileName);
}

void AttachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)TrueLoadLibrary, HookLoadLibrary);
	DetourAttach(&(PVOID&)TrueOpenEvent, HookOpenEvent);
	DetourTransactionCommit();
}

void DetachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach(&(PVOID&)TrueLoadLibrary, HookLoadLibrary);
	DetourDetach(&(PVOID&)TrueOpenEvent, HookOpenEvent);
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
			revModule = (HMODULE)hModule;
			GetModuleFileName(revModule, revModuleName, MAX_PATH);
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
