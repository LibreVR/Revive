#include "TextureVk.h"
#include "vulkan.h"

#include <glad/glad.h>

TextureVk::TextureVk(VkDevice device, VkPhysicalDevice physicalDevice,
	VkInstance instance, VkQueue* pQueue)
	: m_data()
	, m_image()
	, m_memory()
	, m_memoryProperties()
	, m_device(device)
	, m_pQueue(pQueue)
	, m_hMemoryHandle(nullptr)
	, m_MemoryObject()
	, m_CmdPool()
	, m_LockCmd()
	, m_UnlockCmd()
{
	// Update the texture data
	m_data.m_pDevice = device;
	m_data.m_pPhysicalDevice = physicalDevice;
	m_data.m_pInstance = instance;

	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &m_memoryProperties);
}

TextureVk::~TextureVk()
{
	vkFreeMemory(m_device, m_memory, nullptr);
	vkDestroyImage(m_device, m_image, nullptr);
	if (m_CmdPool)
		vkDestroyCommandPool(m_device, m_CmdPool, nullptr);
}

void TextureVk::ToVRTexture(vr::Texture_t& texture)
{
	// Update the texture data
	m_data.m_pQueue = *m_pQueue;
	// FIXME: We don't know what family the queue is from
	texture.eColorSpace = vr::ColorSpace_Auto;
	texture.eType = vr::TextureType_Vulkan;
	texture.handle = &m_data;
}

VkFormat TextureVk::TextureFormatToVkFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_UNKNOWN:              return VK_FORMAT_UNDEFINED;
		case OVR_FORMAT_B5G6R5_UNORM:         return VK_FORMAT_B5G6R5_UNORM_PACK16;
		case OVR_FORMAT_B5G5R5A1_UNORM:       return VK_FORMAT_B5G5R5A1_UNORM_PACK16;
		case OVR_FORMAT_B4G4R4A4_UNORM:       return VK_FORMAT_B4G4R4A4_UNORM_PACK16;
		case OVR_FORMAT_R8G8B8A8_UNORM:       return VK_FORMAT_R8G8B8A8_UNORM;
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:  return VK_FORMAT_R8G8B8A8_SRGB;
		case OVR_FORMAT_B8G8R8A8_UNORM:       return VK_FORMAT_B8G8R8A8_UNORM;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:  return VK_FORMAT_B8G8R8A8_SRGB;
		case OVR_FORMAT_B8G8R8X8_UNORM:       return VK_FORMAT_B8G8R8_UNORM;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:  return VK_FORMAT_B8G8R8_SRGB;
		case OVR_FORMAT_R16G16B16A16_FLOAT:   return VK_FORMAT_R16G16B16A16_SFLOAT;
		case OVR_FORMAT_R11G11B10_FLOAT:      return VK_FORMAT_A2R10G10B10_UINT_PACK32; // TODO: OpenVR doesn't support R11G11B10

		// Depth formats
		case OVR_FORMAT_D16_UNORM:            return VK_FORMAT_D16_UNORM;
		case OVR_FORMAT_D24_UNORM_S8_UINT:    return VK_FORMAT_D24_UNORM_S8_UINT;
		case OVR_FORMAT_D32_FLOAT:            return VK_FORMAT_D32_SFLOAT;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;

		// Added in 1.5 compressed formats can be used for static layers
		case OVR_FORMAT_BC1_UNORM:            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
		case OVR_FORMAT_BC1_UNORM_SRGB:       return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
		case OVR_FORMAT_BC2_UNORM:            return VK_FORMAT_BC2_UNORM_BLOCK;
		case OVR_FORMAT_BC2_UNORM_SRGB:       return VK_FORMAT_BC2_SRGB_BLOCK;
		case OVR_FORMAT_BC3_UNORM:            return VK_FORMAT_BC3_UNORM_BLOCK;
		case OVR_FORMAT_BC3_UNORM_SRGB:       return VK_FORMAT_BC3_SRGB_BLOCK;
		case OVR_FORMAT_BC6H_UF16:            return VK_FORMAT_BC6H_UFLOAT_BLOCK;
		case OVR_FORMAT_BC6H_SF16:            return VK_FORMAT_BC6H_SFLOAT_BLOCK;
		case OVR_FORMAT_BC7_UNORM:            return VK_FORMAT_BC7_UNORM_BLOCK;
		case OVR_FORMAT_BC7_UNORM_SRGB:       return VK_FORMAT_BC7_SRGB_BLOCK;

		default:                              return VK_FORMAT_UNDEFINED;
	}
}

VkImageUsageFlags TextureVk::BindFlagsToVkImageUsageFlags(unsigned int flags)
{
	VkImageUsageFlags result = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	if (flags & ovrTextureBind_DX_RenderTarget)
		result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (flags & ovrTextureBind_DX_UnorderedAccess)
		result |= VK_IMAGE_USAGE_STORAGE_BIT;
	if (flags & ovrTextureBind_DX_DepthStencil)
		result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	return result;
}

bool TextureVk::GetMemoryType(uint32_t typeBits, VkFlags requirements_mask, uint32_t* typeIndex)
{
	// Search memtypes to find first index with those properties
	for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; i++) {
		if ((typeBits & 1) == 1) {
			// Type is available, does it match user properties?
			if ((m_memoryProperties.memoryTypes[i].propertyFlags & requirements_mask) == requirements_mask) {
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	// No memory types matched, return failure
	return false;
}

bool TextureVk::Init(ovrTextureType Type, int Width, int Height, int MipLevels, int SampleCount,
	int ArraySize, ovrTextureFormat Format, unsigned int MiscFlags, unsigned int BindFlags)
{
	VK_DEVICE_FUNCTION(m_device, vkCreateImage);
	VK_DEVICE_FUNCTION(m_device, vkGetImageMemoryRequirements);
	VK_DEVICE_FUNCTION(m_device, vkAllocateMemory);
	VK_DEVICE_FUNCTION(m_device, vkBindImageMemory);
	VK_DEVICE_FUNCTION(m_device, vkFreeMemory);
	VK_DEVICE_FUNCTION(m_device, vkDestroyImage);

	VkExternalMemoryImageCreateInfoKHR external_create_info = { VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR };
	external_create_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

	VkImageCreateInfo create_info = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
	if (Type == ovrTexture_2D_External)
		create_info.pNext = &external_create_info;
	create_info.imageType = VK_IMAGE_TYPE_2D;
	create_info.format = TextureFormatToVkFormat(Format);
	create_info.extent.width = Width;
	create_info.extent.height = Height;
	create_info.extent.depth = 1;
	create_info.mipLevels = MipLevels;
	create_info.arrayLayers = ArraySize;
	create_info.samples = (VkSampleCountFlagBits)SampleCount;
	create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	create_info.usage = BindFlagsToVkImageUsageFlags(BindFlags);
	create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(m_device, &create_info, nullptr, &m_image) != VK_SUCCESS)
		return false;

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(m_device, m_image, &memReqs);

	VkExportMemoryAllocateInfoKHR export_allocate_info = { VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO_KHR };
	export_allocate_info.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;

	VkMemoryAllocateInfo memAlloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	if (Type == ovrTexture_2D_External)
		memAlloc.pNext = &export_allocate_info;
	memAlloc.allocationSize = memReqs.size;
	if (!GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memAlloc.memoryTypeIndex))
		return false;

	if (vkAllocateMemory(m_device, &memAlloc, nullptr, &m_memory) != VK_SUCCESS)
		return false;

	if (vkBindImageMemory(m_device, m_image, m_memory, 0) != VK_SUCCESS)
		return false;

	if (Type == ovrTexture_2D_External)
	{
		VK_DEVICE_FUNCTION(m_device, vkCreateCommandPool);
		VK_DEVICE_FUNCTION(m_device, vkDestroyCommandPool);
		VK_DEVICE_FUNCTION(m_device, vkAllocateCommandBuffers);
		VK_DEVICE_FUNCTION(m_device, vkCmdPipelineBarrier);
		VK_DEVICE_FUNCTION(m_device, vkQueueSubmit);
		VK_DEVICE_FUNCTION(m_device, vkBeginCommandBuffer);
		VK_DEVICE_FUNCTION(m_device, vkEndCommandBuffer);

		VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
		if (vkCreateCommandPool(m_device, &pool_info, nullptr, &m_CmdPool) != VK_SUCCESS)
			return false;

		VkCommandBuffer cmdBufs[3];
		VkCommandBufferAllocateInfo cmd_allocate_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cmd_allocate_info.commandPool = m_CmdPool;
		cmd_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_allocate_info.commandBufferCount = 2;
		if (vkAllocateCommandBuffers(m_device, &cmd_allocate_info, cmdBufs) != VK_SUCCESS)
			return false;
		m_LockCmd = cmdBufs[0];
		m_UnlockCmd = cmdBufs[1];

		VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
		vkBeginCommandBuffer(m_LockCmd, &begin_info);
		vkBeginCommandBuffer(m_UnlockCmd, &begin_info);

		VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.image = m_image;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		vkCmdPipelineBarrier(m_LockCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		vkCmdPipelineBarrier(m_UnlockCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		vkEndCommandBuffer(m_LockCmd);
		vkEndCommandBuffer(m_UnlockCmd);
	}

	// Update texture data
	m_data.m_nImage = (uint64_t)m_image;
	m_data.m_nWidth = create_info.extent.width;
	m_data.m_nHeight = create_info.extent.height;
	m_data.m_nFormat = create_info.format;
	m_data.m_nSampleCount = create_info.samples;

	return true;
}

bool TextureVk::LockSharedTexture()
{
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &m_LockCmd;
	return vkQueueSubmit(*m_pQueue, 1, &submit_info, 0) == VK_SUCCESS;
}

bool TextureVk::UnlockSharedTexture()
{
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &m_UnlockCmd;
	return vkQueueSubmit(*m_pQueue, 1, &submit_info, 0) == VK_SUCCESS;
}

bool TextureVk::CreateSharedTextureGL(unsigned int* outName)
{
	VK_DEVICE_FUNCTION(m_device, vkGetMemoryWin32HandleKHR);

	VkMemoryGetWin32HandleInfoKHR handleInfo = { VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR };
	handleInfo.memory = m_memory;
	handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT_KHR;
	VkResult result = vkGetMemoryWin32HandleKHR(m_device, &handleInfo, &m_hMemoryHandle);
	if (result != VK_SUCCESS)
		return false;

	VkMemoryRequirements memReqs;
	vkGetImageMemoryRequirements(m_device, m_image, &memReqs);

	glCreateTextures(GL_TEXTURE_2D, 1, outName);
	glCreateMemoryObjectsEXT(1, &m_MemoryObject);
	glImportMemoryWin32HandleEXT(m_MemoryObject, memReqs.size, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, m_hMemoryHandle);
	glTextureStorageMem2DEXT(*outName, 1, GL_RGBA8, m_data.m_nWidth, m_data.m_nHeight, m_MemoryObject, 0);
	return true;
}

void TextureVk::DeleteSharedTextureGL(unsigned int name)
{
	glDeleteMemoryObjectsEXT(1, &m_MemoryObject);
	m_MemoryObject = 0;
	glDeleteTextures(1, &name);
	CloseHandle(m_hMemoryHandle);
	m_hMemoryHandle = nullptr;
}

