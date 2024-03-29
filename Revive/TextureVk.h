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

	virtual void ToVRTexture(vr::Texture_t& texture);
	virtual bool Init(ovrTextureType Type, int Width, int Height, int MipLevels, int SampleCount,
		int ArraySize, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags);

	virtual bool LockSharedTexture() override;
	virtual bool UnlockSharedTexture() override;
	virtual bool CreateSharedTextureGL(unsigned int* outName) override;
	virtual void DeleteSharedTextureGL(unsigned int name) override;

	VkImage Image() { return m_image; }
	VkDevice Device() { return m_device; }

private:
	vr::VRVulkanTextureData_t m_data;

	VkImage m_image;
	VkDeviceMemory m_memory;
	VkPhysicalDeviceMemoryProperties m_memoryProperties;
	VkDevice m_device;
	VkQueue* m_pQueue;

	void* m_hMemoryHandle;
	unsigned int m_MemoryObject;
	VkCommandPool m_CmdPool;
	VkCommandBuffer m_LockCmd;
	VkCommandBuffer m_UnlockCmd;

	VkFormat TextureFormatToVkFormat(ovrTextureFormat format);
	VkImageUsageFlags BindFlagsToVkImageUsageFlags(unsigned int flags);
	bool GetMemoryType(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex);

	VK_DEFINE_FUNCTION(vkCreateImage)
	VK_DEFINE_FUNCTION(vkGetImageMemoryRequirements)
	VK_DEFINE_FUNCTION(vkAllocateMemory)
	VK_DEFINE_FUNCTION(vkBindImageMemory)
	VK_DEFINE_FUNCTION(vkFreeMemory)
	VK_DEFINE_FUNCTION(vkDestroyImage)

	// Needed for external textures
	VK_DEFINE_FUNCTION(vkGetMemoryWin32HandleKHR)
	VK_DEFINE_FUNCTION(vkCreateCommandPool)
	VK_DEFINE_FUNCTION(vkDestroyCommandPool)
	VK_DEFINE_FUNCTION(vkAllocateCommandBuffers)
	VK_DEFINE_FUNCTION(vkCmdPipelineBarrier)
	VK_DEFINE_FUNCTION(vkQueueSubmit)
	VK_DEFINE_FUNCTION(vkBeginCommandBuffer)
	VK_DEFINE_FUNCTION(vkEndCommandBuffer)
};

