#pragma once

#include <openxr/openxr.h>
#include "Extras/OVR_Math.h"
#include "Extras/OVR_StereoProjection.h"

namespace XR {
	class Recti : public OVR::Recti
	{
	public:
		// Inherit constructors
		using OVR::Recti::Rect;
		Recti() : OVR::Recti() { }

		// OpenXR-interop support
		Recti(const XrRect2Di& s)
			: OVR::Recti(
				s.offset.x,
				s.offset.y,
				s.extent.width,
				s.extent.height
			)
		{ }

		operator const XrRect2Di& () const
		{
			return reinterpret_cast<const XrRect2Di&>(*this);
		}
	};

	class Vector2f : public OVR::Vector2f
	{
	public:
		// Inherit constructors
		using OVR::Vector2f::Vector2;
		Vector2f() : OVR::Vector2f() { }

		// OpenXR-interop support
		Vector2f(const XrVector2f& s)
			: OVR::Vector2f(s.x, s.y)
		{ }

		operator const XrVector2f& () const
		{
			return reinterpret_cast<const XrVector2f&>(*this);
		}
	};

	class Vector3f : public OVR::Vector3f
	{
	public:
		// Inherit constructors
		using OVR::Vector3f::Vector3;
		Vector3f() : OVR::Vector3f() { }

		// OpenXR-interop support
		Vector3f(const XrVector3f& s)
			: OVR::Vector3f(s.x, s.y, s.z)
		{ }

		operator const XrVector3f& () const
		{
			return reinterpret_cast<const XrVector3f&>(*this);
		}
	};

	class Quatf : public OVR::Quatf
	{
	public:
		// Inherit constructors
		using OVR::Quatf::Quat;
		Quatf() : OVR::Quatf() { }

		// OpenXR-interop support
		Quatf(const XrQuaternionf& s)
			: OVR::Quatf(s.x, s.y, s.z, s.w)
		{ }

		operator const XrQuaternionf& () const
		{
			return reinterpret_cast<const XrQuaternionf&>(*this);
		}
	};

	class Posef : public OVR::Posef
	{
	public:
		// Inherit constructors
		using OVR::Posef::Pose;
		Posef() : OVR::Posef() { }

		// OpenXR-interop support
		Posef(const XrPosef& s)
			: OVR::Posef(XR::Quatf(s.orientation), XR::Vector3f(s.position))
		{ }

		operator const XrPosef& () const
		{
			return reinterpret_cast<const XrPosef&>(*this);
		}

		static Posef Identity() {
			return OVR::Posef::Identity();
		}
	};

	class FovPort : public OVR::FovPort
	{
	public:
		// Inherit constructors
		using OVR::FovPort::FovPort;
		FovPort() : OVR::FovPort() { }

		// OpenXR-interop support
		FovPort(const XrFovf& s)
			: OVR::FovPort(
				tanf(s.angleUp),
				-tanf(s.angleDown),
				-tanf(s.angleLeft),
				tanf(s.angleRight)
			)
		{ }

		// Needs to be explicitly converted
		operator const XrFovf() const
		{
			return XrFovf{
				atanf(-LeftTan),
				atanf(RightTan),
				atanf(UpTan),
				atanf(-DownTan)
			};
		}
	};

	class Matrix4f : public OVR::Matrix4f
	{
	public:
		// Inherit constructors
		using OVR::Matrix4f::Matrix4;
		Matrix4f() : OVR::Matrix4f() { }

		// Needs to be explicitly converted
		operator const XrFovf() const
		{
			return XR::FovPort((M[1][2] + 1.0f) / M[1][1], (1.0f - M[1][2]) / M[1][1],
								(1.0f - M[0][2]) / M[0][0], (M[0][2] + 1.0f) / M[0][0]);
		}

#ifndef OVR_EXCLUDE_CAPI_FROM_MATH
		static Matrix4f FromProjectionDesc(ovrTimewarpProjectionDesc desc, ovrFovPort fov) {
			Matrix4f projection;
			OVR::ScaleAndOffset2D scaleAndOffset = OVR::CreateNDCScaleAndOffsetFromFov(fov);
			projection.M[0][0] = scaleAndOffset.Scale.x;
			projection.M[0][2] = desc.Projection32 * scaleAndOffset.Offset.x;
			projection.M[1][1] = scaleAndOffset.Scale.y;
			projection.M[1][2] = desc.Projection32 * -scaleAndOffset.Offset.y;
			projection.M[2][2] = desc.Projection22;
			projection.M[2][3] = desc.Projection23;
			projection.M[3][2] = desc.Projection32;
			return projection;
		}
#endif
	};
}
