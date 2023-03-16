#include "OVR_CAPI_GL.h"
#include "Common.h"
#include "Session.h"
#include "Runtime.h"
#include "SwapchainGL.h"

#include <vector>

#include <Windows.h>
#define XR_USE_GRAPHICS_API_OPENGL
#include <glad/glad.h>
#include <openxr/openxr_platform.h>

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainGL(ovrSession session,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	REV_TRACE(ovr_CreateTextureSwapChainGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!desc || !out_TextureSwapChain || desc->Type == ovrTexture_2D_External)
		return ovrError_InvalidParameter;

	if (!session->Session)
	{
		if (!Runtime::Get().Supports(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME))
			return ovrError_Unsupported;

		XR_FUNCTION(session->Instance, GetOpenGLGraphicsRequirementsKHR);
		XrGraphicsRequirementsOpenGLKHR graphicsReq = XR_TYPE(GRAPHICS_REQUIREMENTS_OPENGL_KHR);
		CHK_XR(GetOpenGLGraphicsRequirementsKHR(session->Instance, session->System, &graphicsReq));

		if (gladLoadGL())
		{
			GLint major, minor;
			glGetIntegerv(GL_MAJOR_VERSION, &major);
			glGetIntegerv(GL_MINOR_VERSION, &minor);
			XrVersion version = XR_MAKE_VERSION(major, minor, 0);
			if (version < graphicsReq.minApiVersionSupported)
				return ovrError_IncompatibleGPU;
		}

		XrGraphicsBindingOpenGLWin32KHR graphicsBinding = XR_TYPE(GRAPHICS_BINDING_OPENGL_WIN32_KHR);
		graphicsBinding.hDC = wglGetCurrentDC();
		graphicsBinding.hGLRC = wglGetCurrentContext();
		session->StartSession(&graphicsBinding);
	}
	return ovrTextureSwapChainGL::Create(session, desc, out_TextureSwapChain);
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

	*out_TexId = ((XrSwapchainImageOpenGLKHR*)chain->Images)[index].image;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	REV_TRACE(ovr_CreateMirrorTextureGL);

	if (!session)
		return ovrError_InvalidSession;

	if (!desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData();
	mirrorTexture->Desc = *desc;
	*out_MirrorTexture = mirrorTexture;

	ovrTextureSwapChainDesc swapDesc;
	swapDesc.Type = ovrTexture_2D;
	swapDesc.Format = desc->Format;
	swapDesc.ArraySize = 1;
	swapDesc.Width = desc->Width;
	swapDesc.Height = desc->Height;
	swapDesc.MipLevels = 1;
	swapDesc.SampleCount = 1;
	swapDesc.StaticImage = ovrTrue;
	swapDesc.MiscFlags = desc->MiscFlags;
	swapDesc.BindFlags = 0;
	return ovr_CreateTextureSwapChainGL(session, &swapDesc, &mirrorTexture->Dummy);
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

	return ovr_GetTextureSwapChainBufferGL(session, mirrorTexture->Dummy, 0, out_TexId);
}
