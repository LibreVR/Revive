#pragma once

#include "CompositorBase.h"

#include <openvr.h>
#include <utility>

class CompositorGL :
	public CompositorBase
{
public:
	CompositorGL();
	virtual ~CompositorGL();

	static CompositorGL* Create();
	virtual vr::ETextureType GetAPI() override { return vr::TextureType_OpenGL; };
	virtual void Flush() override;
	virtual TextureBase* CreateTexture() override;

	virtual void RenderTextureSwapChain(vr::EVREye eye, TextureBase* src, TextureBase* dst, ovrRecti viewport, vr::VRTextureBounds_t bounds, vr::HmdVector4_t quad);
	virtual void RenderMirrorTexture(ovrMirrorTexture mirrorTexture);

protected:
	std::pair<vr::glUInt_t, vr::glSharedTextureHandle_t> m_mirror[ovrEye_Count];
	unsigned int m_mirrorFB[ovrEye_Count];

private:
	static unsigned char gladInitialized;
};
