#include "Session.h"
#include "CompositorBase.h"
#include "SessionDetails.h"
#include "InputManager.h"
#include "SettingsManager.h"
#include "Settings.h"

ovrHmdStruct::ovrHmdStruct()
	: ShouldQuit(false)
	, IsVisible(false)
	, FrameIndex(0)
	, StatsIndex(0)
	, Compositor(nullptr)
	, Input(new InputManager())
	, Details(new SessionDetails())
	, Settings(new SettingsManager())
{
	memset(StringBuffer, 0, sizeof(StringBuffer));
	memset(&ResetStats, 0, sizeof(ResetStats));
	memset(Stats, 0, sizeof(Stats));

	// Get the default universe origin from the settings
	TrackingOrigin = (vr::ETrackingUniverseOrigin)ovr_GetInt(this, REV_KEY_DEFAULT_ORIGIN, REV_DEFAULT_ORIGIN);
}

ovrHmdStruct::~ovrHmdStruct()
{
}
