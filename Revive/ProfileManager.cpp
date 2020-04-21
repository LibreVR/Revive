#include "ProfileManager.h"
#include "REV_Math.h"
#include "microprofile.h"
#include "microprofiledraw.h"
#include "microprofileui.h"

#include <assert.h>
#include <glad\glad.h>
#include <GLFW\glfw3.h>

ProfileManager::ProfileManager()
	: m_ProfileWindow(nullptr)
	, m_ProfileOverlay(vr::k_ulOverlayHandleInvalid)
	, m_ProfileTexture()
	, m_MousePos()
	, m_MouseButtons(0)
{
}

ProfileManager::~ProfileManager()
{

}

bool ProfileManager::Initialize()
{
#ifdef _DEBUG
	if (!glfwInit())
		return false;

	HDC dc = wglGetCurrentDC();
	HGLRC ctx = wglGetCurrentContext();

	if (!m_ProfileWindow)
	{
		m_ProfileWindow = glfwCreateWindow(800, 600, "Revive Microprofile", NULL, NULL);
		if (!m_ProfileWindow)
			return false;

		glfwSetWindowAttrib(m_ProfileWindow, GLFW_RESIZABLE, false);
		glfwSetMouseButtonCallback(m_ProfileWindow, MouseButtonCallback);
		glfwSetCursorPosCallback(m_ProfileWindow, CursorPosCallback);
		glfwSetScrollCallback(m_ProfileWindow, ScrollCallback);

		glfwMakeContextCurrent(m_ProfileWindow);
		if (!gladLoadGL())
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

	if (!m_ProfileTexture)
	{
		glGenTextures(1, &m_ProfileTexture);
		glBindTexture(GL_TEXTURE_2D, m_ProfileTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	}

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
#endif
	return true;
}

void ProfileManager::Shutdown()
{
#ifdef _DEBUG
	if (m_ProfileOverlay != vr::k_ulOverlayHandleInvalid)
		vr::VROverlay()->DestroyOverlay(m_ProfileOverlay);
	m_ProfileOverlay = vr::k_ulOverlayHandleInvalid;

	HDC dc = wglGetCurrentDC();
	HGLRC ctx = wglGetCurrentContext();

	glfwMakeContextCurrent(m_ProfileWindow);
	glDeleteTextures(1, &m_ProfileTexture);
	m_ProfileTexture = 0;
	wglMakeCurrent(dc, ctx);
#endif
}

void ProfileManager::Flip()
{
	// Flip the profiler.
	MicroProfileFlip();

#ifdef _DEBUG
	if (!m_ProfileWindow)
		return;

	if (MicroProfileIsDrawing())
		vr::VROverlay()->ShowOverlay(m_ProfileOverlay);
	else
		vr::VROverlay()->HideOverlay(m_ProfileOverlay);

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

	vr::HmdMatrix34_t matrix = REV::Matrix4f(OVR::Matrix4f::RotationX(MATH_FLOAT_PI) * OVR::Matrix4f::RotationY(-MATH_FLOAT_PIOVER2));
	vr::TrackedDeviceIndex_t device = vr::VRSystem()->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
	vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(m_ProfileOverlay, device, &matrix);

	glBindTexture(GL_TEXTURE_2D, m_ProfileTexture);
	glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, width, height, 0);

	vr::Texture_t tex = { (void*)m_ProfileTexture, vr::TextureType_OpenGL, vr::ColorSpace_Auto };
	vr::VROverlay()->SetOverlayTexture(m_ProfileOverlay, &tex);

	glfwSwapBuffers(m_ProfileWindow);
	glfwPollEvents();

	wglMakeCurrent(dc, ctx);
#endif
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
