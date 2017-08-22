#include "CompositorVk.h"
#include "TextureVK.h"
#include "OVR_CAPI.h"

CompositorVk::CompositorVk(VkPhysicalDevice physicalDevice, VkInstance instance)
	: m_physicalDevice(physicalDevice)
	, m_instance(instance)
{
}

CompositorVk::~CompositorVk()
{
}

ovrResult CompositorVk::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(vr::TextureType_Vulkan, *desc);
	swapChain->Identifier = m_ChainCount++;

	for (int i = 0; i < swapChain->Length; i++)
	{
		TextureVk* texture = new TextureVk(m_device, m_physicalDevice, m_instance, &m_queue);
		bool success = texture->Create(desc->Width, desc->Height, desc->MipLevels, desc->ArraySize, desc->Format,
			desc->MiscFlags, desc->BindFlags);
		if (!success)
			return ovrError_RuntimeException;
		swapChain->Textures[i].reset(texture);
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

ovrResult CompositorVk::CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture)
{
	// There can only be one mirror texture at a time
	if (m_MirrorTexture)
		return ovrError_RuntimeException;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(vr::TextureType_Vulkan, *desc);
	TextureVk* texture = new TextureVk(m_device, m_physicalDevice, m_instance, &m_queue);
	bool success = texture->Create(desc->Width, desc->Height, 1, 1, desc->Format,
		desc->MiscFlags, 0);
	if (!success)
		return ovrError_RuntimeException;
	mirrorTexture->Texture.reset(texture);

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

void CompositorVk::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	// TODO: Support mirror textures
}

void CompositorVk::RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad)
{
	// TODO: Support blending multiple scene layers
}
