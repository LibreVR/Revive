#pragma once

#include "Extras/OVR_Math.h"
#include "openvr.h"

OVR::Matrix4f rev_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m);
OVR::Vector3f rev_HmdVectorToOVRVector(vr::HmdVector3_t v);
vr::HmdMatrix34_t rev_OvrPoseToHmdMatrix(ovrPosef pose);
ovrPoseStatef rev_TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time);
