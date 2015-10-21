#include "OVR_CAPI.h"

#include <iostream>
#include <fstream>

#define LOG_RESULT ovr_GetLastErrorInfo(&info); \
	if (info.Result) { f << info.ErrorString << std::endl; return -1; }

//#define LOG_RETURN(x) f << #x << ": " << (x) << std::endl; LOG_RESULT

std::ostream& operator << (std::ostream& os, ovrQuatf& quat)
{
	os << "x: " << quat.x;
	os << " y: " << quat.y;
	os << " z: " << quat.z;
	os << " w: " << quat.w;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrVector3f& vec)
{
	os << "x: " << vec.x;
	os << " y: " << vec.y;
	os << " z: " << vec.z;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrVector2f& vec)
{
	os << "x: " << vec.x;
	os << " y: " << vec.y;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrVector2i& vec)
{
	os << "x: " << vec.x;
	os << " y: " << vec.y;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrSizei& size)
{
	os << "w: " << size.w;
	os << " h: " << size.h;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrRecti& rect)
{
	os << rect.Pos << " " << rect.Size;
	return os;
}

std::ostream& operator << (std::ostream& os, const ovrGraphicsLuid& luid)
{
	os << "\tluid: ";
	for (int i = 0; i < 8; i++)
		os << luid.Reserved[i];
	return os;

}

std::ostream& operator << (std::ostream& os, const ovrFovPort& fov)
{
	os << "UpTan: " << fov.UpTan;
	os << " DownTan: " << fov.DownTan;
	os << " LeftTan: " << fov.LeftTan;
	os << " RightTan: " << fov.RightTan;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrHmdDesc& desc)
{
	os << "\tType: " << desc.Type << std::endl;
	os << "\tProductName: " << desc.ProductName << std::endl;
	os << "\tManufacturer: " << desc.Manufacturer << std::endl;
	os << "\tVendorId: " << desc.VendorId << std::endl;
	os << "\tProductId: " << desc.ProductId << std::endl;
	os << "\tSerialNumber: " << desc.SerialNumber << std::endl;
	os << "\tFirmwareMajor: " << desc.FirmwareMajor << std::endl;
	os << "\tFirmwareMinor: " << desc.FirmwareMinor << std::endl;
	os << "\tAvailableHmdCaps: " << desc.AvailableHmdCaps << std::endl;
	os << "\tDefaultHmdCaps: " << desc.DefaultHmdCaps << std::endl;
	os << "\tAvailableTrackingCaps: " << desc.AvailableTrackingCaps << std::endl;
	os << "\tDefaultTrackingCaps: " << desc.DefaultTrackingCaps << std::endl;
	os << "\tDefaultEyeFov[ovrEye_Left]: " << desc.DefaultEyeFov[ovrEye_Left] << std::endl;
	os << "\tDefaultEyeFov[ovrEye_Right]: " << desc.DefaultEyeFov[ovrEye_Right] << std::endl;
	os << "\tMaxEyeFov[ovrEye_Left]: " << desc.MaxEyeFov[ovrEye_Left] << std::endl;
	os << "\tMaxEyeFov[ovrEye_Right]: " << desc.MaxEyeFov[ovrEye_Right] << std::endl;
	os << "\tResolution: " << desc.Resolution << std::endl;
	os << "\tDisplayRefreshRate: " << desc.DisplayRefreshRate;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrTrackerDesc& desc)
{
	os << "\t\tFrustumHFovInRadians: " << desc.FrustumHFovInRadians << std::endl;
	os << "\t\tFrustumVFovInRadians: " << desc.FrustumVFovInRadians << std::endl;
	os << "\t\tFrustumNearZInMeters: " << desc.FrustumNearZInMeters << std::endl;
	os << "\t\tFrustumFarZInMeters: " << desc.FrustumFarZInMeters;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrSessionStatus& status)
{
	os << "\tIsVisible: " << !!status.IsVisible << std::endl;
	os << "\tHmdPresent: " << !!status.HmdPresent << std::endl;
	os << "\tHmdMounted: " << !!status.HmdMounted << std::endl;
	os << "\tDisplayLost: " << !!status.DisplayLost << std::endl;
	os << "\tShouldQuit: " << !!status.ShouldQuit << std::endl;
	os << "\tShouldRecenter: " << !!status.ShouldRecenter;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrPosef& pose)
{
	os << "\t\t\tOrientation: " << pose.Orientation << std::endl;
	os << "\t\t\tPosition: " << pose.Position;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrPoseStatef& pose)
{
	os << "\t\tThePose:" << std::endl << pose.ThePose << std::endl;
	os << "\t\tAngularVelocity: " << pose.AngularVelocity << std::endl;
	os << "\t\tLinearVelocity: " << pose.LinearVelocity << std::endl;
	os << "\t\tAngularAcceleration: " << pose.AngularAcceleration << std::endl;
	os << "\t\tLinearAcceleration: " << pose.LinearAcceleration << std::endl;
	os << "\t\tTimeInSeconds: " << pose.TimeInSeconds;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrTrackingState& state)
{
	os << "\tHeadPose:" << std::endl << state.HeadPose << std::endl;
	os << "\tStatusFlags: " << state.StatusFlags << std::endl;
	os << "\tHandPoses[ovrHand_Left]:" << std::endl << state.HandPoses[ovrHand_Left] << std::endl;
	os << "\tHandPoses[ovrHand_Right]:" << std::endl << state.HandPoses[ovrHand_Right];
	return os;
}

std::ostream& operator << (std::ostream& os, ovrTrackerPose& pose)
{
	os << "\t\tTrackerFlags: " << pose.TrackerFlags << std::endl;
	os << "\t\tPose: " << std::endl << pose.Pose << std::endl;
	os << "\t\tLeveledPose: " << std::endl << pose.LeveledPose;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrInputState& state)
{
	os << "\tTimeInSeconds: " << state.TimeInSeconds << std::endl;
	os << "\tButtons: " << state.Buttons << std::endl;
	os << "\tTouches: " << state.Touches << std::endl;
	os << "\tIndexTrigger[ovrHand_Left]: " << state.IndexTrigger[ovrHand_Left] << std::endl;
	os << "\tIndexTrigger[ovrHand_Right]: " << state.IndexTrigger[ovrHand_Right] << std::endl;
	os << "\tHandTrigger[ovrHand_Left]: " << state.HandTrigger[ovrHand_Left] << std::endl;
	os << "\tHandTrigger[ovrHand_Right]: " << state.HandTrigger[ovrHand_Right] << std::endl;
	os << "\tThumbstick[ovrHand_Left]: " << state.Thumbstick[ovrHand_Left] << std::endl;
	os << "\tThumbstick[ovrHand_Right]: " << state.Thumbstick[ovrHand_Right] << std::endl;
	os << "\tControllerType: " << state.ControllerType;
	return os;
}

std::ostream& operator << (std::ostream& os, ovrEyeRenderDesc& desc)
{
	os << "\tEye: " << desc.Eye << std::endl;
	os << "\tFov: " << desc.Fov << std::endl;
	os << "\tDistortedViewport: " << desc.DistortedViewport << std::endl;
	os << "\tPixelsPerTanAngleAtCenter: " << desc.PixelsPerTanAngleAtCenter << std::endl;
	os << "\tHmdToEyeOffset: " << desc.HmdToEyeOffset;
	return os;
}

int main()
{
	ovrErrorInfo info;
	std::ofstream f;
	f.open("LibOVRDump.txt");

	ovr_Initialize(nullptr);
	LOG_RESULT;

	ovrSession session;
	ovrGraphicsLuid luid;
	ovr_Create(&session, &luid);
	LOG_RESULT;
	f << "ovr_Create:" << std::endl << luid << std::endl;

	ovrHmdDesc desc = ovr_GetHmdDesc(session);
	LOG_RESULT;
	f << "ovr_GetHmdDesc:" << std::endl << desc << std::endl;

	unsigned int count = ovr_GetTrackerCount(session);
	LOG_RESULT;
	f << "ovr_GetTrackerCount: " << count << std::endl;

	for (unsigned int i = 0; i < count; i++)
	{
		ovrTrackerDesc tracker = ovr_GetTrackerDesc(session, i);
		LOG_RESULT;
		f << "ovr_GetTrackerDesc: " << std::endl;
		f << "\tIndex: " << i << std::endl;
		f << tracker << std::endl;
	}

	ovrSessionStatus status;
	ovr_GetSessionStatus(session, &status);
	f << "ovr_GetSessionStatus: " << std::endl << status << std::endl;
	LOG_RESULT;

	ovrTrackingOrigin origin = ovr_GetTrackingOriginType(session);
	LOG_RESULT;
	f << "ovr_GetTrackingOriginType: " << origin << std::endl;

	ovrTrackingState state = ovr_GetTrackingState(session, 0.0, ovrFalse);
	LOG_RESULT;
	f << "ovr_GetTrackingState: " << std::endl << state << std::endl;

	for (unsigned int i = 0; i < count; i++)
	{
		ovrTrackerPose pose = ovr_GetTrackerPose(session, i);
		LOG_RESULT;
		f << "ovr_GetTrackerPose: " << std::endl;
		f << "\tIndex: " << i << std::endl;
		f << pose << std::endl;
	}

	ovrControllerType controllers[7] = { ovrControllerType_None,
		ovrControllerType_LTouch, ovrControllerType_RTouch,
		ovrControllerType_Touch, ovrControllerType_Remote,
		ovrControllerType_XBox, ovrControllerType_Active };
	for (int i = 0; i < 7; i++)
	{
		ovrInputState input;
		ovr_GetInputState(session, controllers[i], &input);
		LOG_RESULT;
		f << "ovr_GetInputState: " << std::endl;
		f << input << std::endl;
	}

	unsigned int types = ovr_GetConnectedControllerTypes(session);
	LOG_RESULT;
	f << "ovr_GetConnectedControllerTypes: " << types << std::endl;

	ovrSizei size;
	size = ovr_GetFovTextureSize(session, ovrEye_Left, desc.DefaultEyeFov[ovrEye_Left], 1.0);
	LOG_RESULT;
	f << "ovr_GetFovTextureSize(DefaultEyeFov[ovrEye_Left]): " << size << std::endl;
	size = ovr_GetFovTextureSize(session, ovrEye_Right, desc.DefaultEyeFov[ovrEye_Right], 1.0);
	LOG_RESULT;
	f << "ovr_GetFovTextureSize(DefaultEyeFov[ovrEye_Right]): " << size << std::endl;
	size = ovr_GetFovTextureSize(session, ovrEye_Left, desc.MaxEyeFov[ovrEye_Left], 1.0);
	LOG_RESULT;
	f << "ovr_GetFovTextureSize(MaxEyeFov[ovrEye_Left]): " << size << std::endl;
	size = ovr_GetFovTextureSize(session, ovrEye_Right, desc.MaxEyeFov[ovrEye_Right], 1.0);
	LOG_RESULT;
	f << "ovr_GetFovTextureSize(MaxEyeFov[ovrEye_Right]): " << size << std::endl;

	ovrEyeRenderDesc render;
	render = ovr_GetRenderDesc(session, ovrEye_Left, desc.DefaultEyeFov[ovrEye_Left]);
	LOG_RESULT;
	f << "ovr_GetRenderDesc(DefaultEyeFov[ovrEye_Left]): " << std::endl << render << std::endl;
	render = ovr_GetRenderDesc(session, ovrEye_Right, desc.DefaultEyeFov[ovrEye_Right]);
	LOG_RESULT;
	f << "ovr_GetRenderDesc(DefaultEyeFov[ovrEye_Right]): " << std::endl << render << std::endl;
	render = ovr_GetRenderDesc(session, ovrEye_Left, desc.MaxEyeFov[ovrEye_Left]);
	LOG_RESULT;
	f << "ovr_GetRenderDesc(MaxEyeFov[ovrEye_Left]): " << std::endl << render << std::endl;
	render = ovr_GetRenderDesc(session, ovrEye_Right, desc.MaxEyeFov[ovrEye_Right]);
	LOG_RESULT;
	f << "ovr_GetRenderDesc(MaxEyeFov[ovrEye_Right]): " << std::endl << render << std::endl;

	double displayTime = ovr_GetPredictedDisplayTime(session, 0);
	LOG_RESULT;
	f << "ovr_GetPredictedDisplayTime: " << displayTime << std::endl;

	double time = ovr_GetTimeInSeconds();
	LOG_RESULT;
	f << "ovr_GetTimeInSeconds: " << time << std::endl;

	return 0;
}

