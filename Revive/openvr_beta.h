#pragma once

// TODO: Remove these definitions as soon as they're added to OpenVR
namespace vr
{
	// Set to indicate that pTexture is a pointer to a VRTextureWithPose_t.
	const EVRSubmitFlags Submit_TextureWithPose = (EVRSubmitFlags)0x08;

	/** Allows specifying pose used to render provided scene texture (if different from value returned by WaitGetPoses). */
	struct VRTextureWithPose_t : public Texture_t
	{
		HmdMatrix34_t mDeviceToAbsoluteTracking; // Actual pose used to render scene textures.
	};
}
