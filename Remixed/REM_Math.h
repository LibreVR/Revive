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
		{
			x = s.x;
			y = s.y;
			z = s.z;
		}

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
		{
			x = s.x;
			y = s.y;
			z = s.z;
			w = s.w;
		}

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
		{
			M[0][0] = s.m11;
			M[0][1] = s.m12;
			M[0][2] = s.m13;
			M[0][3] = s.m14;
			M[1][0] = s.m21;
			M[1][1] = s.m22;
			M[1][2] = s.m23;
			M[1][3] = s.m24;
			M[2][0] = s.m31;
			M[2][1] = s.m32;
			M[2][2] = s.m33;
			M[2][3] = s.m34;
			M[3][0] = s.m41;
			M[3][1] = s.m42;
			M[3][2] = s.m43;
			M[3][3] = s.m44;
		}

		operator const winrt::Windows::Foundation::Numerics::float4x4& () const
		{
			return reinterpret_cast<const winrt::Windows::Foundation::Numerics::float4x4&>(*this);
		}
	};
}
