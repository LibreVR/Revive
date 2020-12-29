#include "Common.h"

#include <Windows.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

extern XrInstance g_Instance;

XrResult g_LastResult = XR_SUCCESS;

ovrResult ResultToOvrResult(XrResult error)
{
	switch (error)
	{
	case XR_SUCCESS: return ovrSuccess;
	case XR_TIMEOUT_EXPIRED: return ovrError_Timeout;
	case XR_SESSION_LOSS_PENDING: return ovrSuccess;
	case XR_EVENT_UNAVAILABLE: return ovrSuccess;
	case XR_SPACE_BOUNDS_UNAVAILABLE: return ovrSuccess_BoundaryInvalid;
	case XR_SESSION_NOT_FOCUSED: return ovrSuccess_NotVisible;
	case XR_FRAME_DISCARDED: return ovrSuccess_NotVisible;
	case XR_ERROR_VALIDATION_FAILURE: return ovrError_InvalidOperation;
	case XR_ERROR_RUNTIME_FAILURE: return ovrError_DisplayLost;
	case XR_ERROR_OUT_OF_MEMORY: return ovrError_MemoryAllocationFailure;
	case XR_ERROR_API_VERSION_UNSUPPORTED: return ovrError_ServiceVersion;
	case XR_ERROR_INITIALIZATION_FAILED: return ovrError_Initialize;
	case XR_ERROR_FUNCTION_UNSUPPORTED: return ovrError_Unsupported;
	case XR_ERROR_FEATURE_UNSUPPORTED: return ovrError_Unsupported;
	case XR_ERROR_EXTENSION_NOT_PRESENT: return ovrError_Unsupported;
	case XR_ERROR_LIMIT_REACHED: return ovrError_DisplayLimitReached;
	case XR_ERROR_SIZE_INSUFFICIENT: return ovrError_InsufficientArraySize;
	case XR_ERROR_HANDLE_INVALID: return ovrError_InvalidOperation;
	case XR_ERROR_INSTANCE_LOST: return ovrError_DisplayRemoved;
	case XR_ERROR_SESSION_RUNNING: return ovrError_InvalidOperation;
	case XR_ERROR_SESSION_NOT_RUNNING: return ovrError_InvalidOperation;
	case XR_ERROR_SESSION_LOST: return ovrError_ServiceConnection;
	case XR_ERROR_SYSTEM_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_PATH_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_PATH_COUNT_EXCEEDED: return ovrError_InvalidOperation;
	case XR_ERROR_PATH_FORMAT_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_PATH_UNSUPPORTED: return ovrError_InvalidParameter;
	case XR_ERROR_LAYER_INVALID: return ovrError_TextureSwapChainInvalid;
	case XR_ERROR_LAYER_LIMIT_EXCEEDED: return ovrError_InvalidOperation;
	case XR_ERROR_SWAPCHAIN_RECT_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED: return ovrError_InvalidParameter;
	case XR_ERROR_ACTION_TYPE_MISMATCH: return ovrError_InvalidParameter;
	case XR_ERROR_SESSION_NOT_READY: return ovrError_RuntimeException;
	case XR_ERROR_SESSION_NOT_STOPPING: return ovrError_RuntimeException;
	case XR_ERROR_TIME_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_REFERENCE_SPACE_UNSUPPORTED: return ovrError_InvalidParameter;
	case XR_ERROR_FILE_ACCESS_ERROR: return ovrError_RuntimeException;
	case XR_ERROR_FILE_CONTENTS_INVALID: return ovrError_RuntimeException;
	case XR_ERROR_FORM_FACTOR_UNSUPPORTED: return ovrError_NoHmd;
	case XR_ERROR_FORM_FACTOR_UNAVAILABLE: return ovrError_DeviceUnavailable;
	case XR_ERROR_API_LAYER_NOT_PRESENT: return ovrError_RuntimeException;
	case XR_ERROR_CALL_ORDER_INVALID: return ovrError_TextureSwapChainFull;
	case XR_ERROR_GRAPHICS_DEVICE_INVALID: return ovrError_MismatchedAdapters;
	case XR_ERROR_POSE_INVALID: return ovrError_InvalidHeadsetOrientation;
	case XR_ERROR_INDEX_OUT_OF_RANGE: return ovrError_InvalidParameter;
	case XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED: return ovrError_InvalidParameter;
	case XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED: return ovrError_InvalidParameter;
	case XR_ERROR_NAME_DUPLICATED: return ovrError_InvalidParameter;
	case XR_ERROR_NAME_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_ACTIONSET_NOT_ATTACHED: return ovrError_InvalidParameter;
	case XR_ERROR_ACTIONSETS_ALREADY_ATTACHED: return ovrError_InvalidParameter;
	case XR_ERROR_LOCALIZED_NAME_DUPLICATED: return ovrError_InvalidParameter;
	case XR_ERROR_LOCALIZED_NAME_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING: return ovrError_NotInitialized;
	default: return ovrError_RuntimeException;
	}
}

XrTime AbsTimeToXrTime(XrInstance instance, double absTime)
{
	XR_FUNCTION(instance, ConvertWin32PerformanceCounterToTimeKHR);

	// Get back the XrTime
	static double PerfFrequency = 0.0;
	if (PerfFrequency == 0.0)
	{
		LARGE_INTEGER freq;
		QueryPerformanceFrequency(&freq);
		PerfFrequency = (double)freq.QuadPart;
	}

	XrResult rs;
	XrTime time;
	LARGE_INTEGER li;
	li.QuadPart = (LONGLONG)(absTime * PerfFrequency);
	rs = ConvertWin32PerformanceCounterToTimeKHR(instance, &li, &time);
	assert(XR_SUCCEEDED(rs));
	return time;
}

XrPath GetXrPath(const char* path)
{
	XrPath outPath;
	XrResult rs = xrStringToPath(g_Instance, path, &outPath);
	assert(XR_SUCCEEDED(rs));
	if (XR_FAILED(rs))
		return XR_NULL_PATH;
	return outPath;
}

XrPath GetXrPath(std::string path)
{
	return GetXrPath(path.c_str());
}
