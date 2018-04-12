#pragma once

#include "Extras/OVR_Math.h"

#include <winrt/base.h>

namespace REM {
	class Vector2f : public OVR::Vector2f
	{
	public:
		// Inherit constructors
		using OVR::Vector2f::Vector2;
		Vector2f() : OVR::Vector2f() { }

		// Numerics-interop support
		Vector2f(const winrt::Windows::Foundation::Numerics::float2& s)
			: OVR::Vector2f(s.x, s.y) { }
		Vector2f(const winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float2>& s)
			: REM::Vector2f(s ? s.Value() : winrt::Windows::Foundation::Numerics::float2::zero()) { }

		operator const winrt::Windows::Foundation::Numerics::float2& () const
		{
			return reinterpret_cast<const winrt::Windows::Foundation::Numerics::float2&>(*this);
		}
	};

	class Vector3f : public OVR::Vector3f
	{
	public:
		// Inherit constructors
		using OVR::Vector3f::Vector3;
		Vector3f() : OVR::Vector3f() { }

		// Numerics-interop support
		Vector3f(const winrt::Windows::Foundation::Numerics::float3& s)
			: OVR::Vector3f(s.x, s.y, s.z) { }
		Vector3f(const winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float3>& s)
			: REM::Vector3f(s ? s.Value() : winrt::Windows::Foundation::Numerics::float3::zero()) { }

		operator const winrt::Windows::Foundation::Numerics::float3& () const
		{
			return reinterpret_cast<const winrt::Windows::Foundation::Numerics::float3&>(*this);
		}
	};

	class Quatf : public OVR::Quatf
	{
	public:
		// Inherit constructors
		using OVR::Quatf::Quat;
		Quatf() : OVR::Quatf() { }

		// Numerics-interop support
		Quatf(const winrt::Windows::Foundation::Numerics::quaternion& s)
			: OVR::Quatf(s.x, s.y, s.z, s.w) { }
		Quatf(const winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::quaternion>& s)
			: REM::Quatf(s ? s.Value() : winrt::Windows::Foundation::Numerics::quaternion::identity()) { }

		operator const winrt::Windows::Foundation::Numerics::quaternion& () const
		{
			return reinterpret_cast<const winrt::Windows::Foundation::Numerics::quaternion&>(*this);
		}
	};

	class Matrix4f : public OVR::Matrix4f
	{
	public:
		// Inherit constructors
		using OVR::Matrix4f::Matrix4;
		Matrix4f() : OVR::Matrix4f() { }

		// Numerics-interop support
		// FIXME: For some reason the float4x4 matrices are column-major instead of row-major
		Matrix4f(const winrt::Windows::Foundation::Numerics::float4x4& s)
			: OVR::Matrix4f(
				s.m11, s.m21, s.m31, s.m41,
				s.m12, s.m22, s.m32, s.m42,
				s.m13, s.m23, s.m33, s.m43,
				s.m14, s.m24, s.m34, s.m44) { }
		Matrix4f(const winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Numerics::float4x4>& s)
			: REM::Matrix4f(s ? s.Value() : winrt::Windows::Foundation::Numerics::float4x4::identity()) { }

		operator const winrt::Windows::Foundation::Numerics::float4x4& () const
		{
			return reinterpret_cast<const winrt::Windows::Foundation::Numerics::float4x4&>(*this);
		}
	};
}
