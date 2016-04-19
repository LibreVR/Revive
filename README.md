# Revive Compatibility Layer

This is a proof-of-concept compatibility layer between the Oculus SDK and OpenVR.

*WARNING:* This is still pre-release software, don't buy games from the Oculus Store expecting this layer to work for you. Even for the games that have installation instructions there may still be compatability issues or it may not work at all for you.

# Installation

Currently two games have been tested and are supported, it's important to note that they both require you to use an Xbox controller for now.

Only Windows 8/10 is supported right now, support for Windows 7 will be added in the next version.

If you need to recenter the headset you can do so in the Steam VR dashboard by going to Settings > VR Settings > General VR Settings > Reset Seated Position.

## Project Cars (64-bit)

1. [Download the Project Cars patch here.](https://github.com/LibreVR/Revive/releases/download/0.3/RevivePCars.zip)
2. Install Project Cars from Steam, then go to its properties in the Steam Library.
3. Uncheck "Use Desktop Game Theatre while SteamVR is active".
4. Go to the Local Files tab and click "Browse Local Files...".
5. Extract the patch into the folder that opened.
6. Make sure SteamVR is running and then start `pCARS64.exe`
7. A dialog will incorrectly state that Project Cars does not support VR, ignore that and press OK.

## Defense Grid 2

1. [Download the Revive Injector here.](https://github.com/LibreVR/Revive/releases/download/0.3/ReviveInjector.zip)
2. Install Defense Grid 2 from Oculus Home, then go to `C:\Program Files (x86)\Oculus\Software\hidden-path-entertainment-dg2vr`.
3. Extract the injector in that folder.
4. Make sure SteamVR is running and then drag `DefenseGrid2_Shipping.exe` into `ReviveInjector.exe`.

## Lucky's Tale

1. [Download the Lucky's Tale patch here.](https://github.com/LibreVR/Revive/releases/download/0.3/ReviveLT.zip)
2. Install Lucky's Tale from Oculus Home, then go to `C:\Program Files (x86)\Oculus\Software\playful-luckys-tale`.
3. Extract the patch in that folder, it will overwrite `LT_Data\Plugins\OVRPlugin.dll` so make sure you have a backup.
4. Make sure SteamVR is running and then start `LT.exe`.

## Oculus Dreamdeck

Credit goes to @rjoudrey for implementing the injector.

1. [Download the Revive Injector here.](https://github.com/LibreVR/Revive/releases/download/0.3/ReviveInjector.zip)
2. Install Oculus Dreamdeck from Oculus Home, then go to `C:\Program Files (x86)\Oculus\Software\oculus-dreamdeck\WindowsNoEditor\Dreamdeck\Binaries\Win64`.
3. Extract the injector in that folder.
4. Make sure SteamVR is running and then drag `Dreamdeck-Win64-Shipping.exe` into `ReviveInjector.exe`.

# Implementation

It works by reimplementing functions from the Oculus Runtime and translating them to OpenVR calls.
Unfortunately Oculus has implemented a Code Signing check on the Runtime DLLs, therefore the Revive DLLs
cannot be used unless the application is patched.

The Revive DLLs already contain the necessary hooking code to work around the Code Signing check in any application.
However you will still need to patch the application to actually load the Revive DLLs.

# To-Do List
- Implement OpenGL and DX12 support
- Implement Oculus Touch support
