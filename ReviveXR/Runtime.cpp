#include "Runtime.h"
#include "Common.h"
#include "version.h"

#include <Windows.h>
#include <Shlwapi.h>
#include <algorithm>
#include <vector>
#include <memory>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

const char* Runtime::s_required_extensions[] = {
	"XR_KHR_win32_convert_performance_counter_time",
	"XR_KHR_D3D11_enable"
};

const char* Runtime::s_optional_extensions[] = {
	"XR_KHR_D3D12_enable",
	"XR_KHR_vulkan_enable",
	"XR_KHR_opengl_enable",
	XR_KHR_VISIBILITY_MASK_EXTENSION_NAME,
	XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
	XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME,
	XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME,
	XR_EPIC_VIEW_CONFIGURATION_FOV_EXTENSION_NAME
};

Runtime::HackInfo Runtime::s_known_hacks[] = {
	{ nullptr, "SteamVR/OpenXR", HACK_VALVE_INDEX_PROFILE, 0, 0, true },
	{ nullptr, "SteamVR/OpenXR", HACK_NO_11BIT_FORMAT, 0, 0, true },
	{ nullptr, "SteamVR/OpenXR", HACK_NO_8BIT_LINEAR, 0, 0, true },
	{ nullptr, "SteamVR/OpenXR", HACK_HOOK_CREATE_TEXTURE, 0, 0, true },
	{ nullptr, "SteamVR/OpenXR", HACK_BROKEN_LINE_LOOP, 0, 0, true },
	{ nullptr, "Windows Mixed Reality Runtime", HACK_NO_10BIT_FORMAT, 0, 0, true },
	{ "echovr.exe", nullptr, HACK_FORCE_FOV_FALLBACK, 0, 0, true },
	{ "loneecho.exe", nullptr, HACK_FORCE_FOV_FALLBACK, 0, 0, true },
};

Runtime& Runtime::Get()
{
	static Runtime instance;
	return instance;
}

ovrResult Runtime::CreateInstance(XrInstance* out_Instance, const ovrInitParams* params)
{
	MinorVersion = params && params->Flags & ovrInit_RequestVersion ?
		params->RequestedMinorVersion : OVR_MINOR_VERSION;

	uint32_t size;
	std::vector<XrExtensionProperties> properties;
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &size, nullptr));
	properties.resize(size);
	for (XrExtensionProperties& props : properties)
		props = XR_TYPE(EXTENSION_PROPERTIES);
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)properties.size(), &size, properties.data()));

	m_extensions.clear();

	for (const char* extension : s_required_extensions)
		m_extensions.push_back(extension);

	for (const char* extension : s_optional_extensions)
	{
		auto findExtension = [extension](XrExtensionProperties props)
		{
			return strcmp(props.extensionName, extension) == 0;
		};

		if (std::any_of(properties.begin(), properties.end(), findExtension))
			m_extensions.push_back(extension);
	}

	VisibilityMask = Supports(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
	CompositionDepth = Supports(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
	CompositionCube = Supports(XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME);
	CompositionCylinder = Supports(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);

	XrInstanceCreateInfo createInfo = XR_TYPE(INSTANCE_CREATE_INFO);
	createInfo.applicationInfo = { "Revive", REV_VERSION_INT, "Revive", REV_VERSION_INT, XR_CURRENT_API_VERSION };
	createInfo.enabledExtensionCount = (uint32_t)m_extensions.size();
	createInfo.enabledExtensionNames = m_extensions.data();
	CHK_XR(xrCreateInstance(&createInfo, out_Instance));

	char filepath[MAX_PATH];
	GetModuleFileNameA(NULL, filepath, MAX_PATH);
	char* filename = PathFindFileNameA(filepath);

	XrInstanceProperties props = XR_TYPE(INSTANCE_PROPERTIES);
	CHK_XR(xrGetInstanceProperties(*out_Instance, &props));

	for (auto& hack : s_known_hacks)
	{
		if ((!hack.m_filename || _stricmp(filename, hack.m_filename) == 0) &&
			(!hack.m_runtime || strcmp(props.runtimeName, hack.m_runtime) == 0) &&
			(!hack.m_versionend || hack.m_versionstart <= props.runtimeVersion &&
				props.runtimeVersion < hack.m_versionend))
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

bool Runtime::UseHack(Hack hack)
{
	return m_hacks.find(hack) != m_hacks.end();
}

bool Runtime::Supports(const char* extensionName)
{
	auto findExtension = [extensionName](const char* extension)
	{
		return strcmp(extension, extensionName) == 0;
	};

	return std::any_of(m_extensions.begin(), m_extensions.end(), findExtension);
}
