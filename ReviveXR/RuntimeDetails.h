#pragma once

#include "Common.h"
#include "OVR_CAPI.h"

#include <openxr/openxr.h>
#include <map>

class RuntimeDetails
{
public:
	enum Hack
	{
		// Hack: SteamVR OpenXR runtime doesn't support the Oculus Touch interaction profile.
		// Use the Valve Index interaction profile instead.
		HACK_VALVE_INDEX_PROFILE,
	};

	ovrResult InitHacks(XrInstance);
	bool UseHack(Hack hack);

private:
	struct HackInfo
	{
		const char* m_filename;		// The filename of the main executable
		const char* m_runtime;		// The name of the runtime
		Hack m_hack;				// Which hack is it?
		XrVersion m_versionstart;	// When it started
		XrVersion m_versionend;		// When it ended
		bool m_usehack;				// Should it use the hack?
	};

	static HackInfo m_known_hacks[];
	std::map<Hack, HackInfo> m_hacks;
};
