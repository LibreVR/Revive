#include "SessionDetails.h"
#include "Common.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <vector>
#include <memory>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

// TODO: test and port remaining hacks from Revive classic
SessionDetails::HackInfo SessionDetails::m_known_hacks[] = {
	{ nullptr, nullptr, HACK_WMR_SRGB, true },
};

SessionDetails::SessionDetails(XrInstance instance)
{
	/* Check OpenXR runtime for WMR */
	XrInstanceProperties props = XR_TYPE(INSTANCE_PROPERTIES);
	assert(XR_SUCCEEDED(xrGetInstanceProperties(instance, &props)));
	if (strstr(props.runtimeName, "Windows Mixed Reality Runtime")) {
		auto& hack = m_known_hacks[HACK_WMR_SRGB];
		m_hacks.emplace(HACK_WMR_SRGB, hack);
	}
}

SessionDetails::~SessionDetails()
{
}

bool SessionDetails::UseHack(Hack hack)
{
	return m_hacks.find(hack) != m_hacks.end();
}
