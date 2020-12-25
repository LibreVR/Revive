#include "OVR_CAPI.h"
#include "XR_Math.h"
#include "Common.h"
#include "Session.h"
#include "InputManager.h"

#include <thread>

ovrResult ovrHmdStruct::BeginSession(void* graphicsBinding, bool waitFrame)
{
	if (Session)
		return ovrError_InvalidOperation;

	XrSessionCreateInfo createInfo = XR_TYPE(SESSION_CREATE_INFO);
	createInfo.next = graphicsBinding;
	createInfo.systemId = System;
	CHK_XR(xrCreateSession(Instance, &createInfo, &Session));
	memset(&SessionStatus, 0, sizeof(SessionStatus));

	// Attach it to the InputManager
	if (Input)
		Input->AttachSession(Session);

	// Create reference spaces
	XrReferenceSpaceCreateInfo spaceInfo = XR_TYPE(REFERENCE_SPACE_CREATE_INFO);
	spaceInfo.poseInReferenceSpace = XR::Posef::Identity();
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
	CHK_XR(xrCreateReferenceSpace(Session, &spaceInfo, &ViewSpace));
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
	CHK_XR(xrCreateReferenceSpace(Session, &spaceInfo, &LocalSpace));
	spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
	CHK_XR(xrCreateReferenceSpace(Session, &spaceInfo, &StageSpace));
	CalibratedOrigin = OVR::Posef::Identity();

	XrSessionBeginInfo beginInfo = XR_TYPE(SESSION_BEGIN_INFO);
	beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	CHK_XR(xrBeginSession(Session, &beginInfo));

	if (waitFrame)
	{
		CHK_OVR(ovr_WaitToBeginFrame(this, 0));
		CHK_OVR(ovr_BeginFrame(this, 0));
	}
	return ovrSuccess;
}

ovrResult ovrHmdStruct::EndSession()
{
	if (!Session)
		return ovrError_InvalidOperation;

	if (Input)
		Input->AttachSession(XR_NULL_HANDLE);

	CHK_XR(xrDestroySession(Session));
	Session = XR_NULL_HANDLE;
	ViewSpace = XR_NULL_HANDLE;
	LocalSpace = XR_NULL_HANDLE;
	StageSpace = XR_NULL_HANDLE;
	return ovrSuccess;
}

ovrResult ovrHmdStruct::LocateViews(XrView out_Views[ovrEye_Count], XrViewStateFlags* out_Flags) const
{
	if (!Session)
	{
		// If the session is not fully initialized, return the cached values
		memcpy(out_Views, ViewPoses, sizeof(ViewPoses));
		if (out_Flags)
			*out_Flags = 0;
		return ovrSuccess;
	}

	uint32_t numViews;
	XrViewLocateInfo locateInfo = XR_TYPE(VIEW_LOCATE_INFO);
	XrViewState viewState = XR_TYPE(VIEW_STATE);
	locateInfo.space = ViewSpace;
	locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	locateInfo.displayTime = AbsTimeToXrTime(Instance, ovr_GetTimeInSeconds());
	CHK_OVR(xrLocateViews(Session, &locateInfo, &viewState, ovrEye_Count, &numViews, out_Views));
	assert(numViews == ovrEye_Count);
	if (out_Flags)
		*out_Flags = viewState.viewStateFlags;
	return ovrSuccess;
}
