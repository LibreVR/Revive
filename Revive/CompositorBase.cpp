#include "CompositorBase.h"
#include "OVR_CAPI.h"
#include "REV_Math.h"
#include "Settings.h"
#include "Session.h"
#include "SessionDetails.h"
#include "microprofile.h"

#include <openvr.h>
#include <vector>
#include <algorithm>

#define REV_LAYER_BIAS 0.0001f

MICROPROFILE_DEFINE(WaitToBeginFrame, "Compositor", "WaitFrame", 0x00ff00);
MICROPROFILE_DEFINE(BeginFrame, "Compositor", "BeginFrame", 0x00ff00);
MICROPROFILE_DEFINE(EndFrame, "Compositor", "EndFrame", 0x00ff00);
MICROPROFILE_DEFINE(SubmitFovLayer, "Compositor", "SubmitFovLayer", 0x00ff00);
MICROPROFILE_DEFINE(SubmitSceneLayer, "Compositor", "SubmitSceneLayer", 0x00ff00);

ovrResult rev_CompositorErrorToOvrError(vr::EVRCompositorError error)
{
	switch (error)
	{
	case vr::VRCompositorError_None: return ovrSuccess;
	case vr::VRCompositorError_IncompatibleVersion: return ovrError_ServiceError;
	case vr::VRCompositorError_DoNotHaveFocus: return ovrSuccess_NotVisible;
	case vr::VRCompositorError_InvalidTexture: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_IsNotSceneApplication: return ovrError_InvalidSession;
	case vr::VRCompositorError_TextureIsOnWrongDevice: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_TextureUsesUnsupportedFormat: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_SharedTexturesNotSupported: return ovrError_TextureSwapChainInvalid;
	case vr::VRCompositorError_IndexOutOfRange: return ovrError_InvalidParameter;
	default: return ovrError_RuntimeException;
	}
}


CompositorBase::CompositorBase()
	: m_MirrorTexture(nullptr)
	, m_ChainCount(0)
{
	m_SceneLayer = nullptr;
}

CompositorBase::~CompositorBase()
{
	if (m_MirrorTexture)
		delete m_MirrorTexture;
}

ovrResult CompositorBase::CreateTextureSwapChain(const ovrTextureSwapChainDesc* desc, ovrTextureSwapChain* out_TextureSwapChain)
{
	ovrTextureSwapChain swapChain = new ovrTextureSwapChainData(*desc);
	swapChain->Identifier = m_ChainCount++;

	// FIXME: A bug in OpenVR causes Asynchronous Reprojection to fail with swapchains
	if (GetAPI() == vr::TextureType_OpenGL)
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

	vr::EVRCompositorError err = vr::VRCompositorError_None;
	for (long long index = session->FrameIndex; index < frameIndex; index++)
	{
		// Call WaitGetPoses to block until the running start, also known as queue-ahead in the Oculus SDK.
		err = vr::VRCompositor()->WaitGetPoses(nullptr, 0, nullptr, 0);
	}
	return rev_CompositorErrorToOvrError(err);
}

ovrResult CompositorBase::BeginFrame(ovrSession session, long long frameIndex)
{
	MICROPROFILE_SCOPE(BeginFrame);

	session->FrameIndex = frameIndex;
	return ovrSuccess;
}

ovrResult CompositorBase::EndFrame(ovrSession session, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	MICROPROFILE_SCOPE(EndFrame);

	if (layerCount == 0 || !layerPtrList)
		return ovrError_InvalidParameter;

	// Flush all pending draw calls.
	Flush();

	// Other layers are interpreted as overlays.
	std::vector<vr::VROverlayHandle_t> activeOverlays;
	for (uint32_t i = 0; i < layerCount; i++)
	{
		if (layerPtrList[i] == nullptr)
			continue;

		if (layerPtrList[i]->Type == ovrLayerType_Quad)
		{
			ovrLayerQuad* layer = (ovrLayerQuad*)layerPtrList[i];
			ovrTextureSwapChain chain = layer->ColorTexture;

			// Every overlay is associated with a swapchain.
			// This is necessary because the position of the layer may change in the array,
			// which would otherwise cause flickering between overlays.
			// TODO: Support multiple overlays using the same texture.
			vr::VROverlayHandle_t overlay = layer->ColorTexture->Overlay;
			if (overlay == vr::k_ulOverlayHandleInvalid)
			{
				overlay = CreateOverlay();
				layer->ColorTexture->Overlay = overlay;

				vr::VRTextureWithPose_t texture = chain->Textures[chain->SubmitIndex]->ToVRTexture();
				vr::VROverlay()->SetOverlayTexture(chain->Overlay, &texture);
			}
			activeOverlays.push_back(overlay);

			// Set the layer rendering order.
			vr::VROverlay()->SetOverlaySortOrder(overlay, i);

			// Transform the overlay.
			vr::HmdMatrix34_t transform = REV::Matrix4f(layer->QuadPoseCenter);
			vr::VROverlay()->SetOverlayWidthInMeters(overlay, layer->QuadSize.x);
			if (layer->Header.Flags & ovrLayerFlag_HeadLocked)
				vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform);
			else
				vr::VROverlay()->SetOverlayTransformAbsolute(overlay, vr::VRCompositor()->GetTrackingSpace(), &transform);

			// Set the texture and show the overlay.
			vr::VRTextureBounds_t bounds = ViewportToTextureBounds(layer->Viewport, layer->ColorTexture, layer->Header.Flags);
			vr::Texture_t texture = chain->Textures[chain->SubmitIndex]->ToVRTexture();
			vr::VROverlay()->SetOverlayTextureBounds(overlay, &bounds);

			// Show the overlay, unfortunately we have no control over the order in which
			// overlays are drawn.
			// TODO: Support ovrLayerFlag_HighQuality for overlays with anisotropic sampling.
			// TODO: Handle overlay errors.
			vr::VROverlay()->ShowOverlay(overlay);
			chain->Submit();
		}
		else if (layerPtrList[i]->Type == ovrLayerType_EyeFov)
		{
			ovrLayerEyeFov* sceneLayer = (ovrLayerEyeFov*)layerPtrList[i];

			if (!m_SceneLayer)
				m_SceneLayer = layerPtrList[i];
			else
				SubmitFovLayer(sceneLayer->Viewport, sceneLayer->Fov, sceneLayer->ColorTexture, sceneLayer->Header.Flags);
		}
		else if (layerPtrList[i]->Type == ovrLayerType_EyeMatrix)
		{
			ovrLayerEyeMatrix* sceneLayer = (ovrLayerEyeMatrix*)layerPtrList[i];

			ovrFovPort fov[ovrEye_Count] = {
				MatrixToFovPort(sceneLayer->Matrix[ovrEye_Left]),
				MatrixToFovPort(sceneLayer->Matrix[ovrEye_Right])
			};

			if (!m_SceneLayer)
				m_SceneLayer = layerPtrList[i];
			else
				SubmitFovLayer(sceneLayer->Viewport, fov, sceneLayer->ColorTexture, sceneLayer->Header.Flags);
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
	if (m_SceneLayer && m_SceneLayer->Type == ovrLayerType_EyeFov)
	{
		ovrLayerEyeFov* sceneLayer = (ovrLayerEyeFov*)m_SceneLayer;
		error = SubmitSceneLayer(session, sceneLayer->Viewport, sceneLayer->Fov, sceneLayer->ColorTexture, sceneLayer->RenderPose, sceneLayer->Header.Flags);
	}
	else if (m_SceneLayer && m_SceneLayer->Type == ovrLayerType_EyeMatrix)
	{
		ovrLayerEyeMatrix* sceneLayer = (ovrLayerEyeMatrix*)m_SceneLayer;

		ovrFovPort fov[ovrEye_Count] = {
			MatrixToFovPort(sceneLayer->Matrix[ovrEye_Left]),
			MatrixToFovPort(sceneLayer->Matrix[ovrEye_Right])
		};

		error = SubmitSceneLayer(session, sceneLayer->Viewport, fov, sceneLayer->ColorTexture, sceneLayer->RenderPose, sceneLayer->Header.Flags);
	}

	if (m_MirrorTexture && error == vr::VRCompositorError_None)
		RenderMirrorTexture(m_MirrorTexture);

	m_SceneLayer = nullptr;

	// Flip the profiler.
	MicroProfileFlip();

	vr::VRCompositor()->GetCumulativeStats(&session->Stats[session->FrameIndex % ovrMaxProvidedFrameStats], sizeof(vr::Compositor_CumulativeStats));

	return rev_CompositorErrorToOvrError(error);
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
	// Workaround for Defense Grid 2, which leaves these variables unintialized.
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

ovrFovPort CompositorBase::MatrixToFovPort(ovrMatrix4f matrix)
{
	ovrFovPort fov;
	fov.LeftTan = fov.RightTan = .5f / matrix.M[0][0];
	fov.UpTan   = fov.DownTan  = -.5f / matrix.M[1][1];
	return fov;
}

void CompositorBase::SubmitFovLayer(ovrRecti viewport[ovrEye_Count], ovrFovPort fov[ovrEye_Count], ovrTextureSwapChain swapChain[ovrEye_Count], unsigned int flags)
{
	MICROPROFILE_SCOPE(SubmitFovLayer);

	// If the right eye isn't set used the left eye for both
	if (!swapChain[ovrEye_Right])
		swapChain[ovrEye_Right] = swapChain[ovrEye_Left];

	MICROPROFILE_META_CPU("SwapChain Right", swapChain[ovrEye_Right]->Identifier);
	MICROPROFILE_META_CPU("SwapChain Left", swapChain[ovrEye_Left]->Identifier);

	// Render the scene layer
	for (int i = 0; i < ovrEye_Count; i++)
	{
		// Get the scene fov
		ovrFovPort sceneFov;
		if (m_SceneLayer->Type == ovrLayerType_EyeFov)
			sceneFov = ((ovrLayerEyeFov*)m_SceneLayer)->Fov[i];
		else if (m_SceneLayer->Type == ovrLayerType_EyeMatrix)
			sceneFov = MatrixToFovPort(((ovrLayerEyeMatrix*)m_SceneLayer)->Matrix[i]);

		// Calculate the fov quad
		vr::HmdVector4_t quad;
		quad.v[0] = fov[i].LeftTan / -sceneFov.LeftTan;
		quad.v[1] = fov[i].RightTan / sceneFov.RightTan;
		quad.v[2] = fov[i].UpTan / sceneFov.UpTan;
		quad.v[3] = fov[i].DownTan / -sceneFov.DownTan;

		// Calculate the texture bounds
		vr::VRTextureBounds_t bounds = ViewportToTextureBounds(viewport[i], swapChain[i], flags);

		// Composit the layer
		if (m_SceneLayer->Type == ovrLayerType_EyeFov)
		{
			ovrLayerEyeFov* layer = (ovrLayerEyeFov*)m_SceneLayer;
			RenderTextureSwapChain((vr::EVREye)i, swapChain[i], layer->ColorTexture[i], layer->Viewport[i], bounds, quad);
		}
		else if (m_SceneLayer->Type == ovrLayerType_EyeMatrix)
		{
			ovrLayerEyeMatrix* layer = (ovrLayerEyeMatrix*)m_SceneLayer;
			RenderTextureSwapChain((vr::EVREye)i, swapChain[i], layer->ColorTexture[i], layer->Viewport[i], bounds, quad);
		}
	}

	swapChain[ovrEye_Left]->Submit();
	if (swapChain[ovrEye_Left] != swapChain[ovrEye_Right])
		swapChain[ovrEye_Right]->Submit();
}

vr::VRCompositorError CompositorBase::SubmitSceneLayer(ovrSession session, ovrRecti viewport[ovrEye_Count], ovrFovPort fov[ovrEye_Count], ovrTextureSwapChain swapChain[ovrEye_Count], ovrPosef renderPose[ovrEye_Count], unsigned int flags)
{
	MICROPROFILE_SCOPE(SubmitSceneLayer);

	// If the right eye isn't set used the left eye for both
	if (!swapChain[ovrEye_Right])
		swapChain[ovrEye_Right] = swapChain[ovrEye_Left];

	MICROPROFILE_META_CPU("SwapChain Right", swapChain[ovrEye_Right]->Identifier);
	MICROPROFILE_META_CPU("Right Submit", swapChain[ovrEye_Right]->SubmitIndex);
	MICROPROFILE_META_CPU("SwapChain Left", swapChain[ovrEye_Left]->Identifier);
	MICROPROFILE_META_CPU("Left Submit", swapChain[ovrEye_Left]->SubmitIndex);

	// Submit the scene layer.
	vr::VRCompositorError err;
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrTextureSwapChain chain = swapChain[i];
		vr::VRTextureBounds_t bounds = ViewportToTextureBounds(viewport[i], swapChain[i], flags);

		// Get the descriptor for this eye
		ovrEyeRenderDesc* desc = session->Details->RenderDesc[i];

		// Shrink the bounds to account for the overlapping fov
		vr::VRTextureBounds_t fovBounds = FovPortToTextureBounds(desc->Fov, fov[i]);

		// Combine the fov bounds with the viewport bounds
		bounds.uMin += fovBounds.uMin * bounds.uMax;
		bounds.uMax *= fovBounds.uMax;
		bounds.vMin += fovBounds.vMin * bounds.vMax;
		bounds.vMax *= fovBounds.vMax;

		vr::VRTextureWithPose_t texture = chain->Textures[chain->SubmitIndex]->ToVRTexture();

		// Add the pose data to the eye texture
		REV::Matrix4f pose(renderPose[i]);
		if (session->TrackingOrigin == vr::TrackingUniverseSeated)
		{
			REV::Matrix4f offset(vr::VRSystem()->GetSeatedZeroPoseToStandingAbsoluteTrackingPose());
			texture.mDeviceToAbsoluteTracking = REV::Matrix4f(offset * pose);
		}
		else
		{
			texture.mDeviceToAbsoluteTracking = pose;
		}

		err = vr::VRCompositor()->Submit((vr::EVREye)i, (vr::Texture_t*)&texture, &bounds, vr::Submit_TextureWithPose);
		if (err != vr::VRCompositorError_None)
			break;
	}

	swapChain[ovrEye_Left]->Submit();
	if (swapChain[ovrEye_Left] != swapChain[ovrEye_Right])
		swapChain[ovrEye_Right]->Submit();

	return err;
}

void CompositorBase::SetMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	m_MirrorTexture = mirrorTexture;
}

vr::VRTextureBounds_t CompositorBase::FovPortToTextureBounds(ovrFovPort eyeFov, ovrFovPort fov)
{
	vr::VRTextureBounds_t result;

	// Adjust the bounds based on the field-of-view in the game
	result.uMin = 0.5f - 0.5f * eyeFov.LeftTan / fov.LeftTan;
	result.uMax = 0.5f + 0.5f * eyeFov.RightTan / fov.RightTan;
	result.vMin = 0.5f - 0.5f * eyeFov.UpTan / fov.UpTan;
	result.vMax = 0.5f + 0.5f * eyeFov.DownTan / fov.DownTan;

	return result;
}
