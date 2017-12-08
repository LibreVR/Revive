#pragma once

enum revGripType
{
	revGrip_Normal = 0,
	revGrip_Toggle = 1,
	revGrip_Hybrid = 2,
};

#define REV_SETTINGS_SECTION				"revive"
#define REV_ROUND(x)						round((double)x * pow(10.0, 4)) / pow(10.0, 4);

#define REV_KEY_DEFAULT_ORIGIN				"DefaultTrackingOrigin"
#define REV_DEFAULT_ORIGIN					vr::TrackingUniverseSeated

#define REV_KEY_PIXELS_PER_DISPLAY			"pixelsPerDisplayPixel"
#define REV_DEFAULT_PIXELS_PER_DISPLAY		0.0f

#define REV_KEY_THUMB_DEADZONE				"ThumbDeadzone"
#define REV_DEFAULT_THUMB_DEADZONE			0.3f

#define REV_KEY_THUMB_SENSITIVITY			"ThumbSensitivity"
#define REV_DEFAULT_THUMB_SENSITIVITY		2.0f

#define REV_KEY_TOGGLE_GRIP					"ToggleGrip"
#define REV_DEFAULT_TOGGLE_GRIP				revGrip_Normal

#define REV_KEY_TRIGGER_GRIP				"TriggerAsGrip"
#define REV_DEFAULT_TRIGGER_GRIP			false

#define REV_KEY_TOGGLE_DELAY				"ToggleDelay"
#define REV_DEFAULT_TOGGLE_DELAY			0.5f

#define REV_KEY_TOUCH_PITCH					"TouchPitch"
#define REV_DEFAULT_TOUCH_PITCH				-36.0f

#define REV_KEY_TOUCH_YAW					"TouchYaw"
#define REV_DEFAULT_TOUCH_YAW				0.0f

#define REV_KEY_TOUCH_ROLL					"TouchRoll"
#define REV_DEFAULT_TOUCH_ROLL				0.0f

#define REV_KEY_TOUCH_X						"TouchX"
#define REV_DEFAULT_TOUCH_X					0.016f

#define REV_KEY_TOUCH_Y						"TouchY"
#define REV_DEFAULT_TOUCH_Y					-0.036f

#define REV_KEY_TOUCH_Z						"TouchZ"
#define REV_DEFAULT_TOUCH_Z					0.016f

#define REV_KEY_IGNORE_ACTIVITYLEVEL		"IgnoreActivityLevel"
#define REV_DEFAULT_IGNORE_ACTIVITYLEVEL	false

#define REV_KEY_INPUT_SCRIPT				"InputScript"
#define REV_DEFAULT_INPUT_SCRIPT			"default.lua"
