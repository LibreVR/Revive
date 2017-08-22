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

TextureBase* CompositorVk::CreateTexture()
{
	return new TextureVk(m_device, m_physicalDevice, m_instance, &m_queue);
}

void CompositorVk::RenderMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	// TODO: Support mirror textures
}

void CompositorVk::RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad)
{
	// TODO: Support blending multiple scene layers
}
