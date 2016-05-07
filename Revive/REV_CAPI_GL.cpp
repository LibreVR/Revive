#include "OVR_CAPI_GL.h"

#include "openvr.h"
#include <GL/glew.h>

#include "REV_Assert.h"
#include "REV_Common.h"

GLboolean glewInitialized = GL_FALSE;

GLuint g_MirrorProgram = 0;

const GLchar g_MirrorVS[] = {
	"#version 150"
	"in int gl_VertexID"
	"out vec2 tex"
	"const vec2 data[4] = vec2[]"
	"("
	"	vec2(-1.0,  1.0),"
	"	vec2(-1.0, -1.0),"
	"	vec2(1.0,  1.0),"
	"	vec2(1.0, -1.0)"
	");"
	"void main()"
	"{"
	"	tex = vec2(gl_VertexID & 1,gl_VertexID >> 1);"
	"	gl_Position = vec4(data[ gl_VertexID ], 0.0, 1.0);"
	"}"
};

const GLchar g_MirrorPS[] = {
	"#version 150"
	"uniform sampler2D leftEye"
	"uniform sampler2D rightEye"
	"in vec2 tex"
	"out vec4 color"
	"void main()"
	"{"
	"	tex.x *= 2.0f;"
	"	if (uv.x < 1.0f)"
	"	{"
	"		color = tex2D(leftEye, tex);"
	"	}"
	"	else"
	"	{"
	"		tex.x -= 1.0f;"
	"		color = tex2D(rightEye, tex);"
	"	}"
	"	color = vec4(1.0f, 0.0f, 0.0f, 1.0f);"
	"}"
};

GLenum ovr_TextureFormatToInternalFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_UNKNOWN:				return GL_RGBA8;
		case OVR_FORMAT_B5G6R5_UNORM:			return GL_RGB565;
		case OVR_FORMAT_B5G5R5A1_UNORM:			return GL_RGB5_A1;
		case OVR_FORMAT_B4G4R4A4_UNORM:			return GL_RGBA4;
		case OVR_FORMAT_R8G8B8A8_UNORM:			return GL_RGBA8;
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:	return GL_SRGB8_ALPHA8;
		case OVR_FORMAT_B8G8R8A8_UNORM:			return GL_RGBA8;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:	return GL_SRGB8_ALPHA8;
		case OVR_FORMAT_B8G8R8X8_UNORM:			return GL_RGBA8;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:	return GL_SRGB8_ALPHA8;
		case OVR_FORMAT_R16G16B16A16_FLOAT:		return GL_RGBA16F;
		case OVR_FORMAT_D16_UNORM:				return GL_DEPTH_COMPONENT16;
		case OVR_FORMAT_D24_UNORM_S8_UINT:		return GL_DEPTH24_STENCIL8;
		case OVR_FORMAT_D32_FLOAT:				return GL_DEPTH_COMPONENT32F;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT:	return GL_DEPTH32F_STENCIL8;
		default: return GL_RGBA8;
	}
}

GLenum ovr_TextureFormatToGLFormat(ovrTextureFormat format)
{
	switch (format)
	{
		case OVR_FORMAT_UNKNOWN:				return GL_RGBA;
		case OVR_FORMAT_B5G6R5_UNORM:			return GL_BGR;
		case OVR_FORMAT_B5G5R5A1_UNORM:			return GL_BGRA;
		case OVR_FORMAT_B4G4R4A4_UNORM:			return GL_BGRA;
		case OVR_FORMAT_R8G8B8A8_UNORM:			return GL_RGBA;
		case OVR_FORMAT_R8G8B8A8_UNORM_SRGB:	return GL_RGBA;
		case OVR_FORMAT_B8G8R8A8_UNORM:			return GL_BGRA;
		case OVR_FORMAT_B8G8R8A8_UNORM_SRGB:	return GL_BGRA;
		case OVR_FORMAT_B8G8R8X8_UNORM:			return GL_BGRA;
		case OVR_FORMAT_B8G8R8X8_UNORM_SRGB:	return GL_BGRA;
		case OVR_FORMAT_R16G16B16A16_FLOAT:		return GL_RGBA;
		case OVR_FORMAT_D16_UNORM:				return GL_DEPTH_COMPONENT;
		case OVR_FORMAT_D24_UNORM_S8_UINT:		return GL_DEPTH_STENCIL;
		case OVR_FORMAT_D32_FLOAT:				return GL_DEPTH_COMPONENT;
		case OVR_FORMAT_D32_FLOAT_S8X24_UINT:	return GL_DEPTH_STENCIL;
		default: return GL_RGBA;
	}
}

GLenum REV_GlewInit()
{
	if (!glewInitialized)
	{
		glewExperimental = GL_TRUE;
		GLenum nGlewError = glewInit();
		if (nGlewError != GLEW_OK)
			return nGlewError;
		glGetError(); // to clear the error caused deep in GLEW
		glewInitialized = GL_TRUE;
	}
	return GLEW_OK;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateTextureSwapChainGL(ovrSession session,
                                                            const ovrTextureSwapChainDesc* desc,
                                                            ovrTextureSwapChain* out_TextureSwapChain)
{
	if (REV_GlewInit() != GLEW_OK)
		return ovrError_RuntimeException;

	if (!desc || !out_TextureSwapChain || desc->Type != ovrTexture_2D)
		return ovrError_InvalidParameter;

	GLenum internalFormat = ovr_TextureFormatToInternalFormat(desc->Format);
	GLenum format = ovr_TextureFormatToGLFormat(desc->Format);

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData();
	swapChain->length = 2;
	swapChain->index = 0;
	swapChain->desc = *desc;

	for (int i = 0; i < swapChain->length; i++)
	{
		swapChain->texture[i].eType = vr::API_OpenGL;
		swapChain->texture[i].eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
		glGenTextures(1, (GLuint*)&swapChain->texture[i].handle);
		glBindTexture(GL_TEXTURE_2D, (GLuint)swapChain->texture[i].handle);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc->Width, desc->Height, 0, format, GL_UNSIGNED_BYTE, nullptr);
	}
	swapChain->current = swapChain->texture[swapChain->index];

	// Clean up and return
	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetTextureSwapChainBufferGL(ovrSession session,
                                                               ovrTextureSwapChain chain,
                                                               int index,
                                                               unsigned int* out_TexId)
{
	*out_TexId = (GLuint)chain->texture[index].handle;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_CreateMirrorTextureGL(ovrSession session,
                                                         const ovrMirrorTextureDesc* desc,
                                                         ovrMirrorTexture* out_MirrorTexture)
{
	if (REV_GlewInit() != GLEW_OK)
		return ovrError_RuntimeException;

	if (!desc || !out_MirrorTexture)
		return ovrError_InvalidParameter;

	GLenum internalFormat = ovr_TextureFormatToInternalFormat(desc->Format);
	GLenum format = ovr_TextureFormatToGLFormat(desc->Format);

	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData();
	mirrorTexture->texture.eType = vr::API_OpenGL;
	mirrorTexture->texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
	glGenTextures(1, (GLuint*)&mirrorTexture->texture.handle);
	glBindTexture(GL_TEXTURE_2D, (GLuint)mirrorTexture->texture.handle);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc->Width, desc->Height, 0, format, GL_UNSIGNED_BYTE, nullptr);

	// Clean up and return
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetMirrorTextureBufferGL(ovrSession session,
                                                            ovrMirrorTexture mirrorTexture,
                                                            unsigned int* out_TexId)
{
	if (!g_MirrorProgram)
	{
		const char* src[] = { g_MirrorVS, g_MirrorPS };
		g_MirrorProgram = glCreateProgram();

		GLuint vs = glCreateShader(GL_VERTEX_SHADER);
		glShaderSource(vs, 1, src, NULL);
		glCompileShader(vs);
		glAttachShader(g_MirrorProgram, vs);

		GLuint ps = glCreateShader(GL_FRAGMENT_SHADER);
		glShaderSource(ps, 1, src + 1, NULL);
		glCompileShader(ps);
		glAttachShader(g_MirrorProgram, ps);

		glLinkProgram(g_MirrorProgram);
		glDeleteShader(vs);
		glDeleteShader(ps);
		glUseProgram(g_MirrorProgram);

		GLint left = glGetUniformLocation(g_MirrorProgram, "leftEye");
		GLint right = glGetUniformLocation(g_MirrorProgram, "rightEye");
		glUniform1i(left, 0);
		glUniform1i(right, 1);
	}

	// Only draw mirror texture if both views have been set.
	if (session->ColorTexture[0] && session->ColorTexture[1])
	{
		glBindVertexArray(0);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, (GLuint)session->ColorTexture[1]->current.handle);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, (GLuint)session->ColorTexture[0]->current.handle);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	}

	*out_TexId = (GLuint)mirrorTexture->texture.handle;
	return ovrSuccess;
}
