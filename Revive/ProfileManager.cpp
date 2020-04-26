#if MICROPROFILE_ENABLED
#include "ProfileManager.h"
#include "TextureBase.h"
#include "REV_Math.h"
#include "microprofile.h"
#include "microprofiledraw.h"
#include "microprofileui.h"

#include <assert.h>
#include <glad\glad.h>
#include <glad\glad_wgl.h>
#include <GLFW\glfw3.h>
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW\glfw3native.h>
#include <d3d11.h>

ProfileManager::ProfileManager()
	: m_ProfileWindow(nullptr)
	, m_ProfileOverlay(vr::k_ulOverlayHandleInvalid)
	, m_Texture()
	, m_Target()
	, m_MousePos()
	, m_MouseButtons(0)
	, m_hInteropDevice()
{
}

ProfileManager::~ProfileManager()
{
}

bool ProfileManager::Initialize()
{
	if (!glfwInit())
		return false;

	HDC dc = wglGetCurrentDC();
	HGLRC ctx = wglGetCurrentContext();

	if (!m_ProfileWindow)
	{
		m_ProfileWindow = glfwCreateWindow(PROFILE_WINDOW_WIDTH, PROFILE_WINDOW_HEIGHT, "Revive Microprofile", NULL, NULL);
		if (!m_ProfileWindow)
			return false;

		glfwSetWindowAttrib(m_ProfileWindow, GLFW_RESIZABLE, false);
		glfwSetMouseButtonCallback(m_ProfileWindow, MouseButtonCallback);
		glfwSetCursorPosCallback(m_ProfileWindow, CursorPosCallback);
		glfwSetScrollCallback(m_ProfileWindow, ScrollCallback);

		glfwMakeContextCurrent(m_ProfileWindow);
		if (!gladLoadGL() || !gladLoadWGL(wglGetCurrentDC()))
			return false;

		MicroProfileDrawInitGL();
		assert(glGetError() == 0);
	}
	else
	{
		glfwMakeContextCurrent(m_ProfileWindow);
	}

	int width, height;
	glfwGetFramebufferSize(m_ProfileWindow, &width, &height);

	if (m_ProfileOverlay == vr::k_ulOverlayHandleInvalid)
	{
		vr::VROverlay()->CreateOverlay("revive.debug.microprofile", "Revive Microprofile", &m_ProfileOverlay);
		vr::VROverlay()->SetOverlayWidthInMeters(m_ProfileOverlay, 0.5f);
		vr::VROverlay()->SetOverlayInputMethod(m_ProfileOverlay, vr::VROverlayInputMethod_Mouse);
		vr::VROverlay()->SetOverlayFlag(m_ProfileOverlay, vr::VROverlayFlags_MakeOverlaysInteractiveIfVisible, true);
		vr::VROverlay()->SetOverlayFlag(m_ProfileOverlay, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);

		vr::HmdVector2_t vecWindowSize = { (float)width, (float)height };
		vr::VROverlay()->SetOverlayMouseScale(m_ProfileOverlay, &vecWindowSize);
	}

	MicroProfileSetDisplayMode(1);
	wglMakeCurrent(dc, ctx);
	return true;
}

void ProfileManager::Shutdown()
{
	if (m_ProfileOverlay != vr::k_ulOverlayHandleInvalid)
		vr::VROverlay()->DestroyOverlay(m_ProfileOverlay);
	m_ProfileOverlay = vr::k_ulOverlayHandleInvalid;
	SetTexture(nullptr);
}

bool ProfileManager::SetTexture(TextureBase* pTexture)
{
	HDC dc = wglGetCurrentDC();
	HGLRC ctx = wglGetCurrentContext();

	if (m_hInteropDevice)
	{
		glfwMakeContextCurrent(m_ProfileWindow);
		if (m_hInteropTarget)
			wglDXUnregisterObjectNV(m_hInteropDevice, m_hInteropTarget);
		wglDXCloseDeviceNV(m_hInteropDevice);
		glDeleteTextures(1, &m_Target);
		m_hInteropTarget = nullptr;
		m_hInteropDevice = nullptr;
		wglMakeCurrent(dc, ctx);
	}

	if (!pTexture)
	{
		m_Target = 0;
		m_Texture.handle = nullptr;
		return true;
	}

	pTexture->ToVRTexture(m_Texture);

	if (m_Texture.eType == vr::TextureType_OpenGL)
	{
		m_Target = (GLuint)reinterpret_cast<uintptr_t>(m_Texture.handle);
		return wglShareLists(glfwGetWGLContext(m_ProfileWindow), ctx);
	}
	else if (m_Texture.eType == vr::TextureType_DirectX)
	{
		glfwMakeContextCurrent(m_ProfileWindow);

		ID3D11Texture2D* pTexture2D = (ID3D11Texture2D*)m_Texture.handle;
		ID3D11Device* pDevice;
		pTexture2D->GetDevice(&pDevice);

		glGenTextures(1, &m_Target);
		m_hInteropDevice = wglDXOpenDeviceNV(pDevice);
		if (m_hInteropDevice)
			m_hInteropTarget = wglDXRegisterObjectNV(m_hInteropDevice, pTexture2D, m_Target, GL_TEXTURE_2D, WGL_ACCESS_READ_WRITE_NV);
		wglMakeCurrent(dc, ctx);
		return m_hInteropDevice != nullptr;
	}

	return false;
}

void ProfileManager::Flip()
{
	// Flip the profiler.
	MicroProfileFlip();

	if (!m_ProfileWindow)
		return;

	int width, height;
	glfwGetFramebufferSize(m_ProfileWindow, &width, &height);
	HDC dc = wglGetCurrentDC();
	HGLRC ctx = wglGetCurrentContext();

	glfwMakeContextCurrent(m_ProfileWindow);
	glClear(GL_COLOR_BUFFER_BIT);

	int nWheelDelta = 0;
	vr::VREvent_t VREvent;
	while (vr::VROverlay()->PollNextOverlayEvent(m_ProfileOverlay, &VREvent, sizeof(VREvent)))
	{
		switch (VREvent.eventType)
		{
		case vr::VREvent_MouseMove:
			m_MousePos.v[0] = VREvent.data.mouse.x;
			m_MousePos.v[1] = VREvent.data.mouse.y;
			MicroProfileMousePosition((uint32_t)m_MousePos.v[0], (uint32_t)m_MousePos.v[1], nWheelDelta);
			break;
		case vr::VREvent_ScrollDiscrete:
			nWheelDelta = int(-VREvent.data.scroll.ydelta * (float)height);
			MicroProfileMousePosition((uint32_t)m_MousePos.v[0], (uint32_t)m_MousePos.v[1], nWheelDelta);
			break;
		case vr::VREvent_MouseButtonDown:
			m_MouseButtons |= VREvent.data.mouse.button;
			MicroProfileMouseButton(!!(m_MouseButtons & vr::VRMouseButton_Left), !!(m_MouseButtons & vr::VRMouseButton_Right));
			break;
		case vr::VREvent_MouseButtonUp:
			m_MouseButtons &= ~VREvent.data.mouse.button;
			MicroProfileMouseButton(!!(m_MouseButtons & vr::VRMouseButton_Left), !!(m_MouseButtons & vr::VRMouseButton_Right));
			break;
		}
	}

	MicroProfileRender(width, height, 1.0f);

	if (m_Target)
	{
		if (m_hInteropDevice)
			wglDXLockObjectsNV(m_hInteropDevice, 1, &m_hInteropTarget);

		glBindTexture(GL_TEXTURE_2D, m_Target);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

		if (m_hInteropDevice)
			wglDXUnlockObjectsNV(m_hInteropDevice, 1, &m_hInteropTarget);
	}

	glfwSwapBuffers(m_ProfileWindow);
	glfwPollEvents();

	wglMakeCurrent(dc, ctx);

	if (m_Texture.handle)
	{
		vr::HmdMatrix34_t matrix = REV::Matrix4f(OVR::Matrix4f::RotationX(MATH_FLOAT_PI) * OVR::Matrix4f::RotationY(-MATH_FLOAT_PIOVER2));
		vr::TrackedDeviceIndex_t device = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
		vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(m_ProfileOverlay, device, &matrix);

		if (m_Texture.eType != vr::TextureType_OpenGL)
		{
			vr::VRTextureBounds_t bounds = { 0.0f, 1.0f, 1.0f, 0.0f };
			vr::VROverlay()->SetOverlayTextureBounds(m_ProfileOverlay, &bounds);
		}

		vr::VROverlay()->SetOverlayTexture(m_ProfileOverlay, &m_Texture);

		if (MicroProfileIsDrawing())
			vr::VROverlay()->ShowOverlay(m_ProfileOverlay);
		else
			vr::VROverlay()->HideOverlay(m_ProfileOverlay);
	}

}

void ProfileManager::MouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS && !MicroProfileIsDrawing())
		MicroProfileSetDisplayMode(1);

	MicroProfileMouseButton(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT), glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT));
}

void ProfileManager::CursorPosCallback(GLFWwindow* window, double xpos, double ypos)
{
	// TODO: Can't scroll and move the cursor at the same time
	MicroProfileMousePosition((uint32_t)xpos, (uint32_t)ypos, 0);
}

void ProfileManager::ScrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
	double xpos, ypos;
	glfwGetCursorPos(window, &xpos, &ypos);
	MicroProfileMousePosition((uint32_t)xpos, (uint32_t)ypos, (int)-yoffset);
}
#endif
