# Revive Compatibility Layer

This is a proof-of-concept compatibility layer between the Oculus SDK and OpenVR.

*WARNING:* This is still pre-release software, don't buy games from the Oculus Store expecting this layer to work for you. Even for the games that have installation instructions there may still be compatibility issues or it may not work at all for you.

You can find a [community-compiled list of working games on the wiki](https://github.com/LibreVR/Revive/wiki/Compatibility-list), feel free to add your own results.

# Installation

*You need to have [Oculus Home](https://www.oculus.com/en-us/setup/) installed.*

If you need to recenter the headset you can do so in the Steam VR dashboard by holding down the `Dashboard` button and selecting `Recenter seated position`.

Newly installed games will give you an `Entitlement check failed` error until you reboot the Oculus Runtime Service, quickest way to do that is through Oculus Home `Settings > Beta > Restart Oculus`.

Also, you may need to enable unknown sources by going to `Settings > General` and toggling `Unknown Sources` on.

## Oculus Home games

1. Install Oculus Home and download the games you want to play.
2. Download the [Revive installer here.](https://github.com/LibreVR/Revive/releases/download/0.5/ReviveInstaller.exe)
3. Install Revive in your preferred directory.
4. Start or reboot SteamVR.
5. Open the dashboard and click the new Revive tab.

## Standalone games

Credit goes to @rjoudrey for implementing the injector.

1. Download the [Revive Injector here.](https://github.com/LibreVR/Revive/releases/download/0.5/ReviveInjector.zip)
2. Go to the installation directory of the game.
3. Find the main executable for the game. For Unreal Games the executable ends with `...-Shipping.exe`.
4. Extract all Revive Injector files next to the executable.
5. Make sure SteamVR is running and then drag main executable onto `ReviveInjector_x64.exe`.

# Controls

## Xbox Controller

Most games are designed around the Xbox controller, so that remains the preferred input device for all Oculus Home games.

The controls are self-explanatory.

## Oculus Remote (Single Vive controller)

Some games have support for the Oculus Remote instead of the Oculus Touch. If so, you can turn off one of your Vive controllers and use the remaining one as the Oculus Remote.

When only one Vive controller is connected it will be used as an Oculus Remote. The remote maps the touchpad as a DPad with an `Enter` button in the middle, just like the Oculus Remote. The Application Menu serves as the `Back` button.

## Touch Controllers (Both Vive controllers)

When both Vive controllers are connected they will be used to emulate the Oculus Touch controllers. Many games allow you to use the Oculus Touch in place of the Xbox controller if you don't have one.

The Vive controllers have been mapped a little bit differently to accommodate the different designs of the controllers. The touchpad functions both as buttons and as the thumbstick, you can switch between them using the `Application Menu` button.

In Thumbstick Mode you move your finger across the touchpad to move the thumbstick and press down to press the thumbstick. In Button Mode the touchpad is divided into four diagonal areas, each mapped to the buttons `A`, `B`, `X`, `Y` press down on the touchpad to press the buttons.

By default the left controller starts in Thumbstick Mode and the right controller starts in Button Mode. For most games you will not have to switch between these modes.

# Implementation

It works by reimplementing functions from the Oculus Runtime and translating them to OpenVR calls.
Unfortunately Oculus has implemented a Code Signing check on the Runtime DLLs, therefore the Revive DLLs
cannot be used unless the application is patched.

The Revive DLLs already contain the necessary hooking code to work around the Code Signing check in any application.
However you will still need to patch the application to actually load the Revive DLLs.

# To-Do List
- Translate Oculus Touch haptic feedback to the Vive Controllers.
- Add render models for the Vive controller to show the Oculus Touch mapping.
- Implement DX12 support
