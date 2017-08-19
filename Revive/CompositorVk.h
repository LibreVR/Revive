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

	// Texture Swapchain
	virtual ovrResult CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain);
	virtual void RenderTextureSwapChain(vr::EVREye eye, ovrTextureSwapChain swapChain, ovrTextureSwapChain sceneChain, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);

	// Mirror Texture
	virtual ovrResult CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture, ovrTextureSwapChain swapChain[ovrEye_Count]);

	void SetDevice(VkDevice device) { m_device = device; }
	void SetQueue(VkQueue queue) { m_queue = queue; }

private:
	VkDevice m_device;
	VkPhysicalDevice m_physicalDevice;
	VkInstance m_instance;
	VkQueue m_queue;
};

