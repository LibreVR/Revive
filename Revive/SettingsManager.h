#pragma once

#include <OVR_CAPI.h>
#include <openvr.h>
#include <thread>

// Forward declarations
enum revGripType;

class SettingsManager
{
public:
	SettingsManager();
	~SettingsManager();

	void LoadSettings();

	// Revive settings
	float Deadzone;
	float Sensitivity;
	revGripType ToggleGrip;
	bool TriggerAsGrip;
	float ToggleDelay;
	bool IgnoreActivity;
	vr::HmdMatrix34_t TouchOffset[ovrHand_Count];

	template<typename T> T Get(const char* key, T defaultVal);
};
