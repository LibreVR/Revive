#include "OVR_CAPI_Prototypes.h"

#undef OVR_LIST_INTERNAL_APIS // We need to define the actual internal APIs
#define OVR_LIST_INTERNAL_APIS(_,X) \
X(unsigned int, ovr_GetEnabledCaps, , (ovrSession session)) \
X(void, ovr_SetEnabledCaps, , (ovrSession session, unsigned int hmdCaps)) \
X(unsigned int, ovr_GetTrackingCaps, , (ovrSession session)) \
_(ovrResult, ovr_Lookup, , (const char* name, void** data)) \
_(ovrResult, ovr_ConfigureTracking, , (ovrSession session, unsigned int requestedTrackingCaps, unsigned int requiredTrackingCaps)) \
_(ovrResult, ovr_GetHmdColorDesc, , ()) \
_(ovrResult, ovr_QueryDistortion, , ()) \
_(ovrResult, ovr_SetClientColorDesc, , ())

#define REV_STUB_EXPORT(ReturnValue, FunctionName, OptionalVersion, Arguments) \
OVR_PUBLIC_FUNCTION(ReturnValue) \
FunctionName Arguments \
{ return ReturnValue{ ovrError_Unsupported }; }

#define REV_STUB_VOID(ReturnValue, FunctionName, OptionalVersion, Arguments) \
OVR_PUBLIC_FUNCTION(ReturnValue) \
FunctionName Arguments \
{ return ReturnValue(); }

OVR_LIST_PRIVATE_APIS(REV_STUB_EXPORT, REV_STUB_VOID)
OVR_LIST_INTERNAL_APIS(REV_STUB_EXPORT, REV_STUB_VOID)
