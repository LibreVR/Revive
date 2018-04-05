#include "TrackingManager.h"

#include <winrt/Windows.Graphics.Holographic.h>
using namespace winrt::Windows::Graphics::Holographic;

#include <winrt/Windows.Perception.h>
using namespace winrt::Windows::Perception;

#include <winrt/Windows.Perception.Spatial.h>
using namespace winrt::Windows::Perception::Spatial;

#include <winrt/Windows.Foundation.h>
using namespace winrt::Windows::Foundation;

#include "OVR_CAPI_Keys.h"

TrackingManager::TrackingManager()
	: m_ShouldRecenter(false)
	, m_UseStageFrame(false)
	, m_SpatialMutex()
	, m_Locator(nullptr)
	, m_AttachedReference(nullptr)
	, m_StationaryReference(nullptr)
	, m_StageReference(nullptr)
	, m_LastTransform(Numerics::float4x4::identity())
	, m_OriginPosition(Numerics::float3::zero())
	, m_OriginOrientation(Numerics::quaternion::identity())
{
	m_Locator = SpatialLocator::GetDefault();
	m_AttachedReference = m_Locator.CreateAttachedFrameOfReferenceAtCurrentHeading();
	m_StationaryReference = m_Locator.CreateStationaryFrameOfReferenceAtCurrentLocation();

	m_StageReference = SpatialStageFrameOfReference::Current();
	SpatialStageFrameOfReference::CurrentChanged([=](
		winrt::Windows::Foundation::IInspectable sender,
		winrt::Windows::Foundation::IInspectable args)
	{
		m_ShouldRecenter = true;
	});
}

TrackingManager::~TrackingManager()
{
}

SpatialCoordinateSystem TrackingManager::CoordinateSystem()
{
	std::shared_lock<std::shared_mutex> lk(m_SpatialMutex);
	return m_UseStageFrame && m_StageReference ? m_StageReference.CoordinateSystem() : m_StationaryReference.CoordinateSystem();
}

HolographicStereoTransform TrackingManager::GetViewTransform(HolographicFrame frame, uint32_t displayIndex)
{
	HolographicFramePrediction prediction = frame.CurrentPrediction();
	HolographicCameraPose pose = prediction.CameraPoses().GetAt(displayIndex);

	SpatialCoordinateSystem coord = CoordinateSystem();
	SpatialCoordinateSystem stationaryCoord = m_AttachedReference.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp());
	IReference<Numerics::float4x4> transform = coord.TryGetTransformTo(stationaryCoord);
	if (transform)
		m_LastTransform = transform.Value();

	IReference<HolographicStereoTransform> view = pose.TryGetViewTransform(coord);
	if (view)
		return view.Value();

	view = pose.TryGetViewTransform(stationaryCoord);
	HolographicStereoTransform stereo = view.Value();
	stereo.Left = m_LastTransform * stereo.Left;
	stereo.Right = m_LastTransform * stereo.Right;
	return stereo;
}

HolographicStereoTransform TrackingManager::GetLocalViewTransform(HolographicFrame frame, uint32_t displayIndex)
{
	HolographicFramePrediction prediction = frame.CurrentPrediction();
	HolographicCameraPose pose = prediction.CameraPoses().GetAt(displayIndex);

	SpatialLocatorAttachedFrameOfReference reference = m_Locator.CreateAttachedFrameOfReferenceAtCurrentHeading();
	IReference<HolographicStereoTransform> ref = pose.TryGetViewTransform(reference.GetStationaryCoordinateSystemAtTimestamp(prediction.Timestamp()));
	assert(ref);
	return ref.Value();
}

void TrackingManager::UseFloorLevelFrameOfReference(bool enabled)
{
	// Make the stationary reference frame somewhat floor-level in case as a fall-back
	if (!m_StageReference && m_UseStageFrame != enabled)
	{
		std::unique_lock<std::shared_mutex> lk(m_SpatialMutex);
		Numerics::float3 position = Numerics::float3::zero();
		m_OriginPosition.y += enabled ? -OVR_DEFAULT_PLAYER_HEIGHT : OVR_DEFAULT_PLAYER_HEIGHT;
		m_StationaryReference = m_Locator.CreateStationaryFrameOfReferenceAtCurrentLocation(m_OriginPosition, m_OriginOrientation);
	}

	// Try to use the stage reference frame if possible since it's already floor-level
	m_UseStageFrame = enabled;
}

void TrackingManager::RecenterTrackingOrigin(winrt::Windows::Foundation::Numerics::float3 position,
	winrt::Windows::Foundation::Numerics::quaternion orientation)
{
	m_ShouldRecenter = false;
	std::unique_lock<std::shared_mutex> lk(m_SpatialMutex);
	m_OriginPosition += position;
	m_OriginOrientation *= orientation;
	m_StationaryReference = m_Locator.CreateStationaryFrameOfReferenceAtCurrentLocation(m_OriginPosition, m_OriginOrientation);
	m_StageReference = SpatialStageFrameOfReference::Current();
}
