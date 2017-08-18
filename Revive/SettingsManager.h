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
	ovrVector3f RotationOffset, PositionOffset;
	vr::HmdMatrix34_t TouchOffset[ovrHand_Count];

private:
	bool m_bRunning;
	std::thread m_thread;

	static void SettingsThread(SettingsManager* manager);
};
