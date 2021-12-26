#if MICROPROFILE_ENABLED
#include "ProfileManager.h"
#include "TextureBase.h"
#include "TextureVk.h"
#include "REV_Math.h"
#include "microprofile.h"
#include "microprofiledraw.h"
#include "microprofileui.h"

#include <assert.h>
#include <glad/glad.h>
#include <glad/glad_wgl.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>

ProfileManager::ProfileManager()
	: m_ProfileWindow(nullptr)
	, m_ProfileOverlay(vr::k_ulOverlayHandleInvalid)
	, m_Texture(nullptr)
	, m_Target()
	, m_MousePos()
	, m_MouseButtons(0)
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

	if (m_ProfileOverlay == vr::k_ulOverlayHandleInvalid)
	{
		vr::VROverlay()->CreateOverlay("revive.debug.microprofile", "Revive Microprofile", &m_ProfileOverlay);
		vr::VROverlay()->SetOverlayWidthInMeters(m_ProfileOverlay, 0.5f);
		vr::VROverlay()->SetOverlayInputMethod(m_ProfileOverlay, vr::VROverlayInputMethod_Mouse);
		vr::VROverlay()->SetOverlayFlag(m_ProfileOverlay, vr::VROverlayFlags_VisibleInDashboard, true);
		vr::VROverlay()->SetOverlayFlag(m_ProfileOverlay, vr::VROverlayFlags_SortWithNonSceneOverlays, true);
		vr::VROverlay()->SetOverlayFlag(m_ProfileOverlay, vr::VROverlayFlags_SendVRDiscreteScrollEvents, true);

		int width, height;
		glfwGetFramebufferSize(m_ProfileWindow, &width, &height);
		vr::HmdVector2_t vecWindowSize = { (float)width, (float)height };
		vr::VROverlay()->SetOverlayMouseScale(m_ProfileOverlay, &vecWindowSize);
	}

	MicroProfileSetDisplayMode(1);
	wglMakeCurrent(dc, ctx);
	return true;
}

void* ProfileManager::GetContext()
{
	return glfwGetWGLContext(m_ProfileWindow);
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
	if (!m_ProfileWindow)
		return false;

	HDC dc = wglGetCurrentDC();
	HGLRC ctx = wglGetCurrentContext();
	glfwMakeContextCurrent(m_ProfileWindow);

	if (m_Texture)
	{
		m_Texture->DeleteSharedTextureGL(m_Target);
		m_Target = 0;
		m_Texture = nullptr;
	}

	bool result = true;
	if (pTexture)
	{
		result = pTexture->CreateSharedTextureGL(&m_Target);
		if (result)
			m_Texture = pTexture;
	}
	wglMakeCurrent(dc, ctx);
	return result;
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

	if (m_Texture)
	{
		m_Texture->LockSharedTexture();

		glBindTexture(GL_TEXTURE_2D, m_Target);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);

		m_Texture->UnlockSharedTexture();
	}

	glfwSwapBuffers(m_ProfileWindow);
	glfwPollEvents();

	if (m_Texture)
	{
		static const vr::HmdMatrix34_t matrix = REV::Matrix4f(OVR::Matrix4f::RotationX(MATH_FLOAT_PI) * OVR::Matrix4f::RotationY(-MATH_FLOAT_PIOVER2) *
			OVR::Matrix4f::Translation(OVR::Vector3f(0.0f, 0.0f, 0.1f)));
		vr::TrackedDeviceIndex_t device = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
		vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(m_ProfileOverlay, device, &matrix);

		vr::Texture_t texture;
		m_Texture->ToVRTexture(texture);
		if (texture.eType != vr::TextureType_OpenGL)
		{
			static const vr::VRTextureBounds_t bounds = { 0.0f, 1.0f, 1.0f, 0.0f };
			vr::VROverlay()->SetOverlayTextureBounds(m_ProfileOverlay, &bounds);
		}

		vr::VROverlay()->SetOverlayTexture(m_ProfileOverlay, &texture);

		if (MicroProfileIsDrawing())
			vr::VROverlay()->ShowOverlay(m_ProfileOverlay);
		else
			vr::VROverlay()->HideOverlay(m_ProfileOverlay);
	}

	wglMakeCurrent(dc, ctx);
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
