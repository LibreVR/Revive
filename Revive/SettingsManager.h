#pragma once

#include <list>
#include <atomic>

#include <openvr.h>
#include <OVR_CAPI.h>

// Forward declarations
enum revGripType;

struct InputSettings
{
	float Deadzone;
	revGripType ToggleGrip;
	bool TriggerAsGrip;
	float ToggleDelay;
	vr::HmdMatrix34_t TouchOffset[ovrHand_Count];
};

class SettingsManager
{
public:
	std::atomic<InputSettings*> Input;
	std::string InputScript;

	SettingsManager();
	~SettingsManager();

	void ReloadSettings();
	template<typename T> T Get(const char* key, T defaultVal);

private:
	// We keep a list of all instances, but we don't garbage collect them.
	// These structures rarely change, the app is short-lived and RCU is hard.
	std::list<InputSettings> InputSettingsList;
};
