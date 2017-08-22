#pragma once

#include "CompositorBase.h"

class CompositorVk :
	public CompositorBase
{
public:
	CompositorVk(VkPhysicalDevice physicalDevice, VkInstance instance);
	virtual ~CompositorVk();

	virtual vr::ETextureType GetAPI() { return vr::TextureType_Vulkan; };
	virtual void Flush() { };
	virtual TextureBase* CreateTexture();

	virtual void RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture);

	void SetDevice(VkDevice device) { m_device = device; }
	void SetQueue(VkQueue queue) { m_queue = queue; }

private:
	VkDevice m_device;
	VkPhysicalDevice m_physicalDevice;
	VkInstance m_instance;
	VkQueue m_queue;
};

