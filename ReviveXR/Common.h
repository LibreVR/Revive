#pragma once

#include "OVR_CAPI.h"
#include "microprofile.h"

#include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>
#include <assert.h>
#include <string>

#if 0
#include <Windows.h>
#define REV_TRACE(x) OutputDebugStringA("Revive: " #x "\n");
#else
#define REV_TRACE(x) MICROPROFILE_SCOPEI("Revive", #x, 0xff0000);
#endif

#define XR_ENUM_CASE_STR(name, val) case name: return L#name;
constexpr const wchar_t* ResultToString(XrResult e)
{
	switch (e)
	{
		XR_LIST_ENUM_XrResult(XR_ENUM_CASE_STR)
		default: return L"Unknown";
	}
}

extern XrResult g_LastResult;

#ifdef NDEBUG
#define assertmsg(expression, message) ((void)0)
#else
#define assertmsg(expression, message) (void)(                                                       \
            (!!(expression)) ||                                                              \
            (_wassert(message, _CRT_WIDE(__FILE__), (unsigned)(__LINE__)), 0) \
        )
#endif

#define CHK_XR(x) \
	{ \
		g_LastResult = (x); \
		assertmsg(XR_SUCCEEDED(g_LastResult), ResultToString(g_LastResult)); \
		if (XR_FAILED(g_LastResult)) return ResultToOvrResult(g_LastResult); \
	}

#define CHK_OVR(x) \
	{ \
		ovrResult __LastResult = (x); \
		assert(OVR_SUCCESS(__LastResult)); \
		if (OVR_FAILURE(__LastResult)) return __LastResult; \
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
