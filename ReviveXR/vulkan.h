#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_win32.h>

#define VK_DEFINE_FUNCTION(func) PFN_##func func;

#define VK_LIBRARY_FUNCTION(library, func) \
	if(!(func = (PFN_##func)GetProcAddress(library, #func))) \
		return ovrError_InitializeVulkan;

#define VK_INSTANCE_FUNCTION(instance, func) \
	if(!(func = (PFN_##func)vkGetInstanceProcAddr(instance, #func))) \
		return ovrError_InitializeVulkan;

#define VK_DEVICE_FUNCTION(device, func) \
	if(!(func = (PFN_##func)vkGetDeviceProcAddr(device, #func))) \
		return false;

extern VK_DEFINE_FUNCTION(vkGetInstanceProcAddr)
extern VK_DEFINE_FUNCTION(vkGetDeviceProcAddr)
extern VK_DEFINE_FUNCTION(vkEnumeratePhysicalDevices)
extern VK_DEFINE_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
extern VK_DEFINE_FUNCTION(vkGetPhysicalDeviceProperties2KHR)
