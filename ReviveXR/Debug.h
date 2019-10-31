#pragma once

#include <map>
#include <vector>

#include "OVR_CAPI.h"
#include "XR_Math.h"

#include "CImg.h"

using namespace cimg_library;

/*
 * TODO: generalize for plotting other stuff
 *
 * Simple visual debugging view for tracking. Plots various tracking data as
 * needed.
 */
class ReviveTrackingPlotter {

public:

	ReviveTrackingPlotter(int samples) :
		m_samples(samples),
		m_leftHand(samples),
		m_rightHand(samples),
		m_head(samples)
	{
		clear();
	}

	void SampleValue(ovrTrackingState state)
	{
		m_leftHand.set(m_currsample, state.HandPoses[ovrHand_Left]);
		m_rightHand.set(m_currsample, state.HandPoses[ovrHand_Right]);
		m_head.set(m_currsample, state.HeadPose);

		m_currsample++;

		if (m_currsample >= m_samples)
			clear();
	}

	int size()
	{
		return m_currsample;
	}

	void plot()
	{
		/* 1200x700 image, 3 color componenents, default white (1) */
		int resx = 1200;
		int resy = 700;

		const unsigned char blue[] = { 0, 0, 255 };
		const unsigned char red[] = { 255, 0, 0 };
		const unsigned char green[] = { 0, 255, 0 };
		const unsigned char white[] = { 255, 255, 255 };

		/* Graph left hand linear velocity & accel */
		CImg<double> gd_lh_linvel(&m_leftHand.linearVelocity[0], m_leftHand.linearVelocity.size());
		CImg<double> gd_lh_lincel(&m_leftHand.linearAcceleration[0], m_leftHand.linearAcceleration.size());
		CImg<unsigned char> graph(resx, resy, 1, 3, 1);
		graph.draw_grid(10, 10, 0, 0, false, false, white, 0.1f, 0x55555555, 0x55555555);
		graph.draw_graph(gd_lh_linvel, blue, 1, 1, 2, 0, 10);
		graph.draw_graph(gd_lh_lincel, red, 1, 1, 2, 0, 10);
		graph.mirror('y');

		/* Axes */
		int yaxis[] = { 20, 15, 10, 5, 0 };
		int xaxis[] = { 0, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
		CImg<int> yaxes(yaxis, sizeof(yaxis) / sizeof(yaxis[0]));
		CImg<int> xaxes(xaxis, sizeof(xaxis) / sizeof(xaxis[0]));
		graph.draw_axes(xaxes, yaxes, green, 0.3f);

		/* Sample counter */
		char buf[16];
		snprintf(buf, sizeof(buf), "%d", m_currsample);
		graph.draw_text(20, 20, buf, green);

		disp.display(graph);
	}

	void clear()
	{
		m_leftHand.assign(m_samples, 0);
		m_rightHand.assign(m_samples, 0);
		m_head.assign(m_samples, 0);

		m_currsample = 0;
	}

protected:

	class PoseVecs {
	public:
		PoseVecs(int size) :
			linearVelocity(size),
			linearAcceleration(size),
			angularVelocity(size),
			angularAcceleration(size)
		{
			assign(size, 0);
		}

		std::vector<double> linearVelocity;
		std::vector<double> linearAcceleration;
		std::vector<double> angularVelocity;
		std::vector<double> angularAcceleration;

		void assign(int n, int val)
		{
			linearVelocity.assign(n, val);
			linearAcceleration.assign(n, val);
			angularVelocity.assign(n, val);
			angularAcceleration.assign(n, val);
		}

		void set(int i, ovrPoseStatef& poseState)
		{
			linearVelocity[i] = XR::Vector3f(poseState.LinearVelocity).Length();
			linearAcceleration[i] = XR::Vector3f(poseState.LinearAcceleration).Length();
			angularVelocity[i] = XR::Vector3f(poseState.AngularVelocity).Length();
			angularAcceleration[i] = XR::Vector3f(poseState.AngularAcceleration).Length();
		}

	};

	CImgDisplay disp;

	int m_currsample;
	int m_samples;

	PoseVecs m_leftHand;
	PoseVecs m_rightHand;
	PoseVecs m_head;
};
