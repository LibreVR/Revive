#include "OVR_CAPI_GL.h"

#include "openvr.h"
#include <GL/glew.h>
#include <Windows.h>

#include "REV_Assert.h"
#include "REV_Common.h"

GLboolean glewInitialized = GL_FALSE;

GLuint g_MirrorProgram = 0;

const GLchar g_MirrorVS[] = {
	"#version 150\n"
	"in int gl_VertexID;\n"
	"out vec2 uv;\n"
	"const vec2 data[4] = vec2[]\n"
	"(\n"
	"	vec2(-1.0,  1.0),\n"
	"	vec2(-1.0, -1.0),\n"
	"	vec2(1.0,  1.0),\n"
	"	vec2(1.0, -1.0)\n"
	");\n"
	"void main()\n"
	"{\n"
	"	uv = vec2(gl_VertexID & 1,gl_VertexID >> 1);\n"
	"	gl_Position = vec4(data[ gl_VertexID ], 0.0, 1.0);\n"
	"}\n"
};

const GLchar g_MirrorPS[] = {
	"#version 150\n"
	"uniform sampler2D leftEye\n;"
	"uniform sampler2D rightEye;\n"
	"in vec2 uv;\n"
	"out vec4 color;\n"
	"void main()\n"
	"{\n"
	"	vec2 tex = uv;"
	"	tex.x *= 2.0f;\n"
	"	if (tex.x < 1.0f)\n"
	"	{\n"
	"		color = texture(leftEye, tex);\n"
	"	}\n"
	"	else\n"
	"	{\n"
	"		tex.x -= 1.0f;\n"
	"		color = texture(rightEye, tex);\n"
	"	}\n"
	"	color = vec4(1.0f, 0.0f, 0.0f, 1.0f);\n"
	"}\n"
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

void DebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}



GLenum REV_GlewInit()
{
	if (!glewInitialized)
	{
		glewExperimental = GL_TRUE;
		GLenum nGlewError = glewInit();
		if (nGlewError != GLEW_OK)
			return nGlewError;
#ifdef DEBUG
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback((GLDEBUGPROC)DebugCallback, nullptr);
#endif // DEBUG
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
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
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

	GLuint texture, framebuffer;
	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData();
	mirrorTexture->texture.eType = vr::API_OpenGL;
	mirrorTexture->texture.eColorSpace = vr::ColorSpace_Auto; // TODO: Set this from the texture format.
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, desc->Width, desc->Height, 0, format, GL_UNSIGNED_BYTE, nullptr);

	glGenFramebuffers(1, &framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return ovrError_RuntimeException;

	// Clean up and return
	mirrorTexture->texture.handle = (void*)texture;
	mirrorTexture->target = (void*)framebuffer;
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

		GLint isLinked = 0;
		glLinkProgram(g_MirrorProgram);
		glGetProgramiv(g_MirrorProgram, GL_LINK_STATUS, (int *)&isLinked);
		if (isLinked == GL_FALSE)
		{
			GLint maxLength = 0;
			glGetProgramiv(g_MirrorProgram, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(g_MirrorProgram, maxLength, &maxLength, infoLog.data());
			OutputDebugStringA(infoLog.data());
		}

		glDeleteShader(vs);
		glDeleteShader(ps);
		glUseProgram(g_MirrorProgram);

		GLint left = glGetUniformLocation(g_MirrorProgram, "leftEye");
		GLint right = glGetUniformLocation(g_MirrorProgram, "rightEye");
		glUniform1i(left, 0);
		glUniform1i(right, 1);
	}
	glUseProgram(g_MirrorProgram);

	// Only draw mirror texture if both views have been set.
	// FIXME: Invalid format error.
	GLuint leftEye, rightEye;
	vr::glSharedTextureHandle_t leftHandle, rightHandle;
	session->compositor->GetMirrorTextureGL(vr::Eye_Left, &leftEye, &leftHandle);
	session->compositor->GetMirrorTextureGL(vr::Eye_Right, &rightEye, &rightHandle);

	glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)mirrorTexture->target);
	glViewport(0.0f, 0.0f, mirrorTexture->desc.Width, mirrorTexture->desc.Height);
	glBindVertexArray(0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, leftEye);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, rightEye);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);

	session->compositor->ReleaseSharedGLTexture(leftEye, leftHandle);
	session->compositor->ReleaseSharedGLTexture(rightEye, rightHandle);

	*out_TexId = (GLuint)mirrorTexture->texture.handle;
	return ovrSuccess;
}
