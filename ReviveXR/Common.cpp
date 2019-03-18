#include "Common.h"

#include <openxr/openxr.h>

XrResult g_LastResult = XR_SUCCESS;

ovrResult ResultToOvrResult(XrResult error)
{
	switch (error)
	{
	case XR_SUCCESS: return ovrSuccess;
	case XR_TIMEOUT_EXPIRED: return ovrError_Timeout;
	case XR_SESSION_VISIBILITY_UNAVAILABLE: return ovrSuccess_NotVisible;
	case XR_SESSION_LOSS_PENDING: return ovrSuccess;
	case XR_EVENT_UNAVAILABLE: return ovrSuccess;
	case XR_STATE_UNAVAILABLE: return ovrSuccess;
	case XR_STATE_TYPE_UNAVAILABLE: return ovrSuccess;
	case XR_SPACE_BOUNDS_UNAVAILABLE: return ovrSuccess_BoundaryInvalid;
	case XR_SESSION_NOT_FOCUSED: return ovrSuccess_NotVisible;
	case XR_FRAME_DISCARDED: return ovrSuccess_NotVisible;
	case XR_ERROR_VALIDATION_FAILURE: return ovrError_InvalidParameter;
	case XR_ERROR_RUNTIME_FAILURE: return ovrError_RuntimeException;
	case XR_ERROR_OUT_OF_MEMORY: return ovrError_MemoryAllocationFailure;
	case XR_ERROR_RUNTIME_VERSION_INCOMPATIBLE: return ovrError_LibVersion;
	case XR_ERROR_DRIVER_INCOMPATIBLE: return ovrError_ServiceVersion;
	case XR_ERROR_INITIALIZATION_FAILED: return ovrError_Initialize;
	case XR_ERROR_FUNCTION_UNSUPPORTED: return ovrError_Unsupported;
	case XR_ERROR_FEATURE_UNSUPPORTED: return ovrError_Unsupported;
	case XR_ERROR_EXTENSION_NOT_PRESENT: return ovrError_Unsupported;
	case XR_ERROR_LIMIT_REACHED: return ovrError_DisplayLimitReached;
	case XR_ERROR_SIZE_INSUFFICIENT: return ovrError_InsufficientArraySize;
	case XR_ERROR_HANDLE_INVALID: return ovrError_InvalidSession;
	case XR_ERROR_INSTANCE_LOST: return ovrError_DisplayRemoved;
	case XR_ERROR_SESSION_RUNNING: return ovrError_InvalidSession;
	case XR_ERROR_SESSION_NOT_RUNNING: return ovrError_InvalidSession;
	case XR_ERROR_SESSION_LOST: return ovrError_DisplayLost;
	case XR_ERROR_SYSTEM_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_PATH_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_PATH_COUNT_EXCEEDED: return ovrError_InvalidParameter;
	case XR_ERROR_PATH_FORMAT_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_LAYER_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_LAYER_LIMIT_EXCEEDED: return ovrError_InvalidParameter;
	case XR_ERROR_SWAPCHAIN_RECT_INVALID: return ovrError_InvalidParameter;
	case XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED: return ovrError_InvalidParameter;
	case XR_ERROR_ACTION_TYPE_MISMATCH: return ovrError_InvalidParameter;
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
	case XR_ERROR_BINDINGS_DUPLICATED: return ovrError_InvalidParameter;
	case XR_ERROR_NAME_DUPLICATED: return ovrError_InvalidParameter;
	case XR_ERROR_NAME_INVALID: return ovrError_InvalidParameter;
	default: return ovrError_RuntimeException;
	}
}
