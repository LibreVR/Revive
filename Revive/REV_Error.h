#pragma once

#include "OVR_CAPI.h"
#include "openvr.h"

ovrResult REV_InitErrorToOvrError(vr::EVRInitError error)
{
	switch (error)
	{
		case vr::VRInitError_None: return ovrSuccess;
		case vr::VRInitError_Unknown: return ovrError_Initialize;
		case vr::VRInitError_Init_InstallationNotFound: return ovrError_LibLoad;
		case vr::VRInitError_Init_InstallationCorrupt: return ovrError_LibLoad;
		case vr::VRInitError_Init_VRClientDLLNotFound: return ovrError_LibLoad;
		case vr::VRInitError_Init_FileNotFound: return ovrError_LibLoad;
		case vr::VRInitError_Init_FactoryNotFound: return ovrError_ServiceConnection;
		case vr::VRInitError_Init_InterfaceNotFound: return ovrError_ServiceConnection;
		case vr::VRInitError_Init_InvalidInterface: return ovrError_MismatchedAdapters;
		case vr::VRInitError_Init_UserConfigDirectoryInvalid: return ovrError_Initialize;
		case vr::VRInitError_Init_HmdNotFound: return ovrError_NoHmd;
		case vr::VRInitError_Init_NotInitialized: return ovrError_Initialize;
		case vr::VRInitError_Init_PathRegistryNotFound: return ovrError_Initialize;
		case vr::VRInitError_Init_NoConfigPath: return ovrError_Initialize;
		case vr::VRInitError_Init_NoLogPath: return ovrError_Initialize;
		case vr::VRInitError_Init_PathRegistryNotWritable: return ovrError_Initialize;
		case vr::VRInitError_Init_AppInfoInitFailed: return ovrError_ServerStart;
		//case vr::VRInitError_Init_Retry: return ovrError_Reinitialization; // Internal
		case vr::VRInitError_Init_InitCanceledByUser: return ovrError_Initialize;
		case vr::VRInitError_Init_AnotherAppLaunching: return ovrError_ServerStart;
		case vr::VRInitError_Init_SettingsInitFailed: return ovrError_Initialize;
		case vr::VRInitError_Init_ShuttingDown: return ovrError_ServerStart;
		case vr::VRInitError_Init_TooManyObjects: return ovrError_Initialize;
		case vr::VRInitError_Init_NoServerForBackgroundApp: return ovrError_ServerStart;
		case vr::VRInitError_Init_NotSupportedWithCompositor: return ovrError_Initialize;
		case vr::VRInitError_Init_NotAvailableToUtilityApps: return ovrError_Initialize;
		default: return ovrError_RuntimeException;
	}
}

ovrResult REV_CompositorErrorToOvrError(vr::EVRCompositorError error)
{
	switch (error)
	{
		case vr::VRCompositorError_None: return ovrSuccess;
		case vr::VRCompositorError_IncompatibleVersion: return ovrError_ServiceError;
		case vr::VRCompositorError_DoNotHaveFocus: return ovrSuccess_NotVisible;
		case vr::VRCompositorError_InvalidTexture: return ovrError_TextureSwapChainInvalid;
		case vr::VRCompositorError_IsNotSceneApplication: return ovrError_InvalidSession;
		case vr::VRCompositorError_TextureIsOnWrongDevice: return ovrError_TextureSwapChainInvalid;
		case vr::VRCompositorError_TextureUsesUnsupportedFormat: return ovrError_TextureSwapChainInvalid;
		case vr::VRCompositorError_SharedTexturesNotSupported: return ovrError_TextureSwapChainInvalid;
		case vr::VRCompositorError_IndexOutOfRange: return ovrError_InvalidParameter;
		default: return ovrError_RuntimeException;
	}
}
