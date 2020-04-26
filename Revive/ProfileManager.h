#pragma once

#include <openvr.h>

#ifdef MICROPROFILE_ENABLED

#define PROFILE_WINDOW_WIDTH 800
#define PROFILE_WINDOW_HEIGHT 600

class ProfileManager
{
public:
	ProfileManager();
	~ProfileManager();

	bool Initialize();
	void Shutdown();

	bool SetTexture(class TextureBase* pTexture);
	void Flip();

private:
	static void MouseButtonCallback(struct GLFWwindow* window, int button, int action, int mods);
	static void CursorPosCallback(struct GLFWwindow* window, double xpos, double ypos);
	static void ScrollCallback(struct GLFWwindow* window, double xoffset, double yoffset);

	struct GLFWwindow* m_ProfileWindow;
	vr::VROverlayHandle_t m_ProfileOverlay;
	vr::Texture_t m_Texture;
	unsigned int m_Target;

	vr::HmdVector2_t m_MousePos;
	uint32_t m_MouseButtons;

	void* m_hInteropDevice;
	void* m_hInteropTarget;
};
#endif
