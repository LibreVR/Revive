#pragma once

#include "Extras/OVR_Math.h"
#include "openvr.h"

OVR::Matrix4f REV_HmdMatrixToOVRMatrix(vr::HmdMatrix34_t m);
OVR::Vector3f REV_HmdVectorToOVRVector(vr::HmdVector3_t v);
vr::HmdMatrix34_t REV_OvrPoseToHmdMatrix(ovrPosef pose);
ovrPoseStatef REV_TrackedDevicePoseToOVRPose(vr::TrackedDevicePose_t pose, double time);
