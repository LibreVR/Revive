#include "CompositorBase.h"
#include "Session.h"
#include "FrameList.h"

#include "OVR_CAPI.h"
#include "microprofile.h"

#include <vector>
#include <algorithm>

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

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

	session->Frames->GetFrame(frameIndex - 1).WaitForFrameToFinish();
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

	ovrLayerEyeFov baseLayer;
	bool baseLayerFound = false;
	for (uint32_t i = 0; i < layerCount; i++)
	{
		if (layerPtrList[i] == nullptr)
			continue;

		// TODO: Support ovrLayerType_Quad, ovrLayerType_Cylinder and ovrLayerType_Cube
		if (layerPtrList[i]->Type == ovrLayerType_EyeFov ||
			layerPtrList[i]->Type == ovrLayerType_EyeFovDepth ||
			layerPtrList[i]->Type == ovrLayerType_EyeFovMultires)
		{
			ovrLayerEyeFov* layer = (ovrLayerEyeFov*)layerPtrList[i];

			SubmitFovLayer(session, frameIndex, layer);
			baseLayerFound = true;
		}
		else if (layerPtrList[i]->Type == ovrLayerType_EyeMatrix)
		{
			ovrLayerEyeFov layer = ToFovLayer((ovrLayerEyeMatrix*)layerPtrList[i]);

			SubmitFovLayer(session, frameIndex, &layer);
			baseLayerFound = true;
		}
	}

	HolographicFrame frame = session->Frames->GetFrame(frameIndex);
	HolographicFramePrediction prediction = frame.CurrentPrediction();
	HolographicCameraPose pose = prediction.CameraPoses().GetAt(0);
	HolographicCamera cam = pose.HolographicCamera();
	//cam.IsPrimaryLayerEnabled(baseLayerFound);

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

void CompositorBase::SubmitFovLayer(ovrSession session, long long frameIndex, ovrLayerEyeFov* fovLayer)
{
	MICROPROFILE_SCOPE(SubmitFovLayer);

	ovrTextureSwapChain swapChain[ovrEye_Count] = {
		fovLayer->ColorTexture[ovrEye_Left],
		fovLayer->ColorTexture[ovrEye_Right]
	};

	// If the right eye isn't set use the left eye for both
	if (!swapChain[ovrEye_Right])
		swapChain[ovrEye_Right] = swapChain[ovrEye_Left];

	// Submit the scene layer.
	for (int i = 0; i < ovrEye_Count; i++)
	{
		ovrRecti viewport = fovLayer->Viewport[i];
		if (fovLayer->Header.Flags & ovrLayerFlag_TextureOriginAtBottomLeft)
		{
			viewport.Pos.y += viewport.Size.h;
			viewport.Size.h *= -1;
		}
		RenderTextureSwapChain(session, frameIndex, (ovrEyeType)i, swapChain[i], viewport);
	}

	swapChain[ovrEye_Left]->Submit();
	if (swapChain[ovrEye_Left] != swapChain[ovrEye_Right])
		swapChain[ovrEye_Right]->Submit();
}

void CompositorBase::SetMirrorTexture(ovrMirrorTexture mirrorTexture)
{
	m_MirrorTexture = mirrorTexture;
}
