#include "SettingsManager.h"
#include "Settings.h"
#include "REV_Math.h"

#include <Windows.h>
#include <Shlobj.h>
#include <OVR_CAPI.h>
#include <sstream>

SettingsManager::SettingsManager()
{
	ReloadSettings();
}

SettingsManager::~SettingsManager()
{
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

template<> const char* SettingsManager::Get<const char*>(const char* key, const char* defaultVal)
{
	vr::EVRSettingsError err;
	static char result[MAX_PATH]; // TODO: Support larger string sizes
	vr::VRSettings()->GetString(REV_SETTINGS_SECTION, key, result, MAX_PATH, &err);
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

	const char* script = Get<const char*>(REV_KEY_INPUT_SCRIPT, REV_DEFAULT_INPUT_SCRIPT);
	wchar_t* documents;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_Documents, KF_FLAG_DEFAULT, NULL, &documents);
	if (SUCCEEDED(hr))
	{
		std::stringstream ss;
		ss << documents << "\\Revive\\Input\\" << script;
		InputScript = ss.str();
		CoTaskMemFree(documents);
	}
	else
	{
		// Let's try a relative path instead
		InputScript = script;
	}
}
