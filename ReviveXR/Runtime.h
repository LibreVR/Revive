#pragma once

#include "Common.h"
#include "OVR_CAPI.h"

#include <openxr/openxr.h>
#include <map>
#include <vector>

class Runtime
{
public:
	static Runtime& Get();

	enum Hack
	{
		// Hack: SteamVR runtime doesn't support the Oculus Touch interaction profile.
		// Use the Valve Index interaction profile instead.
		HACK_VALVE_INDEX_PROFILE,
		// Hack: WMR runtime doesn't support the Oculus Touch interaction profile.
		// Use the WMR motion controller interaction profile instead.
		HACK_WMR_PROFILE,
		// Hack: Some runtimes don't support the R11G11B10 swapchain format.
		// Fall back to the R10G10B10A2 format instead.
		HACK_NO_11BIT_FORMAT,
		// Hack: Some runtimes don't support the floating point swapchain format.
		// Fall back to the 8-bit sRGB format instead.
		HACK_NO_10BIT_FORMAT,
		// Hack: Some runtimes don't support 8-bit linear swapchain formats.
		// Fall back to the sRGB formats instead.
		HACK_NO_8BIT_LINEAR,
		// Hack: Some games only call GetRenderDesc once before the session is fully initialized.
		// Therefore we need to force the fallback field-of-view query so we get full ViewPoses.
		HACK_FORCE_FOV_FALLBACK,
		// Hack: SteamVR runtime has some inflexibilities in its swapchain creation.
		// To work around that we hook the D3D11 texture creation function and force our own parameters.
		HACK_HOOK_CREATE_TEXTURE,
		// Hack: SteamVR runtime allocates a buffer that is too big for the visibility line loop.
		// Causes the rest of the buffer to be filled with uninitialized coordinates.
		HACK_BROKEN_LINE_LOOP,
	};

	bool UseHack(Hack hack);
	ovrResult CreateInstance(XrInstance* out_Instance, const ovrInitParams* params);
	bool Supports(const char* extensionName);

	bool VisibilityMask;
	bool CompositionDepth;
	bool CompositionCube;
	bool CompositionCylinder;

	uint32_t MinorVersion;

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

	static const char* s_required_extensions[];
	static const char* s_optional_extensions[];
	static HackInfo s_known_hacks[];

	std::map<Hack, HackInfo> m_hacks;
	std::vector<const char*> m_extensions;
};
