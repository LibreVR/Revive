#pragma once

#include "openvr.h"
#include "Extras/OVR_Math.h"

namespace REV {
	class Vector3f : public OVR::Vector3f
	{
	public:
		// Inherit constructors
		using OVR::Vector3f::Vector3;
		Vector3f() : OVR::Vector3f() { }

		// OpenVR-interop support
		Vector3f(const vr::HmdVector3_t& s)
		{
			x = s.v[0];
			y = s.v[1];
			z = s.v[2];
		}

		operator const vr::HmdVector3_t& () const
		{
			return reinterpret_cast<const vr::HmdVector3_t&>(*this);
		}
	};

	class Matrix4f : public OVR::Matrix4f
	{
	public:
		// Inherit constructors
		using OVR::Matrix4f::Matrix4;
		Matrix4f() : OVR::Matrix4f() { }

		// OpenVR-interop support
		explicit Matrix4f(const vr::HmdMatrix34_t& s)
		{
			memcpy(M, s.m, sizeof(s.m));
			M[3][0] = M[3][1] = M[3][2] = 0.f; M[3][3] = 1.f;
		}

		operator const vr::HmdMatrix34_t() const
		{
			vr::HmdMatrix34_t s;
			memcpy(s.m, M, sizeof(s.m));
			return s;
		}

		// OpenVR-interop support
		Matrix4f(const vr::HmdMatrix44_t& s)
		{
			memcpy(M, s.m, sizeof(s.m));
		}

		operator const vr::HmdMatrix44_t& () const
		{
			return reinterpret_cast<const vr::HmdMatrix44_t&>(*this);
		}
	};
}
