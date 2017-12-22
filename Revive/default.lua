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

  if (string.match(controller_model, "Knuckles")) then
    if (state.ApplicationMenu.pressed and state.Grip.pressed) then
      table.insert(buttons, ovrButton_Enter)
    else
      if (state.Grip.pressed) then
        table.insert(buttons, right_hand and ovrButton_A or ovrButton_X)
      end

      if (state.ApplicationMenu.pressed) then
        table.insert(buttons, right_hand and ovrButton_B or ovrButton_Y)
      end
    end

    if (state[SteamVR_Touchpad].pressed) then
      table.insert(buttons, right_hand and ovrButton_RThumb or ovrButton_LThumb)
    end

    return unpack(buttons)
  end

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

  if (string.match(controller_model, "Knuckles")) then
    if (state.Grip.touched) then
      table.insert(touches, right_hand and ovrTouch_A or ovrTouch_X)
    end

    if (state.ApplicationMenu.touched) then
      table.insert(touches, right_hand and ovrTouch_B or ovrTouch_Y)
    end

    if (state[SteamVR_Touchpad].touched) then
      table.insert(touches, right_hand and ovrTouch_RThumb or ovrTouch_LThumb)
    elseif (not state.Grip.touched and not state.ApplicationMenu.touched) then
      table.insert(touches, right_hand and ovrTouch_RThumbUp or ovrTouch_LThumbUp)
    end

    if (state[SteamVR_Trigger].touched) then
      table.insert(touches, right_hand and ovrTouch_RIndexTrigger or ovrTouch_LIndexTrigger)
    elseif (state[4].x < 0.8) then
      table.insert(touches, right_hand and ovrTouch_RIndexPointing or ovrTouch_LIndexPointing)
    end

    return unpack(touches)
  end

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

function ApplyDeadzone(axis, deadZoneLow, deadZoneHigh)
  local mag = math.sqrt(axis.x^2 + axis.y^2)
  if (mag > deadZoneLow) then
    -- scale such that output magnitude is in the range [0, 1]
    legalRange = 1 - deadZoneHigh - deadZoneLow
    normalizedMag = math.min(1, (mag - deadZoneLow) / legalRange)
    scale = normalizedMag / mag
    return axis.x * scale, axis.y * scale
  else
    -- stick is in the inner dead zone
    return 0, 0
  end
end

local was_touched = false
local center = {x=0, y=0}
function GetThumbstick(right_hand, deadzone)
  if (string.match(controller_model, "Knuckles")) then
    -- For the knuckles we use the trackpad and apply a simple radial deadzone
    return ApplyDeadzone(state[SteamVR_Touchpad], deadzone, 0)
  end

  -- Find a physical Joystick, skip the touchpad and trigger
  for i=3,5 do
    if (state[i].type == "Joystick") then
      -- If there is a physical joystick use that axis with a simple radial deadzone
      return ApplyDeadzone(state[i], deadzone, 0)
    elseif (state[i].type == "None") then
      -- This is not a valid axis anymore, so exit the loop
      break
    end
  end

  -- If we can't find a physical joystick, emulate one with the trackpad
  local axis = state[SteamVR_Touchpad]

  if (axis.touched and not was_touched) then
    -- center the virtual thumbstick at the location the touchpad is touched for the first time
    center.x = axis.x
    center.y = axis.y
  end
  was_touched = axis.touched

  if (not axis.touched) then
    -- we're not touching the touchpad
    return 0, 0
  else  
    -- account for the center
    local out = {x=axis.x - center.x, y=axis.y - center.y}
    return ApplyDeadzone(out, deadzone, deadzone / 2)
  end
end

local gripped = false
local was_pressed = false
local hybrid_time = 0
function GetTriggers(right_hand)
  if (string.match(controller_model, "Knuckles")) then
    return state[SteamVR_Trigger].x, state[4].y
  end

  if (state.Grip.pressed ~= was_pressed) then
    -- Allow users to enable a toggled grip.
    if (grip_mode.hybrid) then
      -- In hybrid grip mode the user will toggle the grip if it has been released within the delay time.
      if (state.Grip.pressed) then
        -- Only set the timestamp on when we're not toggled on, we don't want to toggle twice.
        if (not gripped) then
          hybrid_time = time + grip_mode.delay
        end
        gripped = true
      else
        -- If the user releases the grip after the delay has passed, then we can release the grip normally.
        if (time > hybrid_time) then
          gripped = false
        end
        -- Reset the timestamp so we immediately release the next time.
        hybrid_time = 0
      end
    elseif (grip_mode.toggle) then
      -- A simple grip toggle
      if (state.Grip.pressed) then
        gripped = not gripped
      end
    else
      gripped = state.Grip.pressed
    end
  end
  was_pressed = state.Grip.pressed

  if (grip_mode.trigger) then
    -- Some users prefer the trigger and grip to be swapped.
    return gripped and 1 or 0, state[SteamVR_Trigger].x
  else
    -- When we release the grip we need to keep it just a little bit pressed.
    -- This is necessary because Toybox can't handle a sudden jump to zero.
    return state[SteamVR_Trigger].x, gripped and 1 or 0.01
  end
end
