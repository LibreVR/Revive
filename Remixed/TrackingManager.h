#pragma once

#include <mutex>
#include <atomic>

#include <winrt/Windows.Graphics.Holographic.h>
#include <winrt/Windows.Perception.Spatial.h>

class TrackingManager
{
public:
	TrackingManager();
	~TrackingManager();

	winrt::Windows::Graphics::Holographic::HolographicStereoTransform GetViewTransform(
		winrt::Windows::Graphics::Holographic::HolographicFrame frame, uint32_t displayIndex = 0);
	winrt::Windows::Graphics::Holographic::HolographicStereoTransform GetLocalViewTransform(
		winrt::Windows::Graphics::Holographic::HolographicFrame frame, uint32_t displayIndex = 0);

	winrt::Windows::Perception::Spatial::SpatialCoordinateSystem CoordinateSystem();

	bool UsingFloorLevel() { return m_UseStageFrame; }
	bool ShouldRecenter() { return m_ShouldRecenter; }
	void UseFloorLevelFrameOfReference(bool enabled = true);
	void RecenterTrackingOrigin(
		winrt::Windows::Foundation::Numerics::float3 position = winrt::Windows::Foundation::Numerics::float3::zero(),
		winrt::Windows::Foundation::Numerics::quaternion orientation = winrt::Windows::Foundation::Numerics::quaternion::identity());

private:
	std::atomic_bool m_UseStageFrame;
	std::atomic_bool m_ShouldRecenter;
	std::shared_mutex m_SpatialMutex;
	winrt::Windows::Perception::Spatial::SpatialLocator m_Locator;
	winrt::Windows::Perception::Spatial::SpatialLocatorAttachedFrameOfReference m_AttachedReference;
	winrt::Windows::Perception::Spatial::SpatialStationaryFrameOfReference m_StationaryReference;
	winrt::Windows::Perception::Spatial::SpatialStageFrameOfReference m_StageReference;

	winrt::Windows::Foundation::Numerics::float4x4 m_LastTransform;
	winrt::Windows::Foundation::Numerics::float3 m_OriginPosition;
	winrt::Windows::Foundation::Numerics::quaternion m_OriginOrientation;
};
