#include "Extensions.h"
#include "Common.h"

#include "version.h"

#include <algorithm>
#include <vector>

const char* requiredExtensions[] = {
	"XR_KHR_win32_convert_performance_counter_time",
	"XR_KHR_D3D11_enable"
};

const char* optionalExtensions[] = {
	"XR_KHR_D3D12_enable",
	"XR_KHR_vulkan_enable",
	"XR_KHR_opengl_enable",
	XR_KHR_VISIBILITY_MASK_EXTENSION_NAME,
	XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
	XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME,
	XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME
};

void Extensions::InitExtensionList(std::vector<XrExtensionProperties>& properties)
{
	m_extensions.clear();

	for (const char* extension : requiredExtensions)
		m_extensions.push_back(extension);

	for (const char* extension : optionalExtensions)
	{
		auto findExtension = [extension](XrExtensionProperties props)
		{
			return strcmp(props.extensionName, extension) == 0;
		};

		if (std::any_of(properties.begin(), properties.end(), findExtension))
			m_extensions.push_back(extension);
	}

	VisibilityMask		= Supports(XR_KHR_VISIBILITY_MASK_EXTENSION_NAME);
	CompositionDepth	= Supports(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
	CompositionCube		= Supports(XR_KHR_COMPOSITION_LAYER_CUBE_EXTENSION_NAME);
	CompositionCylinder = Supports(XR_KHR_COMPOSITION_LAYER_CYLINDER_EXTENSION_NAME);
}

ovrResult Extensions::CreateInstance(XrInstance* out_Instance)
{
	uint32_t size;
	std::vector<XrExtensionProperties> properties;
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &size, nullptr));
	properties.resize(size);
	for (XrExtensionProperties& props : properties)
		props = XR_TYPE(EXTENSION_PROPERTIES);
	CHK_XR(xrEnumerateInstanceExtensionProperties(nullptr, (uint32_t)properties.size(), &size, properties.data()));
	InitExtensionList(properties);

	XrInstanceCreateInfo createInfo = XR_TYPE(INSTANCE_CREATE_INFO);
	createInfo.applicationInfo = { "Revive", REV_VERSION_INT, "Revive", REV_VERSION_INT, XR_CURRENT_API_VERSION };
	createInfo.enabledExtensionCount = (uint32_t)m_extensions.size();
	createInfo.enabledExtensionNames = m_extensions.data();
	CHK_XR(xrCreateInstance(&createInfo, out_Instance));
	return ovrSuccess;
}

bool Extensions::Supports(const char* extensionName)
{
	auto findExtension = [extensionName](const char* extension)
	{
		return strcmp(extension, extensionName) == 0;
	};

	return std::any_of(m_extensions.begin(), m_extensions.end(), findExtension);
}
