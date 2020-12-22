#include "OVR_CAPI.h"
#include "XR_Math.h"
#include "Common.h"
#include "Session.h"
#include "InputManager.h"

#include <thread>

ovrResult ovrHmdStruct::BeginSession(void* graphicsBinding)
{
	XrSessionCreateInfo createInfo = XR_TYPE(SESSION_CREATE_INFO);
	createInfo.next = graphicsBinding;
	createInfo.systemId = System;
	CHK_XR(xrCreateSession(Instance, &createInfo, &Session));

	// Attach it to the InputManager
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

	CHK_OVR(ovr_WaitToBeginFrame(this, 0));
	CHK_OVR(ovr_BeginFrame(this, 0));
}

ovrResult ovrHmdStruct::EndSession()
{
	if (!Session)
		return ovrError_InvalidSession;

	CHK_XR(xrRequestExitSession(Session));
	while (!XR_SUCCEEDED(xrEndSession(Session)))
		std::this_thread::sleep_for(std::chrono::milliseconds(100));

	CHK_XR(xrDestroySession(Session));
	Session = XR_NULL_HANDLE;
}
