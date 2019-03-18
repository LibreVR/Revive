#include "OVR_CAPI_Vk.h"
#include "Common.h"
#include "SwapChain.h"

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetInstanceExtensionsVk(
	ovrGraphicsLuid luid,
	char* extensionNames,
	uint32_t* inoutExtensionNamesSize)
{
	if (!inoutExtensionNamesSize)
		ovrError_InvalidParameter;

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetDeviceExtensionsVk(
	ovrGraphicsLuid luid,
	char* extensionNames,
	uint32_t* inoutExtensionNamesSize)
{
	if (!inoutExtensionNamesSize)
		ovrError_InvalidParameter;

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult)
ovr_GetSessionPhysicalDeviceVk(
	ovrSession session,
	ovrGraphicsLuid luid,
	VkInstance instance,
	VkPhysicalDevice* out_physicalDevice)
{
	return ovrError_Unsupported;
}

#undef ovr_SetSynchonizationQueueVk
OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetSynchonizationQueueVk(ovrSession session, VkQueue queue)
{
	return ovr_SetSynchronizationQueueVk(session, queue);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_SetSynchronizationQueueVk(ovrSession session, VkQueue queue)
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
	REV_TRACE(ovr_CreateTextureSwapChainVk);

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
}

