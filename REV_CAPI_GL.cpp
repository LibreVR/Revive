#include "OVR_CAPI_GL.h"

#include "openvr.h"
#include <GL/glew.h>

#include "REV_Assert.h"

GLboolean glewInitialized = GL_FALSE;

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainGL(ovrSession session,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	if (!glewInitialized)
	{
		glewExperimental = GL_TRUE;
		GLenum nGlewError = glewInit();
		if (nGlewError != GLEW_OK)
			return ovrError_RuntimeException;
		glGetError(); // to clear the error caused deep in GLEW
		glewInitialized = GL_TRUE;
	}

	REV_UNIMPLEMENTED_RUNTIME;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferGL(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               unsigned int* out_TexId) { REV_UNIMPLEMENTED_RUNTIME; }

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	if (!glewInitialized)
	{
		glewExperimental = GL_TRUE;
		GLenum nGlewError = glewInit();
		if (nGlewError != GLEW_OK)
			return ovrError_RuntimeException;
		glGetError(); // to clear the error caused deep in GLEW
		glewInitialized = GL_TRUE;
	}

	REV_UNIMPLEMENTED_RUNTIME;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferGL(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            unsigned int* out_TexId) { REV_UNIMPLEMENTED_RUNTIME; }
