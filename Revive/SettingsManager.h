#pragma once

#include <thread>
#include <list>
#include <atomic>

#include <openvr.h>
#include <OVR_CAPI.h>

#define SESSION_RELOAD_EVENT_NAME L"ReviveReloadSettings"

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

	SettingsManager();
	~SettingsManager();

	void ReloadSettings();
	template<typename T> T Get(const char* key, T defaultVal);

private:
	static void SettingsThreadFunc(SettingsManager* manager);
	std::thread SettingsThread;
	std::atomic_bool Running;

	// We keep a list of all instances, but we don't garbage collect them.
	// These structures rarely change, the app is short-lived and RCU is hard.
	std::list<InputSettings> InputSettingsList;
};
