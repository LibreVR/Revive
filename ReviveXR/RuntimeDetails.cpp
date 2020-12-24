#include "RuntimeDetails.h"
#include "Common.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <vector>
#include <memory>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

// TODO: test and port remaining hacks from Revive classic
RuntimeDetails::HackInfo RuntimeDetails::m_known_hacks[] = {
	{ nullptr, "SteamVR/OpenXR", HACK_VALVE_INDEX_PROFILE, true },
	{ nullptr, "SteamVR/OpenXR", HACK_10BIT_FORMAT, true },
};

ovrResult RuntimeDetails::InitHacks(XrInstance instance)
{
	char filepath[MAX_PATH];
	GetModuleFileNameA(NULL, filepath, MAX_PATH);
	char* filename = PathFindFileNameA(filepath);

	XrInstanceProperties props = XR_TYPE(INSTANCE_PROPERTIES);
	CHK_XR(xrGetInstanceProperties(instance, &props));

	for (auto& hack : m_known_hacks)
	{
		if ((!hack.m_filename || _stricmp(filename, hack.m_filename) == 0) &&
			(!hack.m_runtime || strcmp(props.runtimeName, hack.m_runtime) == 0) &&
			(hack.m_versionstart <= props.runtimeVersion && props.runtimeVersion < hack.m_versionend))
		{
			if (hack.m_usehack)
				m_hacks.emplace(hack.m_hack, hack);
		}
		else if (!hack.m_usehack)
		{
			m_hacks.emplace(hack.m_hack, hack);
		}
	}
	return ovrSuccess;
}

bool RuntimeDetails::UseHack(Hack hack)
{
	return m_hacks.find(hack) != m_hacks.end();
}
