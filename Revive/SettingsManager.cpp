#include "SettingsManager.h"
#include "Settings.h"
#include "REV_Math.h"

#include <OVR_CAPI.h>
#include <thread>

SettingsManager::SettingsManager()
	: m_bRunning(true)
{
	LoadSettings();

	m_thread = std::thread(SettingsThread, this);
}

SettingsManager::~SettingsManager()
{
	m_bRunning = false;
	if (m_thread.joinable())
		m_thread.join();
}

void SettingsManager::LoadSettings()
{
	Deadzone = ovr_GetFloat(nullptr, REV_KEY_THUMB_DEADZONE, REV_DEFAULT_THUMB_DEADZONE);
	Sensitivity = ovr_GetFloat(nullptr, REV_KEY_THUMB_SENSITIVITY, REV_DEFAULT_THUMB_SENSITIVITY);
	ToggleGrip = (revGripType)ovr_GetInt(nullptr, REV_KEY_TOGGLE_GRIP, REV_DEFAULT_TOGGLE_GRIP);
	TriggerAsGrip = !!ovr_GetBool(nullptr, REV_KEY_TRIGGER_GRIP, REV_DEFAULT_TRIGGER_GRIP);
	ToggleDelay = ovr_GetFloat(nullptr, REV_KEY_TOGGLE_DELAY, REV_DEFAULT_TOGGLE_DELAY);
	IgnoreActivity = !!ovr_GetBool(nullptr, REV_KEY_IGNORE_ACTIVITYLEVEL, REV_DEFAULT_IGNORE_ACTIVITYLEVEL);

	OVR::Vector3f angles(
		OVR::DegreeToRad(ovr_GetFloat(nullptr, REV_KEY_TOUCH_PITCH, REV_DEFAULT_TOUCH_PITCH)),
		OVR::DegreeToRad(ovr_GetFloat(nullptr, REV_KEY_TOUCH_YAW, REV_DEFAULT_TOUCH_YAW)),
		OVR::DegreeToRad(ovr_GetFloat(nullptr, REV_KEY_TOUCH_ROLL, REV_DEFAULT_TOUCH_ROLL))
	);
	OVR::Vector3f offset(
		ovr_GetFloat(nullptr, REV_KEY_TOUCH_X, REV_DEFAULT_TOUCH_X),
		ovr_GetFloat(nullptr, REV_KEY_TOUCH_Y, REV_DEFAULT_TOUCH_Y),
		ovr_GetFloat(nullptr, REV_KEY_TOUCH_Z, REV_DEFAULT_TOUCH_Z)
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

void SettingsManager::SettingsThread(SettingsManager* manager)
{
	// Only refresh the settings once per second
	std::chrono::microseconds freq(std::chrono::seconds(1));

	while (manager->m_bRunning)
	{
		manager->LoadSettings();
		std::this_thread::sleep_for(freq);
	}
}
