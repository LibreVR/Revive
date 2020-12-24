#pragma once

#include "OVR_CAPI.h"
#include "microprofile.h"

#include <openxr/openxr.h>
#include <assert.h>
#include <string>

#if 0
#include <Windows.h>
#define REV_TRACE(x) OutputDebugStringA("Revive: " #x "\n");
#else
#define REV_TRACE(x) MICROPROFILE_SCOPEI("Revive", #x, 0xff0000);
#endif

extern XrResult g_LastResult;

#define CHK_XR(x) \
	{ \
		g_LastResult = (x); \
		assert(XR_SUCCEEDED(g_LastResult)); \
		if (XR_FAILED(g_LastResult)) return ResultToOvrResult(g_LastResult); \
	}

#define CHK_OVR(x) \
	{ \
		ovrResult g_Result = (x); \
		assert(OVR_SUCCESS(g_Result)); \
		if (OVR_FAILURE(g_Result)) return g_Result; \
	}

#define XR_TYPE(x) { XR_TYPE_##x, nullptr }

#define XR_FUNCTION(instance, func) \
	static PFN_xr##func func = nullptr; \
	if (!func) \
		CHK_XR(xrGetInstanceProcAddr(instance, "xr" #func, (PFN_xrVoidFunction*)&##func));

ovrResult ResultToOvrResult(XrResult error);
XrTime AbsTimeToXrTime(XrInstance instance, double absTime);
XrPath GetXrPath(const char* path);
XrPath GetXrPath(std::string path);
