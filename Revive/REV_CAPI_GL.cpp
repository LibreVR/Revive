#include "OVR_CAPI_GL.h"
#include "Common.h"
#include "Session.h"
#include "CompositorGL.h"
#include "TextureGL.h"

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainGL(ovrSession session,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainGL);

	if (!desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	if (!session->Compositor)
	{
		session->Compositor.reset(CompositorGL::Create());
		if (!session->Compositor)
			return ovrError_RuntimeException;
	}

	if (session->Compositor->GetAPI() != vr::TextureType_OpenGL)
		return ovrError_RuntimeException;

	return session->Compositor->CreateTextureSwapChain(desc, out_TextureSwapChain);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferGL(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               unsigned int* out_TexId)
{
	REV_TRACE(ovr_GetTextureSwapChainBufferGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!chain || !out_TexId)
		return ovrError_InvalidParameter;

	if (index < 0)
		index = chain->CurrentIndex;

	TextureGL* texture = dynamic_cast<TextureGL*>(chain->Textures[index].get());
	if (!texture)
		return ovrError_InvalidParameter;

	*out_TexId = texture->Texture;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	REV_TRACE(ovr_CreateMirrorTextureGL);

	if (!desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	if (!session->Compositor)
	{
		session->Compositor.reset(CompositorGL::Create());
		if (!session->Compositor)
			return ovrError_RuntimeException;
	}

	if (session->Compositor->GetAPI() != vr::TextureType_OpenGL)
		return ovrError_RuntimeException;

	return session->Compositor->CreateMirrorTexture(desc, out_MirrorTexture);
}


OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureWithOptionsGL(ovrSession session,
                                                                    const ovrMirrorTextureDesc* desc,
                                                                    ovrMirrorTexture* out_MirrorTexture)
{
	return ovr_CreateMirrorTextureGL(session, desc, out_MirrorTexture);
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferGL(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            unsigned int* out_TexId)
{
	REV_TRACE(ovr_GetMirrorTextureBufferGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!mirrorTexture || !out_TexId)
		return ovrError_InvalidParameter;

	TextureGL* texture = dynamic_cast<TextureGL*>(mirrorTexture->Texture.get());
	if (!texture)
		return ovrError_InvalidParameter;

	*out_TexId = texture->Texture;
	return ovrSuccess;
}
