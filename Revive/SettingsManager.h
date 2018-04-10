#pragma once

#include <list>
#include <atomic>
#include <thread>
#include "rcu_ptr.h"

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
	SettingsManager();
	~SettingsManager();

	void ReloadSettings();
	std::string GetInputScript();
	template<typename T> T Get(const char* key, T defaultVal);

	rcu_ptr<InputSettings> Input;

private:
	char m_Section[vr::k_unMaxApplicationKeyLength];
	std::shared_ptr<InputSettings> m_WorkingCopy;

	bool m_Running;
	std::thread m_Thread;

	bool FileExists(const char* path);
	static void SettingThreadFunc(SettingsManager* settings);
};
