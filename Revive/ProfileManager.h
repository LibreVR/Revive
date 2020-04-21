#pragma once

#include <openvr.h>

class ProfileManager
{
public:
	ProfileManager();
	~ProfileManager();

	bool Initialize();
	void Shutdown();

	void Flip();

private:
	static void MouseButtonCallback(struct GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(struct GLFWwindow* window, double xpos, double ypos);
	static void ScrollCallback(struct GLFWwindow* window, double xoffset, double yoffset);

	struct GLFWwindow* m_ProfileWindow;
	vr::VROverlayHandle_t m_ProfileOverlay;
	unsigned int m_ProfileTexture;

	vr::HmdVector2_t m_MousePos;
	uint32_t m_MouseButtons;
};
