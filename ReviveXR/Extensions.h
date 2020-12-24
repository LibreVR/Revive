#pragma once

#include <OVR_ErrorCode.h>
#include <openxr/openxr.h>
#include <vector>

class Extensions
{
public:
	ovrResult CreateInstance(XrInstance* out_Instance);
	bool Supports(const char* extensionName);

	bool VisibilityMask;
	bool CompositionDepth;
	bool CompositionCube;
	bool CompositionCylinder;

private:
	std::vector<const char*> m_extensions;
	void InitExtensionList(std::vector<XrExtensionProperties>& properties);
};
