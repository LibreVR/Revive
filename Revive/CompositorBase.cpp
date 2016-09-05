#include "CompositorBase.h"
#include "REV_Common.h"
#include "REV_Math.h"

#include <vector>
#include <algorithm>

#define REV_LAYER_BIAS 0.0001f

CompositorBase::CompositorBase()
{
}

CompositorBase::~CompositorBase()
{
}

vr::EVRCompositorError CompositorBase::SubmitFrame(const ovrViewScaleDesc* viewScaleDesc, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
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

			// Set the high quality overlay.
			// FIXME: Why are High quality overlays headlocked in OpenVR?
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
			vr::VRTextureBounds_t bounds = rev_ViewportToTextureBounds(layer->Viewport, layer->ColorTexture, layer->Header.Flags);
			vr::VROverlay()->SetOverlayTextureBounds(overlay, &bounds);
			vr::VROverlay()->SetOverlayTexture(overlay, layer->ColorTexture->Submitted);

			// Show the overlay, unfortunately we have no control over the order in which
			// overlays are drawn.
			// TODO: Handle overlay errors.
			vr::VROverlay()->ShowOverlay(overlay);
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

	// The first layer is assumed to be the application scene.
	if (layerPtrList[0]->Type == ovrLayerType_EyeFov)
	{
		ovrLayerEyeFov* sceneLayer = (ovrLayerEyeFov*)layerPtrList[0];

		// Submit the scene layer.
		for (int i = 0; i < ovrEye_Count; i++)
		{
			ovrTextureSwapChain chain = sceneLayer->ColorTexture[i];
			vr::VRTextureBounds_t bounds = rev_ViewportToTextureBounds(sceneLayer->Viewport[i], sceneLayer->ColorTexture[i], sceneLayer->Header.Flags);

			float left, right, top, bottom;
			vr::VRSystem()->GetProjectionRaw((vr::EVREye)i, &left, &right, &top, &bottom);

			// Shrink the bounds to account for the overlapping fov
			ovrFovPort fov = sceneLayer->Fov[i];
			float uMin = 0.5f + 0.5f * left / fov.LeftTan;
			float uMax = 0.5f + 0.5f * right / fov.RightTan;
			float vMin = 0.5f - 0.5f * bottom / fov.UpTan;
			float vMax = 0.5f - 0.5f * top / fov.DownTan;

			// Combine the fov bounds with the viewport bounds
			bounds.uMin += uMin * bounds.uMax;
			bounds.uMax *= uMax;
			bounds.vMin += vMin * bounds.vMax;
			bounds.vMax *= vMax;

			if (chain->Textures[i].eType == vr::API_OpenGL)
			{
				bounds.vMin = 1.0f - bounds.vMin;
				bounds.vMax = 1.0f - bounds.vMax;
			}

			vr::EVRCompositorError err = vr::VRCompositor()->Submit((vr::EVREye)i, chain->Submitted, &bounds);
			if (err != vr::VRCompositorError_None)
				return err;
		}
	}
	else if (layerPtrList[0]->Type == ovrLayerType_EyeMatrix)
	{
		ovrLayerEyeMatrix* sceneLayer = (ovrLayerEyeMatrix*)layerPtrList[0];

		// Submit the scene layer.
		for (int i = 0; i < ovrEye_Count; i++)
		{
			ovrTextureSwapChain chain = sceneLayer->ColorTexture[i];
			vr::VRTextureBounds_t bounds = rev_ViewportToTextureBounds(sceneLayer->Viewport[i], sceneLayer->ColorTexture[i], sceneLayer->Header.Flags);

			float left, right, top, bottom;
			vr::VRSystem()->GetProjectionRaw((vr::EVREye)i, &left, &right, &top, &bottom);

			// Shrink the bounds to account for the overlapping fov
			ovrVector2f fov = { .5f / sceneLayer->Matrix[i].M[0][0], .5f / sceneLayer->Matrix[i].M[1][1] };
			float uMin = 0.5f + 0.5f * left / fov.x;
			float uMax = 0.5f + 0.5f * right / fov.x;
			float vMin = 0.5f - 0.5f * bottom / fov.y;
			float vMax = 0.5f - 0.5f * top / fov.y;

			// Combine the fov bounds with the viewport bounds
			bounds.uMin += uMin * bounds.uMax;
			bounds.uMax *= uMax;
			bounds.vMin += vMin * bounds.vMax;
			bounds.vMax *= vMax;

			if (chain->Textures[i].eType == vr::API_OpenGL)
			{
				bounds.vMin = 1.0f - bounds.vMin;
				bounds.vMax = 1.0f - bounds.vMax;
			}

			vr::EVRCompositorError err = vr::VRCompositor()->Submit((vr::EVREye)i, chain->Submitted, &bounds);
			if (err != vr::VRCompositorError_None)
				return err;
		}
	}

	// TODO: Render to the mirror texture here.
	// Currently the mirror texture code is not stable enough yet.

	return vr::VRCompositorError_None;
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
