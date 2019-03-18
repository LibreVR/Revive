#include "OVR_CAPI_GL.h"
#include "Common.h"
#include "SwapChain.h"

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainGL(ovrSession session,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainGL);

	if (!desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	REV_TRACE(ovr_CreateMirrorTextureGL);

	if (!desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	return ovrError_Unsupported;
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

	return ovrError_Unsupported;
}
