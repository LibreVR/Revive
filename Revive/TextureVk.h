#pragma once

#include "TextureBase.h"
#include "CompositorVk.h"
#include "vulkan.h"

#include <openvr.h>

class TextureVk :
	public TextureBase
{
public:
	TextureVk(VkDevice device, VkPhysicalDevice physicalDevice,
		VkInstance instance, VkQueue* pQueue);
	virtual ~TextureVk();

	virtual vr::VRTextureWithPose_t ToVRTexture();
	virtual bool Init(ovrTextureType type, int Width, int Height, int MipLevels, int ArraySize,
		ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	VkImage Image() { return m_image; };

private:
	vr::VRVulkanTextureData_t m_data;

	VkImage m_image;
	VkDeviceMemory m_memory;
	VkPhysicalDeviceMemoryProperties m_memoryProperties;
	VkDevice m_device;
	VkQueue* m_pQueue;

	VkFormat TextureFormatToVkFormat(ovrTextureFormat format);
	VkImageUsageFlags BindFlagsToVkImageUsageFlags(unsigned int flags);
	bool GetMemoryType(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex);

	VK_DEFINE_FUNCTION(vkCreateImage)
	VK_DEFINE_FUNCTION(vkGetImageMemoryRequirements)
	VK_DEFINE_FUNCTION(vkAllocateMemory)
	VK_DEFINE_FUNCTION(vkBindImageMemory)
	VK_DEFINE_FUNCTION(vkFreeMemory)
	VK_DEFINE_FUNCTION(vkDestroyImage)
};

