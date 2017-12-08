#include "SettingsManager.h"
#include "Settings.h"
#include "REV_Math.h"

#include <Windows.h>
#include <OVR_CAPI.h>
#include <thread>

void SettingsManager::SettingsThreadFunc(SettingsManager* manager)
{
	HANDLE reload = OpenEventW(SYNCHRONIZE, FALSE, SESSION_RELOAD_EVENT_NAME);
	if (!reload)
		return;

	while (manager->Running)
	{
		// Check running state 100ms
		if (WaitForSingleObject(reload, 100) == WAIT_OBJECT_0)
			manager->ReloadSettings();
	}
}

SettingsManager::SettingsManager()
	: Running(true)
{
	SettingsThread = std::thread(SettingsThreadFunc, this);
	ReloadSettings();
}

SettingsManager::~SettingsManager()
{
	Running = false;
	if (SettingsThread.joinable())
		SettingsThread.join();
}

template<> float SettingsManager::Get<float>(const char* key, float defaultVal)
{
	vr::EVRSettingsError err;
	float result = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, key, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

template<> int SettingsManager::Get<int>(const char* key, int defaultVal)
{
	vr::EVRSettingsError err;
	int result = vr::VRSettings()->GetInt32(REV_SETTINGS_SECTION, key, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

template<> bool SettingsManager::Get<bool>(const char* key, bool defaultVal)
{
	vr::EVRSettingsError err;
	bool result = vr::VRSettings()->GetBool(REV_SETTINGS_SECTION, key, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

void SettingsManager::ReloadSettings()
{
	InputSettings s = {
		Get<float>(REV_KEY_THUMB_DEADZONE, REV_DEFAULT_THUMB_DEADZONE),
		(revGripType)Get<int>(REV_KEY_TOGGLE_GRIP, REV_DEFAULT_TOGGLE_GRIP),
		Get<bool>(REV_KEY_TRIGGER_GRIP, REV_DEFAULT_TRIGGER_GRIP),
		Get<float>(REV_KEY_TOGGLE_DELAY, REV_DEFAULT_TOGGLE_DELAY)
	};

	OVR::Vector3f angles(
		OVR::DegreeToRad(Get<float>(REV_KEY_TOUCH_PITCH, REV_DEFAULT_TOUCH_PITCH)),
		OVR::DegreeToRad(Get<float>(REV_KEY_TOUCH_YAW, REV_DEFAULT_TOUCH_YAW)),
		OVR::DegreeToRad(Get<float>(REV_KEY_TOUCH_ROLL, REV_DEFAULT_TOUCH_ROLL))
	);
	OVR::Vector3f offset(
		Get<float>(REV_KEY_TOUCH_X, REV_DEFAULT_TOUCH_X),
		Get<float>(REV_KEY_TOUCH_Y, REV_DEFAULT_TOUCH_Y),
		Get<float>(REV_KEY_TOUCH_Z, REV_DEFAULT_TOUCH_Z)
	);

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
		memcpy(s.TouchOffset[i].m, matrix.M, sizeof(vr::HmdMatrix34_t));
	}

	// Add the state to the list and update the pointer
	InputSettingsList.push_back(s);
	Input = &InputSettingsList.back();
}
