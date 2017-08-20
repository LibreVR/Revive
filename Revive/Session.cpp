#include "Session.h"
#include "REV_Math.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "Settings.h"

ovrHmdStruct::ovrHmdStruct()
	: ShouldQuit(false)
	, IsVisible(false)
	, FrameIndex(0)
	, StatsIndex(0)
	, NextLoadTime(0.0)
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
{
	memset(StringBuffer, 0, sizeof(StringBuffer));
	memset(&ResetStats, 0, sizeof(ResetStats));
	memset(Stats, 0, sizeof(Stats));
	memset(TouchOffset, 0, sizeof(TouchOffset));

	// Get the render target multiplier
	PixelsPerDisplayPixel = ovr_GetFloat(this, REV_KEY_PIXELS_PER_DISPLAY, REV_DEFAULT_PIXELS_PER_DISPLAY);

	// Get the default universe origin from the settings
	TrackingOrigin = (vr::ETrackingUniverseOrigin)ovr_GetInt(nullptr, REV_KEY_DEFAULT_ORIGIN, REV_DEFAULT_ORIGIN);

	LoadSettings();
}

void ovrHmdStruct::LoadSettings()
{
	// Only refresh the settings once per second
	double absTime = ovr_GetTimeInSeconds();
	if (absTime < NextLoadTime)
		return;
	NextLoadTime = absTime + 1.0;

	Deadzone = ovr_GetFloat(this, REV_KEY_THUMB_DEADZONE, REV_DEFAULT_THUMB_DEADZONE);
	Sensitivity = ovr_GetFloat(this, REV_KEY_THUMB_SENSITIVITY, REV_DEFAULT_THUMB_SENSITIVITY);
	ToggleGrip = (revGripType)ovr_GetInt(this, REV_KEY_TOGGLE_GRIP, REV_DEFAULT_TOGGLE_GRIP);
	TriggerAsGrip = !!ovr_GetBool(this, REV_KEY_TRIGGER_GRIP, REV_DEFAULT_TRIGGER_GRIP);
	ToggleDelay = ovr_GetFloat(this, REV_KEY_TOGGLE_DELAY, REV_DEFAULT_TOGGLE_DELAY);
	IgnoreActivity = !!ovr_GetBool(this, REV_KEY_IGNORE_ACTIVITYLEVEL, REV_DEFAULT_IGNORE_ACTIVITYLEVEL);

	OVR::Vector3f angles(
		OVR::DegreeToRad(ovr_GetFloat(this, REV_KEY_TOUCH_PITCH, REV_DEFAULT_TOUCH_PITCH)),
		OVR::DegreeToRad(ovr_GetFloat(this, REV_KEY_TOUCH_YAW, REV_DEFAULT_TOUCH_YAW)),
		OVR::DegreeToRad(ovr_GetFloat(this, REV_KEY_TOUCH_ROLL, REV_DEFAULT_TOUCH_ROLL))
	);
	OVR::Vector3f offset(
		ovr_GetFloat(this, REV_KEY_TOUCH_X, REV_DEFAULT_TOUCH_X),
		ovr_GetFloat(this, REV_KEY_TOUCH_Y, REV_DEFAULT_TOUCH_Y),
		ovr_GetFloat(this, REV_KEY_TOUCH_Z, REV_DEFAULT_TOUCH_Z)
	);

	// Check if the offset matrix needs to be updated
	if (angles != RotationOffset || offset != PositionOffset)
	{
		RotationOffset = angles;
		PositionOffset = offset;

		for (int i = 0; i < ovrHand_Count; i++)
		{
			OVR::Matrix4f yaw = OVR::Matrix4f::RotationY(angles.y);
			OVR::Matrix4f pitch = OVR::Matrix4f::RotationX(angles.x);
			OVR::Matrix4f roll = OVR::Matrix4f::RotationZ(angles.z);

			// Mirror the right touch controller offsets
			if (i == ovrHand_Right)
			{
				yaw.Invert();
				roll.Invert();
				offset.x *= -1.0f;
			}

			OVR::Matrix4f matrix(yaw * pitch * roll);
			matrix.SetTranslation(offset);
			memcpy(TouchOffset[i].m, matrix.M, sizeof(vr::HmdMatrix34_t));
		}
	}
}
