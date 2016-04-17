#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"
#include <map>
#include "OVR_Version.h"
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"
#include "OVR_CAPI_D3D.h"
#include "OVR_CAPI_Audio.h"

#include <openvr.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"

/// This is the Windows Named Event name that is used to check for HMD connected state.
#define REV_HMD_CONNECTED_EVENT_NAME L"ReviveHMDConnected"

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);
typedef HANDLE(__stdcall* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef FARPROC(__stdcall* _GetProcAddress)(HMODULE hModule, LPCSTR  lpProcName);

_LoadLibrary TrueLoadLibrary;
_OpenEvent TrueOpenEvent;
_GetProcAddress TrueGetProcAddress;

#define OVR_ENTRY(x) { #x, &x }
std::map<std::string, void *> lookup = {
	OVR_ENTRY(ovr_ClearShouldRecenterFlag),
	OVR_ENTRY(ovr_CommitTextureSwapChain),
	OVR_ENTRY(ovr_Create),
	OVR_ENTRY(ovr_CreateMirrorTextureDX),
	OVR_ENTRY(ovr_CreateMirrorTextureGL),
	OVR_ENTRY(ovr_CreateTextureSwapChainDX),
	OVR_ENTRY(ovr_CreateTextureSwapChainGL),
	OVR_ENTRY(ovr_Destroy),
	OVR_ENTRY(ovr_DestroyMirrorTexture),
	OVR_ENTRY(ovr_DestroyTextureSwapChain),
	OVR_ENTRY(ovr_GetAudioDeviceInGuid),
	OVR_ENTRY(ovr_GetAudioDeviceInGuidStr),
	OVR_ENTRY(ovr_GetAudioDeviceInWaveId),
	OVR_ENTRY(ovr_GetAudioDeviceOutGuid),
	OVR_ENTRY(ovr_GetAudioDeviceOutGuidStr),
	OVR_ENTRY(ovr_GetAudioDeviceOutWaveId),
	OVR_ENTRY(ovr_GetBool),
	OVR_ENTRY(ovr_GetConnectedControllerTypes),
	OVR_ENTRY(ovr_GetFloat),
	OVR_ENTRY(ovr_GetFloatArray),
	OVR_ENTRY(ovr_GetFovTextureSize),
	OVR_ENTRY(ovr_GetHmdDesc),
	OVR_ENTRY(ovr_GetInputState),
	OVR_ENTRY(ovr_GetInt),
	OVR_ENTRY(ovr_GetLastErrorInfo),
	OVR_ENTRY(ovr_GetMirrorTextureBufferDX),
	OVR_ENTRY(ovr_GetMirrorTextureBufferGL),
	OVR_ENTRY(ovr_GetPredictedDisplayTime),
	OVR_ENTRY(ovr_GetRenderDesc),
	OVR_ENTRY(ovr_GetSessionStatus),
	OVR_ENTRY(ovr_GetString),
	OVR_ENTRY(ovr_GetTextureSwapChainBufferDX),
	OVR_ENTRY(ovr_GetTextureSwapChainBufferGL),
	OVR_ENTRY(ovr_GetTextureSwapChainCurrentIndex),
	OVR_ENTRY(ovr_GetTextureSwapChainDesc),
	OVR_ENTRY(ovr_GetTextureSwapChainLength),
	OVR_ENTRY(ovr_GetTimeInSeconds),
	OVR_ENTRY(ovr_GetTrackerCount),
	OVR_ENTRY(ovr_GetTrackerDesc),
	OVR_ENTRY(ovr_GetTrackerPose),
	OVR_ENTRY(ovr_GetTrackingOriginType),
	OVR_ENTRY(ovr_GetTrackingState),
	OVR_ENTRY(ovr_GetVersionString),
	OVR_ENTRY(ovr_Initialize),
	OVR_ENTRY(ovr_RecenterTrackingOrigin),
	OVR_ENTRY(ovr_SetBool),
	OVR_ENTRY(ovr_SetControllerVibration),
	OVR_ENTRY(ovr_SetFloat),
	OVR_ENTRY(ovr_SetFloatArray),
	OVR_ENTRY(ovr_SetInt),
	OVR_ENTRY(ovr_SetString),
	OVR_ENTRY(ovr_SetTrackingOriginType),
	OVR_ENTRY(ovr_Shutdown),
	OVR_ENTRY(ovr_SubmitFrame),
	OVR_ENTRY(ovr_TraceMessage),
};

HANDLE ReviveModule;
WCHAR libraryName[MAX_PATH];

FARPROC HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName) 
{
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (wcscmp(moduleName, libraryName) == 0) {
		auto iter = lookup.find(lpProcName);
		if (iter != lookup.end()) {
			return (FARPROC)iter->second;
		}
	}
	return TrueGetProcAddress(hModule, lpProcName);
}

bool isUnityGame = false;
HMODULE WINAPI HookLoadLibrary(LPCWSTR lpFileName)
{
	LPCWSTR fileName = PathFindFileNameW(lpFileName);
	if (!isUnityGame && wcscmp(fileName, libraryName) == 0)
		return (HMODULE)ReviveModule;
	auto moduleHandle = TrueLoadLibrary(lpFileName);
	if (wcscmp(fileName, L"OVRPlugin.dll") == 0) {
		isUnityGame = true;
	}
	return moduleHandle;
}

HANDLE WINAPI HookOpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	if (wcscmp(lpName, OVR_HMD_CONNECTED_EVENT_NAME) == 0)
		return ::CreateEventW(NULL, TRUE, TRUE, REV_HMD_CONNECTED_EVENT_NAME);

	return TrueOpenEvent(dwDesiredAccess, bInheritHandle, lpName);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	ReviveModule = hModule;
#if defined(_WIN64)
	const char* pBitDepth = "64";
#else
	const char* pBitDepth = "32";
#endif
	swprintf(libraryName, MAX_PATH, L"LibOVRRT%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
    switch(ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
			MH_Initialize();
			MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
			MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
			MH_CreateHook(GetProcAddress, HookGetProcAddress, (PVOID*)&TrueGetProcAddress);
			MH_EnableHook(LoadLibraryW);
			MH_EnableHook(OpenEventW);
			MH_EnableHook(GetProcAddress);
            break;

		case DLL_PROCESS_DETACH:
			MH_RemoveHook(LoadLibraryW);
			MH_RemoveHook(OpenEventW);
			MH_RemoveHook(GetProcAddress);
			MH_Uninitialize();

        default:
            break;
    }
    return TRUE;
}
