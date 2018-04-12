#include "CompositorBase.h"
#include "Session.h"
#include "FrameList.h"
#include "TrackingManager.h"

#include "OVR_CAPI.h"
#include "REM_Math.h"
#include "microprofile.h"

#include <vector>
#include <algorithm>

#include <winrt/Windows.Foundation.h>
using namespace winrt::Windows::Foundation;

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

#include <winrt/Windows.Graphics.DirectX.h>
using namespace winrt::Windows::Graphics::DirectX;

#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;

MICROPROFILE_DEFINE(WaitToBeginFrame, "Compositor", "WaitFrame", 0x00ff00);
MICROPROFILE_DEFINE(BeginFrame, "Compositor", "BeginFrame", 0x00ff00);
MICROPROFILE_DEFINE(EndFrame, "Compositor", "EndFrame", 0x00ff00);
MICROPROFILE_DEFINE(SubmitFovLayer, "Compositor", "SubmitFovLayer", 0x00ff00);

CompositorBase::CompositorBase()
	: m_MirrorTexture(nullptr)
	, m_ChainCount(0)
{
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

	for (int i = 0; i < swapChain->Length; i++)
	{
		TextureBase* texture = CreateTexture();
		bool success = texture->Init(desc->Type, desc->Width, desc->Height, desc->MipLevels,
			desc->ArraySize, desc->SampleCount, desc->Format, desc->MiscFlags, desc->BindFlags);
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
	bool success = texture->Init(ovrTexture_2D, desc->Width, desc->Height, 1, 1, 1, desc->Format,
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

	HolographicFrame frame = session->Frames->GetFrame(frameIndex - 1);
	if (frame)
		frame.WaitForFrameToFinish();
	session->Frames->EndFrame(frameIndex - 1);
	return ovrSuccess;
}

ovrResult CompositorBase::EndFrame(ovrSession session, long long frameIndex, ovrLayerHeader const * const * layerPtrList, unsigned int layerCount)
{
	MICROPROFILE_SCOPE(EndFrame);

	if (layerCount == 0 || !layerPtrList)
		return ovrError_InvalidParameter;

	// Flush all pending draw calls.
	Flush();

	HolographicFrame frame = session->Frames->GetPendingFrame(frameIndex);
	if (!frame)
		return ovrError_InvalidParameter;

	bool baseLayerFound = false;
	std::vector<HolographicQuadLayer> activeOverlays;
	for (uint32_t i = 0; i < layerCount; i++)
	{
		if (layerPtrList[i] == nullptr)
			continue;

		// TODO: Support ovrLayerType_Cylinder and ovrLayerType_Cube
		if (layerPtrList[i]->Type == ovrLayerType_Quad)
		{
			ovrLayerQuad* layer = (ovrLayerQuad*)layerPtrList[i];
			ovrTextureSwapChain swapchain = layer->ColorTexture;

			if (!swapchain->Overlay)
			{
				Size dims = Size((float)swapchain->Desc.Width, (float)swapchain->Desc.Height);
				DirectXPixelFormat format = TextureBase::IsSRGBFormat(swapchain->Desc.Format) ?
					DirectXPixelFormat::B8G8R8A8UIntNormalizedSrgb :
					DirectXPixelFormat::B8G8R8A8UIntNormalized;
				swapchain->Overlay = HolographicQuadLayer(dims, format);
			}

			HolographicQuadLayerUpdateParameters params = frame.GetQuadLayerUpdateParameters(swapchain->Overlay);
			params.UpdateExtents(REM::Vector2f(layer->QuadSize));
			if (layer->Header.Flags & ovrLayerFlag_HeadLocked)
				params.UpdateLocationWithDisplayRelativeMode(REM::Vector3f(layer->QuadPoseCenter.Position), REM::Quatf(layer->QuadPoseCenter.Orientation));
			else
				params.UpdateLocationWithStationaryMode(session->Tracking->CoordinateSystem(), REM::Vector3f(layer->QuadPoseCenter.Position), REM::Quatf(layer->QuadPoseCenter.Orientation));

			IDirect3DSurface surface = params.AcquireBufferToUpdateContent();
			RenderTextureSwapChain(surface, layer->ColorTexture, layer->Viewport);
			activeOverlays.push_back(swapchain->Overlay);
		}
		else if (layerPtrList[i]->Type == ovrLayerType_EyeFov ||
			layerPtrList[i]->Type == ovrLayerType_EyeFovDepth ||
			layerPtrList[i]->Type == ovrLayerType_EyeFovMultires)
		{
			ovrLayerEyeFov* layer = (ovrLayerEyeFov*)layerPtrList[i];
			SubmitFovLayer(frame, layer);
			baseLayerFound = true;
		}
		else if (layerPtrList[i]->Type == ovrLayerType_EyeMatrix)
		{
			ovrLayerEyeFov layer = ToFovLayer((ovrLayerEyeMatrix*)layerPtrList[i]);
			SubmitFovLayer(frame, &layer);
			baseLayerFound = true;
		}
	}

	HolographicFramePrediction prediction = frame.CurrentPrediction();
	for (HolographicCameraPose pose : prediction.CameraPoses())
	{
		HolographicCamera cam = pose.HolographicCamera();
		size_t size = std::min(activeOverlays.size(), (size_t)cam.MaxQuadLayerCount());
		if (size > 0)
		{
			winrt::array_view<const HolographicQuadLayer> layers(activeOverlays.data(), activeOverlays.data() + size);
			cam.QuadLayers().ReplaceAll(layers);
			cam.IsPrimaryLayerEnabled(baseLayerFound);
		}
	}

	HolographicFramePresentResult result = frame.PresentUsingCurrentPrediction(HolographicFramePresentWaitBehavior::DoNotWaitForFrameToFinish);
	if (result == HolographicFramePresentResult::DeviceRemoved)
		return ovrError_DisplayLost;

	// TODO: Mirror textures
	//if (m_MirrorTexture && success)
	//	RenderMirrorTexture(m_MirrorTexture);

	MicroProfileFlip();

	return ovrSuccess;
}

ovrLayerEyeFov CompositorBase::ToFovLayer(ovrLayerEyeMatrix* matrix)
{
	ovrLayerEyeFov layer = { ovrLayerType_EyeFov };
	layer.Header.Flags = matrix->Header.Flags;
	layer.SensorSampleTime = matrix->SensorSampleTime;

	for (int i = 0; i < ovrEye_Count; i++)
	{
		layer.Fov[i].LeftTan = layer.Fov[i].RightTan = .5f / matrix->Matrix[i].M[0][0];
		layer.Fov[i].UpTan = layer.Fov[i].DownTan = -.5f / matrix->Matrix[i].M[1][1];
		layer.ColorTexture[i] = matrix->ColorTexture[i];
		layer.Viewport[i] = matrix->Viewport[i];
		layer.RenderPose[i] = matrix->RenderPose[i];
	}

	return layer;
}

void CompositorBase::SubmitFovLayer(HolographicFrame frame, ovrLayerEyeFov* fovLayer)
{
	MICROPROFILE_SCOPE(SubmitFovLayer);

	ovrTextureSwapChain swapChain[ovrEye_Count] = {
		fovLayer->ColorTexture[ovrEye_Left],
		fovLayer->ColorTexture[ovrEye_Right]
	};

	// If the right eye isn't set use the left eye for both
	if (!swapChain[ovrEye_Right])
		swapChain[ovrEye_Right] = swapChain[ovrEye_Left];

	HolographicFramePrediction prediction = frame.CurrentPrediction();
	for (HolographicCameraPose pose : prediction.CameraPoses())
	{
		// Submit the scene layer.
		for (int i = 0; i < ovrEye_Count; i++)
		{
			ovrRecti viewport = fovLayer->Viewport[i];
			if (fovLayer->Header.Flags & ovrLayerFlag_TextureOriginAtBottomLeft)
			{
				viewport.Pos.y += viewport.Size.h;
				viewport.Size.h *= -1;
			}

			HolographicCameraRenderingParameters params = frame.GetRenderingParameters(pose);
			IDirect3DSurface surface = params.Direct3D11BackBuffer();
			RenderTextureSwapChain(surface, swapChain[i], viewport, (ovrEyeType)i);
		}
	}

	swapChain[ovrEye_Left]->Submit();
	if (swapChain[ovrEye_Left] != swapChain[ovrEye_Right])
		swapChain[ovrEye_Right]->Submit();
}

void CompositorBase::SetMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	m_MirrorTexture = mirrorTexture;
}
