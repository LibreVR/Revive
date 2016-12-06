#include "Common.h"
#include "Extras/OVR_Math.h"

#include <stdio.h>

// Common structures

ovrTextureSwapChainData::ovrTextureSwapChainData(vr::EGraphicsAPIConvention api, ovrTextureSwapChainDesc desc)
	: ApiType(api)
	, Length(REV_SWAPCHAIN_LENGTH)
	, CurrentIndex(0)
	, Desc(desc)
	, Overlay(vr::k_ulOverlayHandleInvalid)
{
	memset(Textures, 0, sizeof(Textures));
}

ovrMirrorTextureData::ovrMirrorTextureData(vr::EGraphicsAPIConvention api, ovrMirrorTextureDesc desc)
	: ApiType(api)
	, Desc(desc)
{
}

ovrHmdStruct::ovrHmdStruct()
	: ShouldQuit(false)
	, IsVisible(false)
	, WaitThreadId(0)
	, FrameIndex(0)
	, StatsIndex(0)
	, PixelsPerDisplayPixel(0.0f)
	, Compositor(nullptr)
	, Input(new InputManager())
{
	memset(StringBuffer, 0, sizeof(StringBuffer));
	memset(&ResetStats, 0, sizeof(ResetStats));
	memset(Stats, 0, sizeof(Stats));
	memset(TouchOffset, 0, sizeof(TouchOffset));
}

ovrHmdStruct::~ovrHmdStruct()
{
	delete Input;
	delete Compositor;
}

// Common functions

unsigned int rev_TrackedDevicePoseToOVRStatusFlags(vr::TrackedDevicePose_t pose)
{
	unsigned int result = 0;

	if (pose.bPoseIsValid)
	{
		if (pose.bDeviceIsConnected)
			result |= ovrStatus_OrientationTracked;
		if (pose.eTrackingResult != vr::TrackingResult_Calibrating_OutOfRange &&
			pose.eTrackingResult != vr::TrackingResult_Running_OutOfRange)
			result |= ovrStatus_PositionTracked;
	}

	return result;
}

OVR::Matrix4f rev_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m)
{
	OVR::Matrix4f r;
	memcpy(r.M, m.m, sizeof(vr::HmdMatrix34_t));
	return r;
}

OVR::Vector3f rev_HmdVectorToOVRVector(vr::HmdVector3_t v)
{
	OVR::Vector3f r;
	r.x = v.v[0];
	r.y = v.v[1];
	r.z = v.v[2];
	return r;
}

vr::HmdMatrix34_t rev_OvrPoseToHmdMatrix(ovrPosef pose)
{
	vr::HmdMatrix34_t result;
	OVR::Matrix4f matrix(pose);
	memcpy(result.m, matrix.M, sizeof(result.m));
	return result;
}

ovrPoseStatef rev_TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time)
{
	ovrPoseStatef result = { 0 };
	result.ThePose = OVR::Posef::Identity();

	OVR::Matrix4f matrix;
	if (pose.bPoseIsValid)
		matrix = rev_HmdMatrixToOVRMatrix(pose.mDeviceToAbsoluteTracking);
	else
		return result;

	result.ThePose.Orientation = OVR::Quatf(matrix);
	result.ThePose.Position = matrix.GetTranslation();
	result.AngularVelocity = rev_HmdVectorToOVRVector(pose.vAngularVelocity);
	result.LinearVelocity = rev_HmdVectorToOVRVector(pose.vVelocity);
	// TODO: Calculate acceleration.
	result.AngularAcceleration = ovrVector3f();
	result.LinearAcceleration = ovrVector3f();
	result.TimeInSeconds = time;

	return result;
}

vr::VRTextureBounds_t rev_FovPortToTextureBounds(ovrEyeType eye, ovrFovPort fov)
{
	vr::VRTextureBounds_t result;

	// Get the headset field-of-view
	float left, right, top, bottom;
	vr::VRSystem()->GetProjectionRaw((vr::EVREye)eye, &left, &right, &top, &bottom);

	// Adjust the bounds based on the field-of-view in the game
	result.uMin = 0.5f + 0.5f * left / fov.LeftTan;
	result.uMax = 0.5f + 0.5f * right / fov.RightTan;
	result.vMin = 0.5f - 0.5f * bottom / fov.UpTan;
	result.vMax = 0.5f - 0.5f * top / fov.DownTan;

	// Sanitize the output
	if (result.uMin < 0.0)
		result.uMin = 0.0;
	if (result.uMax > 1.0)
		result.uMax = 1.0;

	if (result.vMin < 0.0)
		result.vMin = 0.0;
	if (result.vMax > 1.0)
		result.vMax = 1.0;

	return result;
}
