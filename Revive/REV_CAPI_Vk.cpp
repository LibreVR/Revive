#include <vulkan/vulkan.h>
#include <openvr.h>
#include <vector>

#include "OVR_CAPI_Vk.h"
#include "Assert.h"
#include "Session.h"


OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetSessionPhysicalDeviceVk(
	ovrSession session,
	ovrGraphicsLuid luid,
	VkInstance instance,
	VkPhysicalDevice* out_physicalDevice)
{
	if (!out_physicalDevice)
		return ovrError_InvalidParameter;

	VkPhysicalDevice physicalDevice = 0;
#if 1
	// TODO: We could request this directly from OpenVR, but it seems to always return 0.
	vr::VRSystem()->GetOutputDevice((uint64_t*)&physicalDevice, vr::TextureType_Vulkan);
#else
	uint32_t gpu_count;
	std::vector<VkPhysicalDevice> devices;

	PFN_vkGetPhysicalDeviceProperties2KHR GetPhysicalDeviceProperties2KHR =
		(PFN_vkGetPhysicalDeviceProperties2KHR)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");

	// Get the number of devices (GPUs) available.
	VkResult res = vkEnumeratePhysicalDevices(instance, &gpu_count, NULL);
	// Allocate space and get the list of devices.
	devices.resize(gpu_count);
	res = vkEnumeratePhysicalDevices(instance, &gpu_count, devices.data());

	for (VkPhysicalDevice device : devices)
	{
		VkPhysicalDeviceIDPropertiesKHR uid = {};
		uid.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES_KHR;
		VkPhysicalDeviceProperties2KHR properties = {};
		properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;
		properties.pNext = &uid;

		GetPhysicalDeviceProperties2KHR(device, &properties);

		if (!uid.deviceLUIDValid)
			continue;

		if (memcmp(uid.deviceLUID, luid.Reserved, VK_LUID_SIZE_KHR) == 0)
		{
			physicalDevice = device;
			break;
		}
	}
#endif

	if (out_physicalDevice)
		*out_physicalDevice = physicalDevice;

	return physicalDevice ? ovrSuccess : ovrError_MismatchedAdapters;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetSynchonizationQueueVk(ovrSession session, VkQueue queue)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_CreateTextureSwapChainVk(
	ovrSession session,
	VkDevice device,
	const ovrTextureSwapChainDesc* desc,
	ovrTextureSwapChain* out_TextureSwapChain)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetTextureSwapChainBufferVk(
	ovrSession session,
	ovrTextureSwapChain chain,
	int index,
	VkImage* out_Image)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_CreateMirrorTextureWithOptionsVk(
	ovrSession session,
	VkDevice device,
	const ovrMirrorTextureDesc* desc,
	ovrMirrorTexture* out_MirrorTexture)
{
	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetMirrorTextureBufferVk(
	ovrSession session,
	ovrMirrorTexture mirrorTexture,
	VkImage* out_Image)
{
	return ovrError_Unsupported;
}

