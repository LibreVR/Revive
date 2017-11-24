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

-- The controller state global
state = {}
