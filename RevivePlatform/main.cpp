#include <Windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "MinHook.h"

HMODULE PlatformModule;
WCHAR ovrModuleName[MAX_PATH];

typedef uint64_t ovrID;
typedef uint64_t ovrRequest;
typedef struct ovrMessage *ovrMessageHandle;
typedef enum ovrMessageType_ ovrMessageType;
typedef struct ovrError *ovrErrorHandle;

typedef void(__stdcall* _FreeMessage)(ovrMessageHandle);
typedef bool(__stdcall* _IsError)(const ovrMessageHandle);
typedef ovrMessageType(__stdcall* _GetType)(const ovrMessageHandle);
typedef ovrErrorHandle(__stdcall* _GetError)(const ovrMessageHandle);
typedef const char*(__stdcall* _GetMessage)(const ovrErrorHandle);
typedef ovrID(__stdcall* _GetLoggedInUserID)(void);

_FreeMessage TrueFreeMessage = NULL;
_IsError TrueIsError = NULL;
_GetType TrueGetType = NULL;
_GetError TrueGetError = NULL;
_GetMessage TrueGetMessage = NULL;
_GetLoggedInUserID TrueGetLoggedInUserID = NULL;

typedef HMODULE(__stdcall* _GetModuleHandle)(LPCWSTR lpModuleName);
_GetModuleHandle TrueGetModuleHandle;

void GetRuntimePath(LPWSTR path, DWORD size)
{
	HKEY oculusKey;
	RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Oculus VR, LLC\\Oculus", 0, KEY_READ | KEY_WOW64_32KEY, &oculusKey);
	RegQueryValueEx(oculusKey, L"Base", NULL, NULL, (PBYTE)path, &size);
	wcsncat(path, L"Support\\oculus-runtime\\", size);
}

HMODULE WINAPI HookGetModuleHandle(LPCWSTR lpModuleName)
{
	if (lpModuleName && wcscmp(lpModuleName, ovrModuleName) == 0) {
		return PlatformModule;
	}

	return TrueGetModuleHandle(lpModuleName);
}

extern "C" __declspec(dllexport) ovrRequest ovr_Entitlement_GetIsViewerEntitled()
{
	// Sorry, we can't verify your entitlement without an Oculus Rift...
	return 0;
}

extern "C" __declspec(dllexport) void ovr_FreeMessage(ovrMessageHandle obj)
{
	if (!TrueFreeMessage)
		TrueFreeMessage = (_FreeMessage)GetProcAddress(PlatformModule, "ovr_FreeMessage");

	TrueFreeMessage(obj);
}

extern "C" __declspec(dllexport) bool ovr_Message_IsError(const ovrMessageHandle obj)
{
	if (!TrueIsError)
		TrueIsError = (_IsError)GetProcAddress(PlatformModule, "ovr_Message_IsError");

	return TrueIsError(obj);
}

extern "C" __declspec(dllexport) ovrMessageType ovr_Message_GetType(const ovrMessageHandle obj)
{
	if (!TrueGetType)
		TrueGetType = (_GetType)GetProcAddress(PlatformModule, "ovr_Message_GetType");

	return TrueGetType(obj);
}

extern "C" __declspec(dllexport) ovrErrorHandle ovr_Message_GetError(const ovrMessageHandle obj)
{
	if (!TrueGetError)
		TrueGetError = (_GetError)GetProcAddress(PlatformModule, "ovr_Message_GetError");

	return TrueGetError(obj);
}

extern "C" __declspec(dllexport) const char * ovr_Error_GetMessage(const ovrErrorHandle obj)
{
	if (!TrueGetMessage)
		TrueGetMessage = (_GetMessage)GetProcAddress(PlatformModule, "ovr_Error_GetMessage");

	return TrueGetMessage(obj);
}

extern "C" __declspec(dllexport) ovrID ovr_GetLoggedInUserID()
{
	if (!TrueGetLoggedInUserID)
		TrueGetLoggedInUserID = (_GetLoggedInUserID)GetProcAddress(PlatformModule, "ovr_GetLoggedInUserID");

	return TrueGetLoggedInUserID();
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	WCHAR path[MAX_PATH];
#if defined(_WIN64)
	const char* pBitDepth = "64";
#else
	const char* pBitDepth = "32";
#endif
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		MH_Initialize();
		MH_CreateHook(GetModuleHandleW, HookGetModuleHandle, (PVOID*)&TrueGetModuleHandle);
		MH_EnableHook(GetModuleHandleW);
		swprintf(ovrModuleName, MAX_PATH, L"LibOVRPlatform%hs_1.dll", pBitDepth);
		GetRuntimePath(path, MAX_PATH);
		wcsncat(path, ovrModuleName, MAX_PATH);
		PlatformModule = LoadLibraryW(path);
		break;
	case DLL_PROCESS_DETACH:
		MH_RemoveHook(GetModuleHandleW);
		MH_Uninitialize();
		break;
	default:
		break;
	}
	return TRUE;
}
