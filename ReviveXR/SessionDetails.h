#pragma once

#include <map>

#include "Common.h"
#include "OVR_CAPI.h"

class SessionDetails
{
public:
	enum Hack
	{
		// Hack: Windows Mixed Reality OpenXR runtime incorrectly handles gamma correction.
		// When using displays that require gamma correction, WMR will
		// always send linear colors, even if it means converting from
		// sRGB to linear. This is incorrect, it should be sending sRGB
		// to the display. To work around this, we pass sRGB colors to
		// the WMR runtime, but specify that they are linear.
		HACK_WMR_SRGB,
	};

	SessionDetails(XrInstance instance);
	~SessionDetails();

	bool UseHack(Hack hack);

private:
	struct HackInfo
	{
		const char* m_filename; // The filename of the main executable
		const char* m_driver;	// The name of the driver
		Hack m_hack;            // Which hack is it?
		bool m_usehack;         // Should it use the hack?
	};

	static HackInfo m_known_hacks[];
	std::map<Hack, HackInfo> m_hacks;
};
