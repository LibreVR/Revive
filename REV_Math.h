#pragma once

#include "Extras/OVR_Math.h"
#include "openvr.h"

OVR::Matrix4f REV_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m)
{
	OVR::Matrix4f r;
	memcpy(r.M, m.m, sizeof(vr::HmdMatrix34_t));
	return r;
}

OVR::Vector3f REV_HmdVectorToOVRVector(vr::HmdVector3_t v)
{
	OVR::Vector3f r;
	r.x = v.v[0];
	r.y = v.v[1];
	r.z = v.v[2];
	return r;
}


