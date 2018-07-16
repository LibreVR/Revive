#include "OVR_CAPI_Vk.h"
#include "Assert.h"
#include "Session.h"
#include "CompositorVk.h"
#include "TextureVk.h"
#include "vulkan.h"

#include <Windows.h>
#include <openvr.h>
#include <vector>

HMODULE VulkanLibrary;
VkPhysicalDevice g_physicalDevice;
VkDebugUtilsMessengerEXT g_Messenger;

// Global functions
VK_DEFINE_FUNCTION(vkGetInstanceProcAddr)
VK_DEFINE_FUNCTION(vkGetDeviceProcAddr)
VK_DEFINE_FUNCTION(vkEnumeratePhysicalDevices)
VK_DEFINE_FUNCTION(vkGetPhysicalDeviceMemoryProperties)
VK_DEFINE_FUNCTION(vkGetPhysicalDeviceProperties2KHR)

#ifdef DEBUG
VK_DEFINE_FUNCTION(vkCreateDebugUtilsMessengerEXT)
VK_DEFINE_FUNCTION(vkDestroyDebugUtilsMessengerEXT)
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageType,
	const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
	void*                                            pUserData)
{
	OutputDebugStringA(pCallbackData->pMessage);
	OutputDebugStringA("\n");
	return VK_FALSE;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetInstanceExtensionsVk(
	ovrGraphicsLuid luid,
	char* extensionNames,
	uint32_t* inoutExtensionNamesSize)
{
	if (!inoutExtensionNamesSize)
		ovrError_InvalidParameter;

	uint32_t size = *inoutExtensionNamesSize;
	uint32_t required = vr::VRCompositor()->GetVulkanInstanceExtensionsRequired(extensionNames, *inoutExtensionNamesSize);

	if (required <= size)
	{
		strcpy(extensionNames + required, VK_KHR_SURFACE_EXTENSION_NAME);
		extensionNames[required - 1] = ' ';
	}
	required += (uint32_t)strlen(VK_KHR_SURFACE_EXTENSION_NAME) + 1;
	if (required <= size)
	{
		strcpy(extensionNames + required, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		extensionNames[required - 1] = ' ';
	}
	required += (uint32_t)strlen(VK_KHR_WIN32_SURFACE_EXTENSION_NAME) + 1;

#ifdef DEBUG
	if (required <= size)
	{
		strcpy(extensionNames + required, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		extensionNames[required - 1] = ' ';
	}
	required += (uint32_t)strlen(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) + 1;
#endif

	*inoutExtensionNamesSize = required;

	return (size < required) ? ovrError_InsufficientArraySize : ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetDeviceExtensionsVk(
	ovrGraphicsLuid luid,
	char* extensionNames,
	uint32_t* inoutExtensionNamesSize)
{
	if (!inoutExtensionNamesSize)
		ovrError_InvalidParameter;

	uint32_t size = *inoutExtensionNamesSize;
	uint32_t required = vr::VRCompositor()->GetVulkanDeviceExtensionsRequired(g_physicalDevice, extensionNames, *inoutExtensionNamesSize);

	if (required <= size)
	{
		strcpy(extensionNames + required, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
		extensionNames[required - 1] = ' ';
	}
	required += (uint32_t)strlen(VK_KHR_SWAPCHAIN_EXTENSION_NAME) + 1;

	*inoutExtensionNamesSize = required;

	return (size < required) ? ovrError_InsufficientArraySize : ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetSessionPhysicalDeviceVk(
	ovrSession session,
	ovrGraphicsLuid luid,
	VkInstance instance,
	VkPhysicalDevice* out_physicalDevice)
{
	VulkanLibrary = LoadLibraryW(L"vulkan-1.dll");
	if (!VulkanLibrary)
		return ovrError_InitializeVulkan;

	VK_LIBRARY_FUNCTION(VulkanLibrary, vkGetInstanceProcAddr)
	VK_INSTANCE_FUNCTION(instance, vkGetDeviceProcAddr)
	VK_INSTANCE_FUNCTION(instance, vkEnumeratePhysicalDevices)
	VK_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceMemoryProperties)
	VK_INSTANCE_FUNCTION(instance, vkGetPhysicalDeviceProperties2KHR)

#ifdef DEBUG
	VK_INSTANCE_FUNCTION(instance, vkCreateDebugUtilsMessengerEXT)
	VK_INSTANCE_FUNCTION(instance, vkDestroyDebugUtilsMessengerEXT)

	VkDebugUtilsMessengerCreateInfoEXT callback1 = {
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,  // sType
		NULL,                                                     // pNext
		0,                                                        // flags
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |           // messageSeverity
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT,
		VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |             // messageType
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT,
		DebugUtilsCallback,                                       // pfnUserCallback
		NULL                                                      // pUserData
	};
	vkCreateDebugUtilsMessengerEXT(instance, &callback1, nullptr, &g_Messenger);
#endif

	VkPhysicalDevice physicalDevice = 0;
	vr::VRSystem()->GetOutputDevice((uint64_t*)&physicalDevice, vr::TextureType_Vulkan, instance);

	// Fall-back in case GetOutputDevice fails.
	if (!physicalDevice)
	{
		uint32_t gpu_count;
		std::vector<VkPhysicalDevice> devices;

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

			vkGetPhysicalDeviceProperties2KHR(device, &properties);

			if (!uid.deviceLUIDValid)
				continue;

			if (memcmp(uid.deviceLUID, luid.Reserved, VK_LUID_SIZE_KHR) == 0)
			{
				physicalDevice = device;
				break;
			}
		}
	}

	g_physicalDevice = physicalDevice;

	if (!session->Compositor)
	{
		session->Compositor.reset(new CompositorVk(physicalDevice, instance));
		if (!session->Compositor)
			return ovrError_RuntimeException;
	}

	if (out_physicalDevice)
		*out_physicalDevice = physicalDevice;

	return physicalDevice ? ovrSuccess : ovrError_MismatchedAdapters;
}

#undef ovr_SetSynchonizationQueueVk
OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetSynchonizationQueueVk(ovrSession session, VkQueue queue)
{
	return ovr_SetSynchronizationQueueVk(session, queue);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetSynchronizationQueueVk(ovrSession session, VkQueue queue)
{
	CompositorVk* compositor = dynamic_cast<CompositorVk*>(session->Compositor.get());

	if (!compositor)
		return ovrError_RuntimeException;

	compositor->SetQueue(queue);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_CreateTextureSwapChainVk(
	ovrSession session,
	VkDevice device,
	const ovrTextureSwapChainDesc* desc,
	ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainVk);

	if (!session)
		return ovrError_InvalidSession;

	if (!device || !desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	CompositorVk* compositor = dynamic_cast<CompositorVk*>(session->Compositor.get());

	if (!compositor)
		return ovrError_RuntimeException;

	compositor->SetDevice(device);

	if (session->Compositor->GetAPI() != vr::TextureType_Vulkan)
		return ovrError_RuntimeException;

	return session->Compositor->CreateTextureSwapChain(desc, out_TextureSwapChain);
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetTextureSwapChainBufferVk(
	ovrSession session,
	ovrTextureSwapChain chain,
	int index,
	VkImage* out_Image)
{
	REV_TRACE(ovr_GetTextureSwapChainBufferDX);

	if (!session)
		return ovrError_InvalidSession;

	if (!chain || !out_Image)
		return ovrError_InvalidParameter;

	if (index < 0)
		index = chain->CurrentIndex;

	TextureVk* texture = dynamic_cast<TextureVk*>(chain->Textures[index].get());
	if (!texture)
		return ovrError_RuntimeException;

	*out_Image = texture->Image();

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_CreateMirrorTextureWithOptionsVk(
	ovrSession session,
	VkDevice device,
	const ovrMirrorTextureDesc* desc,
	ovrMirrorTexture* out_MirrorTexture)
{
	REV_TRACE(ovr_CreateMirrorTextureWithOptionsVk);

	if (!session)
		return ovrError_InvalidSession;

	if (!device || !desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	CompositorVk* compositor = dynamic_cast<CompositorVk*>(session->Compositor.get());
	if (!compositor)
		return ovrError_RuntimeException;

	compositor->SetDevice(device);

	if (session->Compositor->GetAPI() != vr::TextureType_Vulkan)
		return ovrError_RuntimeException;

	return session->Compositor->CreateMirrorTexture(desc, out_MirrorTexture);
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetMirrorTextureBufferVk(
	ovrSession session,
	ovrMirrorTexture mirrorTexture,
	VkImage* out_Image)
{
	REV_TRACE(ovr_GetMirrorTextureBufferVk);

	if (!session)
		return ovrError_InvalidSession;

	if (!mirrorTexture || !out_Image)
		return ovrError_InvalidParameter;

	TextureVk* texture = dynamic_cast<TextureVk*>(mirrorTexture->Texture.get());
	if (!texture)
		return ovrError_RuntimeException;

	*out_Image = texture->Image();

	return ovrSuccess;
}

