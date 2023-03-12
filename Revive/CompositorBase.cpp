#include "CompositorBase.h"
#include "Common.h"
#include "Session.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "ProfileManager.h"
#include "TextureBase.h"

#include "microprofile.h"
#include "OVR_CAPI.h"
#include "REV_Math.h"

#include <Windows.h>
#include <assert.h>
#include <openvr.h>
#include <vector>
#include <algorithm>

#define REV_LAYER_BIAS 0.0001f
#define ENABLE_DEPTH_SUBMIT 0

MICROPROFILE_DEFINE(WaitToBeginFrame, "Compositor", "WaitFrame", 0x00ff00);
MICROPROFILE_DEFINE(BeginFrame, "Compositor", "BeginFrame", 0x00ff00);
MICROPROFILE_DEFINE(EndFrame, "Compositor", "EndFrame", 0x00ff00);
MICROPROFILE_DEFINE(BlitLayers, "Compositor", "BlitLayers", 0x00ff00);
MICROPROFILE_DEFINE(SubmitLayer, "Compositor", "SubmitLayer", 0x00ff00);
MICROPROFILE_DEFINE(WaitGetPoses, "Compositor", "WaitGetPoses", 0x00ff00);
MICROPROFILE_DEFINE(PostPresentHandoff, "Compositor", "PostPresentHandoff", 0x00ff00);

ovrResult CompositorBase::CompositorErrorToOvrError(vr::EVRCompositorError error)
{
	switch (error)
	{
	case vr::VRCompositorError_None: return ovrSuccess;
	case vr::VRCompositorError_RequestFailed: return ovrError_InvalidOperation;
	case vr::VRCompositorError_IncompatibleVersion: return ovrError_ServiceError;
	case vr::VRCompositorError_DoNotHaveFocus: return ovrSuccess_NotVisible;
	case vr::VRCompositorError_InvalidTexture: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_IsNotSceneApplication: return ovrError_InvalidSession;
	case vr::VRCompositorError_TextureIsOnWrongDevice: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_TextureUsesUnsupportedFormat: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_SharedTexturesNotSupported: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_IndexOutOfRange: return ovrError_InvalidParameter;
	case vr::VRCompositorError_AlreadySubmitted: return ovrSuccess_NotVisible;
	case vr::VRCompositorError_InvalidBounds: return ovrError_InvalidParameter;
	default: return ovrError_RuntimeException;
	}
}

CompositorBase::CompositorBase()
	: m_ChainCount(0)
	, m_MirrorTexture(nullptr)
	, m_OverlayCount(0)
	, m_ActiveOverlays()
	, m_FrameEvent()
	, m_Timeout(100)
	, m_TimingMode(vr::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff)
#if MICROPROFILE_ENABLED
	, m_ProfileTexture()
#endif
{
	// We want to handle all graphics tasks explicitly instead of implicitly letting WaitGetPoses execute them
	vr::VRCompositor()->SetExplicitTimingMode(vr::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff);

	// Create the frame completion event
	m_FrameEvent = CreateEvent(nullptr, true, true, nullptr);
	m_Timeout = (DWORD)ceilf(1000.0f / vr::VRSystem()->GetFloatTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_DisplayFrequency_Float));
}

CompositorBase::~CompositorBase()
{
	CloseHandle(m_FrameEvent);

	if (m_MirrorTexture)
		delete m_MirrorTexture;

#if MICROPROFILE_ENABLED
	g_ProfileManager.SetTexture(nullptr);
#endif
}

ovrResult CompositorBase::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
#if MICROPROFILE_ENABLED
	if (!m_ProfileTexture)
	{
		// Create the profiler render target texture.
		m_ProfileTexture.reset(CreateTexture());
		m_ProfileTexture->Init(ovrTexture_2D, PROFILE_WINDOW_WIDTH, PROFILE_WINDOW_HEIGHT, 1, 1, OVR_FORMAT_R8G8B8A8_UNORM, ovrTextureMisc_None, ovrTextureBind_DX_RenderTarget);
		g_ProfileManager.SetTexture(m_ProfileTexture.get());
	}
#endif

	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(*desc);
	swapChain->Identifier = m_ChainCount++;

	if (desc->StaticImage)
		swapChain->Length = 1;

	for (int i = 0; i < swapChain->Length; i++)
	{
		TextureBase* texture = CreateTexture();
		bool success = texture->Init(desc->Type, desc->Width, desc->Height, desc->MipLevels,
			desc->ArraySize, desc->Format, desc->MiscFlags, desc->BindFlags);
		if (!success)
			return ovrError_RuntimeException;
		swapChain->Textures[i].reset(texture);
	}

	*out_TextureSwapChain = swapChain;
	return ovrSuccess;
}

ovrResult CompositorBase::CreateMirrorTexture(const ovrMirrorTextureDesc* desc, ovrMirrorTexture* out_MirrorTexture)
{
	// There can only be one mirror texture at a time
	if (m_MirrorTexture)
		return ovrError_RuntimeException;

	// TODO: Support ovrMirrorOptions
	ovrMirrorTexture mirrorTexture = new ovrMirrorTextureData(*desc);
	TextureBase* texture = CreateTexture();
	bool success = texture->Init(ovrTexture_2D, desc->Width, desc->Height, 1, 1, desc->Format,
		desc->MiscFlags | ovrTextureMisc_AllowGenerateMips, ovrTextureBind_DX_RenderTarget);
	if (!success)
		return ovrError_RuntimeException;
	mirrorTexture->Texture.reset(texture);

	m_MirrorTexture = mirrorTexture;
	*out_MirrorTexture = mirrorTexture;
	return ovrSuccess;
}

ovrResult CompositorBase::WaitToBeginFrame(ovrSession session, long long frameIndex)
{
	MICROPROFILE_SCOPE(WaitToBeginFrame);

	// Keep waiting for the targeted frame as long as we don't timeout
	while (session->FrameIndex < frameIndex - 1)
	{
		// Wait on the last frame completion
		if (WaitForSingleObject(m_FrameEvent, m_Timeout) != WAIT_OBJECT_0)
			break;
	}

	// Wait for the actual frame start
	MICROPROFILE_SCOPE(WaitGetPoses);
	return CompositorErrorToOvrError(vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0));
}

ovrResult CompositorBase::BeginFrame(ovrSession session, long long frameIndex)
{
	MICROPROFILE_SCOPE(BeginFrame);

	// Reset the frame completion event
	ResetEvent(m_FrameEvent);

	return CompositorErrorToOvrError(vr::VRCompositor()->SubmitExplicitTimingData());
}

const ovrLayer_Union& CompositorBase::ToUnion(const ovrLayerHeader* layerPtr)
{
	// Version 1.25 introduced a 128-byte reserved parameter, so on older versions the actual data
	// falls within this reserved parameter so the pointer needs to be moved back.
	if (g_MinorVersion < 25)
		return *(ovrLayer_Union*)((uint8_t*)layerPtr - sizeof(ovrLayerHeader::Reserved));
	else
		return *(ovrLayer_Union*)layerPtr;
}

ovrResult CompositorBase::EndFrame(ovrSession session, long long frameIndex, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	MICROPROFILE_SCOPE(EndFrame);

	if (layerCount == 0 || !layerPtrList || frameIndex < session->FrameIndex)
		return ovrError_InvalidParameter;

	const ovrLayerHeader* baseLayer = nullptr;
	std::vector<vr::VROverlayHandle_t> activeOverlays;
	for (uint32_t i = 0; i < layerCount; i++)
	{
		if (!layerPtrList[i])
			continue;

		// TODO: Support ovrLayerType_Cylinder and ovrLayerType_Cube
		if (layerPtrList[i]->Type == ovrLayerType_Quad)
		{
			const ovrLayerQuad& layer = ToUnion(layerPtrList[i]).Quad;
			ovrTextureSwapChain chain = layer.ColorTexture;

			// Every overlay is associated with a swapchain.
			// This is necessary because the position of the layer may change in the array,
			// which would otherwise cause flickering between overlays.
			// TODO: Support multiple overlays using the same texture.
			if (chain->Overlay == vr::k_ulOverlayHandleInvalid)
			{
				chain->Overlay = CreateOverlay();

				// Submit for the first time
				vr::Texture_t texture;
				chain->Submit()->ToVRTexture(texture);
				vr::VROverlay()->SetOverlayTexture(chain->Overlay, &texture);
			}
			activeOverlays.push_back(chain->Overlay);

			// Set the layer rendering order and apply a bias between the layers.
			vr::VROverlay()->SetOverlaySortOrder(chain->Overlay, i);
			OVR::Posef pose = layer.QuadPoseCenter;
			pose.Translation += pose.Rotate(OVR::Vector3f(0.0f, 0.0f, (float)i * REV_LAYER_BIAS));

			// Transform the overlay.
			vr::HmdMatrix34_t transform = REV::Matrix4f(pose);
			vr::VROverlay()->SetOverlayWidthInMeters(chain->Overlay, layer.QuadSize.x);
			if (layerPtrList[i]->Flags & ovrLayerFlag_HeadLocked)
				vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(chain->Overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform);
			else
				vr::VROverlay()->SetOverlayTransformAbsolute(chain->Overlay, session->TrackingOrigin, &transform);

			// Set the texture and show the overlay.
			vr::VRTextureBounds_t bounds = ViewportToTextureBounds(layer.Viewport, layer.ColorTexture, layerPtrList[i]->Flags);
			vr::VROverlay()->SetOverlayTextureBounds(chain->Overlay, &bounds);

			// Show the overlay, unfortunately we have no control over the order in which
			// overlays are drawn.
			// TODO: Support ovrLayerFlag_HighQuality for overlays with anisotropic sampling.
			// TODO: Handle overlay errors.
			vr::VROverlay()->ShowOverlay(chain->Overlay);
		}
		else if (layerPtrList[i]->Type == ovrLayerType_EyeFov ||
			layerPtrList[i]->Type == ovrLayerType_EyeFovDepth ||
			layerPtrList[i]->Type == ovrLayerType_EyeFovMultires ||
			layerPtrList[i]->Type == ovrLayerType_EyeMatrix)
		{
			// We can only submit one eye layer, so once we have a base layer we blit the others.
			if (!baseLayer)
				baseLayer = layerPtrList[i];
			else
				BlitLayers(baseLayer, layerPtrList[i]);
		}
	}

	// Hide previous overlays that are not part of the current layers.
	for (vr::VROverlayHandle_t overlay : m_ActiveOverlays)
	{
		// Find the overlay in the current active overlays, if it was not found then hide it.
		// TODO: Handle overlay errors.
		if (std::find(activeOverlays.begin(), activeOverlays.end(), overlay) == activeOverlays.end())
			vr::VROverlay()->HideOverlay(overlay);
	}
	m_ActiveOverlays = activeOverlays;

	vr::EVRCompositorError error = vr::VRCompositorError_None;
	if (baseLayer)
		error = SubmitLayer(session, baseLayer);

	if (m_TimingMode == vr::VRCompositorTimingMode_Explicit_ApplicationPerformsPostPresentHandoff)
	{
		MICROPROFILE_SCOPE(PostPresentHandoff);
		vr::VRCompositor()->PostPresentHandoff();
	}
	else if (error == vr::VRCompositorError_None)
	{
		MICROPROFILE_SCOPE(WaitGetPoses);
		error = vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
	}

	// Frame now completed so we can let anyone waiting on the next frame call WaitGetPoses
	session->FrameIndex = frameIndex;
	if (m_TimingMode != vr::VRCompositorTimingMode_Implicit)
		SetEvent(m_FrameEvent);

	if (m_MirrorTexture && error == vr::VRCompositorError_None)
		RenderMirrorTexture(m_MirrorTexture);

#if MICROPROFILE_ENABLED
	g_ProfileManager.Flip();
#endif

	return CompositorErrorToOvrError(error);
}

vr::VROverlayHandle_t CompositorBase::CreateOverlay()
{
	// Each overlay needs a unique key, so just count how many overlays we've created until now.
	char keyName[vr::k_unVROverlayMaxKeyLength];
	snprintf(keyName, vr::k_unVROverlayMaxKeyLength, "revive.runtime.layer%d", m_OverlayCount++);

	vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
	vr::VROverlay()->CreateOverlay((const char*)keyName, "Revive Layer", &handle);
	return handle;
}

vr::VRTextureBounds_t CompositorBase::ViewportToTextureBounds(ovrRecti viewport, ovrTextureSwapChain swapChain, unsigned int flags)
{
	vr::VRTextureBounds_t bounds;
	float w = (float)swapChain->Desc.Width;
	float h = (float)swapChain->Desc.Height;
	bounds.uMin = viewport.Pos.x / w;
	bounds.vMin = viewport.Pos.y / h;

	// Sanity check for the viewport size.
	// Workaround for Defense Grid 2, which leaves these variables uninitialized.
	if (viewport.Size.w > 0 && viewport.Size.h > 0)
	{
		bounds.uMax = (viewport.Pos.x + viewport.Size.w) / w;
		bounds.vMax = (viewport.Pos.y + viewport.Size.h) / h;
	}
	else
	{
		bounds.uMax = 1.0f;
		bounds.vMax = 1.0f;
	}

	if (flags & ovrLayerFlag_TextureOriginAtBottomLeft)
	{
		bounds.vMin = 1.0f - bounds.vMin;
		bounds.vMax = 1.0f - bounds.vMax;
	}

	if (GetAPI() == vr::TextureType_OpenGL)
	{
		bounds.vMin = 1.0f - bounds.vMin;
		bounds.vMax = 1.0f - bounds.vMax;
	}

	return bounds;
}

void CompositorBase::BlitLayers(const ovrLayerHeader* dstLayer, const ovrLayerHeader* srcLayer)
{
	MICROPROFILE_SCOPE(BlitLayers);

	const ovrLayer_Union& dst = ToUnion(dstLayer);
	const ovrLayer_Union& src = ToUnion(srcLayer);

	// Render the scene layer
	TextureBase* srcTex = nullptr;
	TextureBase* dstTex = nullptr;
	ovrTextureSwapChain srcChain = nullptr;
	ovrTextureSwapChain dstChain = nullptr;
	for (int i = 0; i < ovrEye_Count; i++)
	{
		if (src.EyeFov.ColorTexture[i] && src.EyeFov.ColorTexture[i] != srcChain)
		{
			srcChain = src.EyeFov.ColorTexture[i];
			srcTex = srcChain->Submit();
		}

		if (dst.EyeFov.ColorTexture[i])
		{
			dstChain = dst.EyeFov.ColorTexture[i];
			dstTex = dstChain->Textures[dstChain->SubmitIndex].get();
		}

		// Get the scene fov
		ovrFovPort srcFov = srcLayer->Type == ovrLayerType_EyeMatrix ?
			REV::Matrix4f(src.EyeMatrix.Matrix[i]).ToFovPort() : src.EyeFov.Fov[i];
		ovrFovPort dstFov = dstLayer->Type == ovrLayerType_EyeMatrix ?
			REV::Matrix4f(dst.EyeMatrix.Matrix[i]).ToFovPort() : dst.EyeFov.Fov[i];

		// Calculate the fov quad
		vr::HmdVector4_t quad;
		quad.v[0] = srcFov.LeftTan / -dstFov.LeftTan;
		quad.v[1] = srcFov.RightTan / dstFov.RightTan;
		quad.v[2] = srcFov.UpTan / dstFov.UpTan;
		quad.v[3] = srcFov.DownTan / -dstFov.DownTan;

		// Calculate the texture bounds
		vr::VRTextureBounds_t bounds = ViewportToTextureBounds(src.EyeFov.Viewport[i], srcChain, srcLayer->Flags);

		// Composit the layer
		RenderTextureSwapChain((vr::EVREye)i, srcTex, dstTex, dst.EyeFov.Viewport[i], bounds, quad);
	}

	MICROPROFILE_META_CPU("SwapChain Left", src.EyeFov.ColorTexture[0]->Identifier);
	MICROPROFILE_META_CPU("Submit Left", src.EyeFov.ColorTexture[0]->SubmitIndex);
	MICROPROFILE_META_CPU("SwapChain Right", srcChain->Identifier);
	MICROPROFILE_META_CPU("Submit Right", srcChain->SubmitIndex);
}

vr::VRCompositorError CompositorBase::SubmitLayer(ovrSession session, const ovrLayerHeader* baseLayer)
{
	MICROPROFILE_SCOPE(SubmitLayer);

	const ovrLayer_Union& layer = ToUnion(baseLayer);

	// Submit the scene layer.
	vr::VRCompositorError err;
	vr::Texture_t depthTexture;
	vr::Texture_t colorTexture;
	ovrTextureSwapChain colorChain = nullptr;
	ovrTextureSwapChain depthChain = nullptr;
	for (int i = 0; i < ovrEye_Count; i++)
	{
		if (layer.EyeFov.ColorTexture[i] && layer.EyeFov.ColorTexture[i] != colorChain)
		{
			colorChain = layer.EyeFov.ColorTexture[i];
			colorChain->Submit()->ToVRTexture(colorTexture);
		}

		ovrFovPort fov = baseLayer->Type == ovrLayerType_EyeMatrix ?
			REV::Matrix4f(layer.EyeMatrix.Matrix[i]).ToFovPort() : layer.EyeFov.Fov[i];

		vr::VRTextureBounds_t bounds = ViewportToTextureBounds(layer.EyeFov.Viewport[i], colorChain, baseLayer->Flags);

		// Get the descriptor for this eye
		const ovrEyeRenderDesc* desc = session->Details->GetRenderDesc((ovrEyeType)i);

		// Shrink the bounds to account for the overlapping fov
		vr::VRTextureBounds_t fovBounds = FovPortToTextureBounds(desc->Fov, fov);

		// Combine the fov bounds with the viewport bounds
		bounds.uMin += fovBounds.uMin * (bounds.uMax - bounds.uMin);
		bounds.uMax = bounds.uMin + fovBounds.uMax * (bounds.uMax - bounds.uMin);
		bounds.vMin += fovBounds.vMin * (bounds.vMax - bounds.vMin);
		bounds.vMax = bounds.vMin + fovBounds.vMax * (bounds.vMax - bounds.vMin);

		unsigned int submitFlags = vr::Submit_Default;
		union
		{
			vr::Texture_t Color;
			vr::VRTextureWithPose_t Pose;
			vr::VRTextureWithDepth_t Depth;
			vr::VRTextureWithPoseAndDepth_t PoseDepth;
		} texture;
		texture.Color = colorTexture;

		// Add the pose data to the eye texture
		if (!session->Details->UseHack(SessionDetails::HACK_STRICT_POSES))
		{
			OVR::Matrix4f hmdToEye(desc->HmdToEyePose);
			REV::Matrix4f pose = baseLayer->Type == ovrLayerType_EyeMatrix ?
				REV::Matrix4f(layer.EyeMatrix.RenderPose[i]) : REV::Matrix4f(layer.EyeFov.RenderPose[i]);
			if (session->TrackingOrigin == vr::TrackingUniverseSeated)
			{
				REV::Matrix4f offset(vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose());
				texture.Pose.mDeviceToAbsoluteTracking = REV::Matrix4f(offset * (pose * hmdToEye.Inverted()));
			}
			else
			{
				texture.Pose.mDeviceToAbsoluteTracking = REV::Matrix4f(pose * hmdToEye.Inverted());
			}
			submitFlags |= vr::Submit_TextureWithPose;
		}

		// Add the depth data if present in the layer
		if (baseLayer->Type == ovrLayerType_EyeFovDepth)
		{
			if (layer.EyeFovDepth.DepthTexture[i] && layer.EyeFovDepth.DepthTexture[i] != depthChain)
			{
				depthChain = layer.EyeFovDepth.DepthTexture[i];
				depthChain->Submit()->ToVRTexture(depthTexture);
			}

#if ENABLE_DEPTH_SUBMIT
			vr::VRTextureDepthInfo_t& depthInfo = submitFlags & vr::Submit_TextureWithPose ?
				texture.PoseDepth.depth : texture.Depth.depth;
			depthInfo.handle = depthTexture.handle;
			depthInfo.mProjection = REV::Matrix4f::FromProjectionDesc(layer.EyeFovDepth.ProjectionDesc, fov);
			depthInfo.vRange = REV::Vector2f(0.0f, 1.0f);
			submitFlags |= vr::Submit_TextureWithDepth;
#endif
		}

		err = vr::VRCompositor()->Submit((vr::EVREye)i, &texture.Color, &bounds, (vr::EVRSubmitFlags)submitFlags);
		if (err != vr::VRCompositorError_None)
			break;
	}

	MICROPROFILE_META_CPU("SwapChain Left", layer.EyeFov.ColorTexture[0]->Identifier);
	MICROPROFILE_META_CPU("Submit Left", layer.EyeFov.ColorTexture[0]->SubmitIndex);
	MICROPROFILE_META_CPU("SwapChain Right", colorChain->Identifier);
	MICROPROFILE_META_CPU("Submit Right", colorChain->SubmitIndex);
	return err;
}

void CompositorBase::SetTimingMode(vr::EVRCompositorTimingMode timingMode)
{
	if (timingMode != m_TimingMode)
	{
		vr::VRCompositor()->SetExplicitTimingMode(timingMode);
		m_TimingMode = timingMode;
	}
}

void CompositorBase::SetMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	m_MirrorTexture = mirrorTexture;
}

vr::VRTextureBounds_t CompositorBase::FovPortToTextureBounds(ovrFovPort eyeFov, ovrFovPort fov)
{
	vr::VRTextureBounds_t result;

	// Adjust the bounds based on the field-of-view in the game
	result.uMin = (fov.LeftTan - eyeFov.LeftTan) / (fov.LeftTan + fov.RightTan);
	result.uMax = 1.0f - (fov.RightTan - eyeFov.RightTan) / (fov.LeftTan + fov.RightTan);
	result.vMin = (fov.UpTan - eyeFov.UpTan) / (fov.UpTan + fov.DownTan);
	result.vMax = 1.0f - (fov.DownTan - eyeFov.DownTan) / (fov.UpTan + fov.DownTan);

	return result;
}
