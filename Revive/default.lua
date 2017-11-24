-- Some definitions for buttons identifiers

-- Oculus buttons
-- A button on XBox controllers and right Touch controller. Select button on Oculus Remote.
ovrButton_A = 0x00000001
-- B button on XBox controllers and right Touch controller. Back button on Oculus Remote.
ovrButton_B = 0x00000002
-- Right thumbstick on XBox controllers and Touch controllers. Not present on Oculus Remote.
ovrButton_RThumb = 0x00000004
-- Right shoulder button on XBox controllers. Not present on Touch controllers or Oculus Remote.
ovrButton_RShoulder = 0x00000008
-- X button on XBox controllers and left Touch controller. Not present on Oculus Remote.
ovrButton_X = 0x00000100
-- Y button on XBox controllers and left Touch controller. Not present on Oculus Remote.
ovrButton_Y = 0x00000200
-- Left thumbstick on XBox controllers and Touch controllers. Not present on Oculus Remote.
ovrButton_LThumb = 0x00000400
-- Left shoulder button on XBox controllers. Not present on Touch controllers or Oculus Remote.
ovrButton_LShoulder = 0x00000800
-- Up button on XBox controllers and Oculus Remote. Not present on Touch controllers.
ovrButton_Up = 0x00010000
-- Down button on XBox controllers and Oculus Remote. Not present on Touch controllers.
ovrButton_Down = 0x00020000
-- Left button on XBox controllers and Oculus Remote. Not present on Touch controllers.
ovrButton_Left = 0x00040000
-- Right button on XBox controllers and Oculus Remote. Not present on Touch controllers.
ovrButton_Right = 0x00080000
-- Start on XBox 360 controller. Menu on XBox One controller and Left Touch controller.
-- Should be referred to as the Menu button in user-facing documentation.
ovrButton_Enter = 0x00100000
-- Back on Xbox 360 controller. View button on XBox One controller. Not present on Touch
-- controllers or Oculus Remote.
ovrButton_Back = 0x00200000
-- Volume button on Oculus Remote. Not present on XBox or Touch controllers.
ovrButton_VolUp = 0x00400000
-- Volume button on Oculus Remote. Not present on XBox or Touch controllers.
ovrButton_VolDown = 0x00800000
-- Home button on XBox controllers. Oculus button on Touch controllers and Oculus Remote.
ovrButton_Home = 0x01000000

ovrTouch_A = ovrButton_A
ovrTouch_B = ovrButton_B
ovrTouch_RThumb = ovrButton_RThumb
ovrTouch_RThumbRest = 0x00000008
ovrTouch_RIndexTrigger = 0x00000010
ovrTouch_X = ovrButton_X
ovrTouch_Y = ovrButton_Y
ovrTouch_LThumb = ovrButton_LThumb
ovrTouch_LThumbRest = 0x00000800
ovrTouch_LIndexTrigger = 0x00001000

-- Finger pose state
-- Derived internally based on distance, proximity to sensors and filtering.
ovrTouch_RIndexPointing = 0x00000020
ovrTouch_RThumbUp = 0x00000040
ovrTouch_LIndexPointing = 0x00002000
ovrTouch_LThumbUp = 0x00004000

-- Indices for well known controls
SteamVR_Touchpad  = 1
SteamVR_Trigger   = 2

-- The actual input script starts here
state = {}

-- Helper function
function ButtonFromQuadrant(axis, right_hand)
  if (right_hand) then
    if (axis.y < -axis.x) then
      if (axis.y < axis.x) then
        return ovrButton_A;
      else
        return ovrButton_B;
      end
    else
      return ovrButton_RThumb;
    end
  else
    if (axis.y < axis.x) then
      if (axis.y < -axis.x) then
        return ovrButton_X
      else
        return ovrButton_Y
      end
    else
      return ovrButton_LThumb
    end
  end
end

function GetButtons(right_hand)
  buttons = {}

  if (state.ApplicationMenu.pressed) then
    table.insert(buttons, ovrButton_Enter)
  end

  if (state.A.pressed) then
    table.insert(buttons, right_hand and ovrButton_A or ovrButton_X)
  end

  if (state.B.pressed) then
    table.insert(buttons, right_hand and ovrButton_B or ovrButton_Y)
  end

  if (state[SteamVR_Touchpad].pressed) then
    table.insert(buttons, ButtonFromQuadrant(state[SteamVR_Touchpad], right_hand))
  end

  return unpack(buttons)
end

function GetTouches(right_hand)
  touches = {}

  if (state.A.touched) then
    table.insert(touches, right_hand and ovrTouch_A or ovrTouch_X)
  end

  if (state.B.touched) then
    table.insert(touches, right_hand and ovrTouch_B or ovrTouch_Y)
  end

  if (state[SteamVR_Touchpad].touched) then
    table.insert(touches, ButtonFromQuadrant(state[SteamVR_Touchpad], right_hand))
  elseif (state.Grip.pressed) then
    table.insert(touches, right_hand and ovrTouch_RThumbUp or ovrTouch_LThumbUp)
  end

  if (state[SteamVR_Trigger].touched) then
    table.insert(touches, right_hand and ovrTouch_RIndexTrigger or ovrTouch_LIndexTrigger)
  elseif (state.Grip.pressed) then
    table.insert(touches, right_hand and ovrTouch_RIndexPointing or ovrTouch_LIndexPointing)
  end

  return unpack(touches)
end

local was_released = true
local center = {x=0, y=0}
function GetThumbstick(right_hand, deadzone)
  axis = state[SteamVR_Touchpad];

  --[[
  todo: find a physical joystick

  magnitude = math.sqrt(axis.x^2 + axis.y^2)
  if (magnitude < deadzone) then
    return 0,0
  else
    normalize = (magnitude - deadzone) / (1 - deadzone)
    return axis.x / magnitude * normalize, axis.y / magnitude * normalize
  end
  --]]

  if (not axis.touched) then
    -- we're not touching the touchpad
    return 0, 0
  elseif was_released then
    -- immediately recenter the virtual thumbstick if the touchpad is touched
    center.x = axis.x
    center.y = axis.y
  end
  was_released = not axis.touched

  local out = {x=axis.x, y=axis.y}

  --[[
  -- account for the center
  out.x = math.min(1, axis.x - center.x)
  out.y = math.min(1, axis.y - center.y)

  -- update the center so it moves if we go outside the range
  center.x = axis.x - out.x
  center.y = axis.y - out.y
  --]]

  deadZoneLow = deadzone
  deadZoneHigh = deadzone / 2

  mag = math.sqrt(out.x^2 + out.y^2)
  if (mag > deadZoneLow) then
    -- scale such that output magnitude is in the range [0, 1]
    legalRange = 1 - deadZoneHigh - deadZoneLow
    normalizedMag = math.min(1, (mag - deadZoneLow) / legalRange)
    scale = normalizedMag / mag
    return out.x * scale, out.y * scale
  else
    -- stick is in the inner dead zone
    return 0, 0
  end
end

local gripped = false
local hybrid_time = 0
function GetTriggers(right_hand, grip_mode)
  -- Allow users to enable a toggled grip.
  if (grip_mode.hybrid) then
    -- In hybrid grip mode the user will toggle the grip if it has been released within the delay time.
    if (state.Grip.pressed) then
      -- Only set the timestamp on the first grip toggle, we don't want to toggle twice.
      if (hybrid_time == 0) then
        hybrid_time = os.clock() + grip_mode.delay
      end
      gripped = true
    else
      -- If the user releases the grip after the delay has passed, then we can release the grip normally.
      if (os.clock() > hybrid_time) then
        gripped = false
        hybrid_time = 0
      end
    end
  elseif (grip_mode.toggle) then
    if (state.Grip.pressed) then
      gripped = not gripped
    end
  else
    gripped = state.Grip.pressed
  end

  if (grip_mode.trigger) then
    -- Some users prefer the trigger and grip to be swapped.
    return gripped and 1 or 0, state[SteamVR_Trigger].x
  else
    -- When we release the grip we need to keep it just a little bit pressed.
    -- This is necessary because Toybox can't handle a sudden jump to zero.
    return state[SteamVR_Trigger].x, gripped and 1 or 0.01
  end
end
