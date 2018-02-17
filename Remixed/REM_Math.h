#pragma once

#include "Extras/OVR_Math.h"

#include <winrt/base.h>

namespace REM {
	class Vector3f : public OVR::Vector3f
	{
	public:
		// Inherit constructors
		using OVR::Vector3f::Vector3;
		Vector3f() : OVR::Vector3f() { }

		// Numerics-interop support
		Vector3f(const winrt::Windows::Foundation::Numerics::float3& s)
			: OVR::Vector3f(s.x, s.y, s.z) { }

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
		Matrix4f(const winrt::Windows::Foundation::Numerics::float4x4& s)
			: OVR::Matrix4f(
				s.m11, s.m12, s.m13, s.m14,
				s.m21, s.m22, s.m23, s.m24,
				s.m31, s.m32, s.m33, s.m34,
				s.m41, s.m42, s.m43, s.m44) { }

		operator const winrt::Windows::Foundation::Numerics::float4x4& () const
		{
			return reinterpret_cast<const winrt::Windows::Foundation::Numerics::float4x4&>(*this);
		}
	};
}
