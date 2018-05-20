#include "SettingsManager.h"
#include "Settings.h"
#include "REV_Math.h"
#include "OVR_CAPI.h"

#include <Windows.h>
#include <Shlobj.h>
#include <atlbase.h>

void SettingsManager::SettingThreadFunc(SettingsManager* settings)
{
	while (settings->m_Running)
	{
		std::chrono::microseconds freq(std::chrono::milliseconds(100));
		settings->ReloadSettings();
		std::this_thread::sleep_for(freq);
	}
}

SettingsManager::SettingsManager()
	: Input(std::make_shared<InputSettings>())
	, m_WorkingCopy(std::make_shared<InputSettings>())
	, m_Section()
	, m_Running(true)
{
	DWORD procId = GetCurrentProcessId();
	vr::EVRApplicationError err = vr::VRApplications()->GetApplicationKeyByProcessId(procId, m_Section, vr::k_unMaxApplicationKeyLength);
	if (err != vr::VRApplicationError_None)
		strcpy(m_Section, REV_SETTINGS_SECTION);

	LoadActionManifest();

	m_Thread = std::thread(SettingThreadFunc, this);
}

SettingsManager::~SettingsManager()
{
	m_Running = false;
	if (m_Thread.joinable())
		m_Thread.join();
}

template<> float SettingsManager::Get<float>(const char* key, float defaultVal)
{
	vr::EVRSettingsError err;
	float result = vr::VRSettings()->GetFloat(m_Section, key, &err);
	if (err != vr::VRSettingsError_None)
		result = vr::VRSettings()->GetFloat(REV_SETTINGS_SECTION, key, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

template<> int SettingsManager::Get<int>(const char* key, int defaultVal)
{
	vr::EVRSettingsError err;
	int result = vr::VRSettings()->GetInt32(m_Section, key, &err);
	if (err != vr::VRSettingsError_None)
		result = vr::VRSettings()->GetInt32(REV_SETTINGS_SECTION, key, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

template<> bool SettingsManager::Get<bool>(const char* key, bool defaultVal)
{
	vr::EVRSettingsError err;
	bool result = vr::VRSettings()->GetBool(m_Section, key, &err);
	if (err != vr::VRSettingsError_None)
		result = vr::VRSettings()->GetBool(REV_SETTINGS_SECTION, key, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

template<> const char* SettingsManager::Get<const char*>(const char* key, const char* defaultVal)
{
	vr::EVRSettingsError err;
	static char result[MAX_PATH]; // TODO: Support larger string sizes
	vr::VRSettings()->GetString(m_Section, key, result, MAX_PATH, &err);
	if (err != vr::VRSettingsError_None)
		vr::VRSettings()->GetString(REV_SETTINGS_SECTION, key, result, MAX_PATH, &err);
	return err == vr::VRSettingsError_None ? result : defaultVal;
}

void SettingsManager::ReloadSettings()
{
	m_WorkingCopy->Deadzone = Get<float>(REV_KEY_THUMB_DEADZONE, REV_DEFAULT_THUMB_DEADZONE);
	m_WorkingCopy->ToggleGrip = (revGripType)Get<int>(REV_KEY_TOGGLE_GRIP, REV_DEFAULT_TOGGLE_GRIP);
	m_WorkingCopy->ToggleDelay = Get<float>(REV_KEY_TOGGLE_DELAY, REV_DEFAULT_TOGGLE_DELAY);
	m_WorkingCopy->TriggerAsGrip = Get<bool>(REV_KEY_TRIGGER_GRIP, REV_DEFAULT_TRIGGER_GRIP);

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
		memcpy(m_WorkingCopy->TouchOffset[i].m, matrix.M, sizeof(vr::HmdMatrix34_t));
	}

	// Swap the input settings pointers
	Input.swap(m_WorkingCopy);
}

bool SettingsManager::FileExists(const char* path)
{
	DWORD attrib = GetFileAttributesA(path);

	return (attrib != INVALID_FILE_ATTRIBUTES &&
		!(attrib & FILE_ATTRIBUTE_DIRECTORY));
}

void SettingsManager::LoadActionManifest()
{
	CComHeapPtr<wchar_t> folder;
	HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_DEFAULT, NULL, &folder);

	if (SUCCEEDED(hr))
	{
		char path[MAX_PATH];
		snprintf(path, MAX_PATH, "%ls\\Revive\\Input\\action_manifest.json", (wchar_t*)folder);
		vr::EVRInputError err = vr::VRInput()->SetActionManifestPath(path);
		if (err == vr::VRInputError_None)
			return;
	}

	vr::VROverlay()->ShowMessageOverlay("Failed to load action manifest, input will not function correctly!", "Action manifest error", "Continue");
}
