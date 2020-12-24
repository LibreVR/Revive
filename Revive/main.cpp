#include <Windows.h>
#include <stdio.h>
#include <dxgi.h>
#include <Shlwapi.h>
#include <string>
#include <detours/detours.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

static HMODULE(WINAPI* TrueLoadLibrary)(LPCWSTR lpFileName) = LoadLibraryW;
static HANDLE(WINAPI* TrueOpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName) = OpenEventW;
static HRESULT(WINAPI* TrueDXGIFactory)(REFIID riid, void **ppFactory) = CreateDXGIFactory;

HMODULE revModule;
WCHAR revModuleName[MAX_PATH];
WCHAR ovrModuleName[MAX_PATH];

HRESULT WINAPI HookDXGIFactory(REFIID riid, void **ppFactory)
{
	// We need shared texture support for OpenVR, so force DXGI 1.0 games to use DXGI 1.1
	IDXGIFactory1* pDXGIFactory;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&pDXGIFactory);
	if (FAILED(hr))
		return hr;
	return pDXGIFactory->QueryInterface(riid, ppFactory);
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

	return TrueLoadLibrary(lpFileName);
}

void AttachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttach((PVOID*)&TrueLoadLibrary, HookLoadLibrary);
	DetourAttach((PVOID*)&TrueOpenEvent, HookOpenEvent);
	DetourAttach((PVOID*)&TrueDXGIFactory, HookDXGIFactory);
	DetourTransactionCommit();
}

void DetachDetours()
{
	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourDetach((PVOID*)&TrueLoadLibrary, HookLoadLibrary);
	DetourDetach((PVOID*)&TrueOpenEvent, HookOpenEvent);
	DetourDetach((PVOID*)&TrueDXGIFactory, HookDXGIFactory);
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
