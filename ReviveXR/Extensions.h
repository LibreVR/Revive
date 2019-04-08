#pragma once

#include <OVR_ErrorCode.h>
#include <openxr/openxr.h>
#include <vector>

// TODO: This could be refactored into an instance class
class Extensions
{
public:
	void InitExtensionList(std::vector<XrExtensionProperties>& properties);
	XrInstanceCreateInfo GetInstanceCreateInfo();
	bool Supports(const char* extensionName);

	bool VisibilityMask;
	bool CompositionDepth;
	bool CompositionCube;
	bool CompositionCylinder;

private:
	std::vector<const char*> m_extensions;
};
