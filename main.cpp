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

WCHAR ovrModuleName[MAX_PATH];

#define OVR_FUNC(x) { #x, &x }
std::map<std::string, void *> ovrFuncAddressMap = {
	OVR_FUNC(ovr_ClearShouldRecenterFlag),
	OVR_FUNC(ovr_CommitTextureSwapChain),
	OVR_FUNC(ovr_Create),
	OVR_FUNC(ovr_CreateMirrorTextureDX),
	OVR_FUNC(ovr_CreateMirrorTextureGL),
	OVR_FUNC(ovr_CreateTextureSwapChainDX),
	OVR_FUNC(ovr_CreateTextureSwapChainGL),
	OVR_FUNC(ovr_Destroy),
	OVR_FUNC(ovr_DestroyMirrorTexture),
	OVR_FUNC(ovr_DestroyTextureSwapChain),
	OVR_FUNC(ovr_GetAudioDeviceInGuid),
	OVR_FUNC(ovr_GetAudioDeviceInGuidStr),
	OVR_FUNC(ovr_GetAudioDeviceInWaveId),
	OVR_FUNC(ovr_GetAudioDeviceOutGuid),
	OVR_FUNC(ovr_GetAudioDeviceOutGuidStr),
	OVR_FUNC(ovr_GetAudioDeviceOutWaveId),
	OVR_FUNC(ovr_GetBool),
	OVR_FUNC(ovr_GetConnectedControllerTypes),
	OVR_FUNC(ovr_GetFloat),
	OVR_FUNC(ovr_GetFloatArray),
	OVR_FUNC(ovr_GetFovTextureSize),
	OVR_FUNC(ovr_GetHmdDesc),
	OVR_FUNC(ovr_GetInputState),
	OVR_FUNC(ovr_GetInt),
	OVR_FUNC(ovr_GetLastErrorInfo),
	OVR_FUNC(ovr_GetMirrorTextureBufferDX),
	OVR_FUNC(ovr_GetMirrorTextureBufferGL),
	OVR_FUNC(ovr_GetPredictedDisplayTime),
	OVR_FUNC(ovr_GetRenderDesc),
	OVR_FUNC(ovr_GetSessionStatus),
	OVR_FUNC(ovr_GetString),
	OVR_FUNC(ovr_GetTextureSwapChainBufferDX),
	OVR_FUNC(ovr_GetTextureSwapChainBufferGL),
	OVR_FUNC(ovr_GetTextureSwapChainCurrentIndex),
	OVR_FUNC(ovr_GetTextureSwapChainDesc),
	OVR_FUNC(ovr_GetTextureSwapChainLength),
	OVR_FUNC(ovr_GetTimeInSeconds),
	OVR_FUNC(ovr_GetTrackerCount),
	OVR_FUNC(ovr_GetTrackerDesc),
	OVR_FUNC(ovr_GetTrackerPose),
	OVR_FUNC(ovr_GetTrackingOriginType),
	OVR_FUNC(ovr_GetTrackingState),
	OVR_FUNC(ovr_GetVersionString),
	OVR_FUNC(ovr_Initialize),
	OVR_FUNC(ovr_RecenterTrackingOrigin),
	OVR_FUNC(ovr_SetBool),
	OVR_FUNC(ovr_SetControllerVibration),
	OVR_FUNC(ovr_SetFloat),
	OVR_FUNC(ovr_SetFloatArray),
	OVR_FUNC(ovr_SetInt),
	OVR_FUNC(ovr_SetString),
	OVR_FUNC(ovr_SetTrackingOriginType),
	OVR_FUNC(ovr_Shutdown),
	OVR_FUNC(ovr_SubmitFrame),
	OVR_FUNC(ovr_TraceMessage),
};

FARPROC HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName) 
{
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (wcscmp(moduleName, ovrModuleName) == 0) {
		auto iter = ovrFuncAddressMap.find(lpProcName);
		if (iter != ovrFuncAddressMap.end()) {
			return (FARPROC)iter->second;
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
