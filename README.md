# Revive Compatibility Layer

This is a proof-of-concept compatibility layer between the Oculus SDK and OpenVR.

*WARNING:* This is still pre-release software, don't buy games from the Oculus Store expecting this layer to work for you. Even for the games that have installation instructions there may still be compatibility issues or it may not work at all for you.

# Installation

*You need to have [Oculus Home](https://www.oculus.com/en-us/setup/) installed.*

If you need to recenter the headset you can do so in the Steam VR dashboard by holding down the `Dashboard` button and selecting `Recenter seated position`.

Game-specific patches are no longer supported (except for Project Cars), all Oculus Home games should now be playable using the Revive Injector.

## Oculus Home games

Credit goes to @rjoudrey for implementing the injector.

1. Download the [Revive Injector here.](https://github.com/LibreVR/Revive/releases/download/0.4/ReviveInjector.zip)
2. Go to the installation directory of the game in `C:\Program Files (x86)\Oculus\Software`.
3. Find the main executable for the game. For Unreal Games the executable ends with `...-Shipping.exe`.
3. Extract `ReviveInjector.exe` and the `Revive` folder next to the executable.
4. Make sure SteamVR is running and then drag main executable onto `ReviveInjector.exe`.

## Project Cars (Steam, 64-bit)

*You need to have [Oculus Home](https://www.oculus.com/en-us/setup/) installed.*

1. [Download the Project Cars patch here.](https://github.com/LibreVR/Revive/releases/download/0.4/RevivePCars.zip)
2. Install Project Cars from Steam, then go to its properties in the Steam Library.
3. Uncheck "Use Desktop Game Theatre while SteamVR is active".
4. Go to the Local Files tab and click "Browse Local Files...".
5. Extract the patch into the folder that opened.
6. Make sure SteamVR is running and then start `pCARS64.exe`
7. A dialog will incorrectly state that Project Cars does not support VR, ignore that and press OK.

# Controls

## Xbox Controller

Most games are designed around the Xbox controller, so that remains the preferred input device for all Oculus Home games.

The controls are self-explanatory.

## Touch (Vive) Controllers

When both Vive controllers are connected they will be used to emulate the Oculus Touch controllers. These controllers can be used in place of the Xbox controller if you don't have one.

The Vive controllers have been mapped a little bit differently to accommodate the different designs of the controllers. The touchpad functions both as buttons and as the thumbstick, you can switch between them using the `Application Menu` button.

In Thumbstick Mode you move your finger across the touchpad to move the thumbstick and press down to press the thumbstick. In Button Mode the touchpad is divided into four diagonal areas, each mapped to the buttons `A`, `B`, `X`, `Y` press down on the touchpad to press the buttons.

By default the left controller starts in Thumbstick Mode and the right controller starts in Button Mode. For most games you will not have to switch between these modes.

## Oculus Remote

When only one Vive controller is connected it will be used as an Oculus Remote. The remote maps the touchpad as a DPad with an `Enter` button in the middle, just like the Oculus Remote. The Application Menu serves as the `Back` button.

# Implementation

It works by reimplementing functions from the Oculus Runtime and translating them to OpenVR calls.
Unfortunately Oculus has implemented a Code Signing check on the Runtime DLLs, therefore the Revive DLLs
cannot be used unless the application is patched.

The Revive DLLs already contain the necessary hooking code to work around the Code Signing check in any application.
However you will still need to patch the application to actually load the Revive DLLs.

# To-Do List
- Implement OpenGL and DX12 support
