#include "CompositorBase.h"
#include "Common.h"

#include <vector>
#include <algorithm>

#define REV_LAYER_BIAS 0.0001f

CompositorBase::CompositorBase()
	: m_MirrorTexture(nullptr)
{
	m_SceneLayer = nullptr;
}

CompositorBase::~CompositorBase()
{
	if (m_MirrorTexture)
		delete m_MirrorTexture;
}

vr::EVRCompositorError CompositorBase::SubmitFrame(ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	// Other layers are interpreted as overlays.
	std::vector<vr::VROverlayHandle_t> activeOverlays;
	for (size_t i = 0; i < layerCount; i++)
	{
		if (layerPtrList[i] == nullptr)
			continue;

		// Overlays are assumed to be monoscopic quads.
		// TODO: Support stereoscopic layers, or at least display them as monoscopic layers.
		if (layerPtrList[i]->Type == ovrLayerType_Quad)
		{
			ovrLayerQuad* layer = (ovrLayerQuad*)layerPtrList[i];

			// Every overlay is associated with a swapchain.
			// This is necessary because the position of the layer may change in the array,
			// which would otherwise cause flickering between overlays.
			vr::VROverlayHandle_t overlay = layer->ColorTexture->Overlay;
			if (overlay == vr::k_ulOverlayHandleInvalid)
			{
				overlay = CreateOverlay();
				layer->ColorTexture->Overlay = overlay;
			}
			activeOverlays.push_back(overlay);

			// TODO: Set the high quality overlay when this is fixed in OpenVR.
			//if (layer->Header.Flags & ovrLayerFlag_HighQuality)
			//	vr::VROverlay()->SetHighQualityOverlay(overlay);

			// Add a depth bias to the pose based on the layer order.
			// TODO: Account for the orientation.
			ovrPosef pose = layer->QuadPoseCenter;
			pose.Position.z += (float)i * REV_LAYER_BIAS;

			// Transform the overlay.
			vr::HmdMatrix34_t transform = rev_OvrPoseToHmdMatrix(pose);
			vr::VROverlay()->SetOverlayWidthInMeters(overlay, layer->QuadSize.x);
			if (layer->Header.Flags & ovrLayerFlag_HeadLocked)
				vr::VROverlay()->SetOverlayTransformTrackedDeviceRelative(overlay, vr::k_unTrackedDeviceIndex_Hmd, &transform);
			else
				vr::VROverlay()->SetOverlayTransformAbsolute(overlay, vr::VRCompositor()->GetTrackingSpace(), &transform);

			// Set the texture and show the overlay.
			vr::VRTextureBounds_t bounds = ViewportToTextureBounds(layer->Viewport, layer->ColorTexture, layer->Header.Flags);
			vr::VROverlay()->SetOverlayTextureBounds(overlay, &bounds);
			vr::VROverlay()->SetOverlayTexture(overlay, &layer->ColorTexture->Submitted->ToVRTexture());

			// Show the overlay, unfortunately we have no control over the order in which
			// overlays are drawn.
			// TODO: Handle overlay errors.
			vr::VROverlay()->ShowOverlay(overlay);
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
		error = SubmitSceneLayer(sceneLayer->Viewport, sceneLayer->Fov, sceneLayer->ColorTexture, sceneLayer->Header.Flags);

		if (m_MirrorTexture && error == vr::VRCompositorError_None)
			RenderMirrorTexture(m_MirrorTexture, sceneLayer->ColorTexture);
	}
	else if (m_SceneLayer && m_SceneLayer->Type == ovrLayerType_EyeMatrix)
	{
		ovrLayerEyeMatrix* sceneLayer = (ovrLayerEyeMatrix*)m_SceneLayer;

		ovrFovPort fov[ovrEye_Count] = {
			MatrixToFovPort(sceneLayer->Matrix[ovrEye_Left]),
			MatrixToFovPort(sceneLayer->Matrix[ovrEye_Right])
		};

		error = SubmitSceneLayer(sceneLayer->Viewport, fov, sceneLayer->ColorTexture, sceneLayer->Header.Flags);

		if (m_MirrorTexture && error == vr::VRCompositorError_None)
			RenderMirrorTexture(m_MirrorTexture, sceneLayer->ColorTexture);
	}

	m_SceneLayer = nullptr;

	return error;
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

	if (GetAPI() == vr::API_OpenGL)
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
}

vr::VRCompositorError CompositorBase::SubmitSceneLayer(ovrRecti viewport[ovrEye_Count], ovrFovPort fov[ovrEye_Count], ovrTextureSwapChain swapChain[ovrEye_Count], unsigned int flags)
{
	// Submit the scene layer.
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrTextureSwapChain chain = swapChain[i];
		vr::VRTextureBounds_t bounds = ViewportToTextureBounds(viewport[i], swapChain[i], flags);

		// Shrink the bounds to account for the overlapping fov
		vr::VRTextureBounds_t fovBounds = rev_FovPortToTextureBounds((ovrEyeType)i, fov[i]);

		// Combine the fov bounds with the viewport bounds
		bounds.uMin += fovBounds.uMin * bounds.uMax;
		bounds.uMax *= fovBounds.uMax;
		bounds.vMin += fovBounds.vMin * bounds.vMax;
		bounds.vMax *= fovBounds.vMax;

		vr::VRCompositorError err = vr::VRCompositor()->Submit((vr::EVREye)i, &chain->Submitted->ToVRTexture(), &bounds);
		if (err != vr::VRCompositorError_None)
			return err;
	}

	return vr::VRCompositorError_None;
}

void CompositorBase::SetMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	m_MirrorTexture = mirrorTexture;
}
