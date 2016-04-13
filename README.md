# Revive Compatibility Layer

This is a proof-of-concept compatibility layer between the Oculus SDK and OpenVR.

# Installation

Currently two games have been tested and are supported, it's important to note that they both require you to use an Xbox controller for now.

## Lucky's Tale

1. [Download the Lucky's Tale patch here.](https://github.com/LibreVR/Revive/releases/download/0.2/ReviveLT.zip)

2. Install Lucky's Tale from Oculus Home, then go to `C:\Program Files (x86)\Oculus\Software\playful-luckys-tale`.

3. Extract the patch in that folder, it will overwrite `LT_Data\Plugins\OVRPlugin.dll` so make sure you have a backup.

4. Make sure SteamVR is running and then start `LT.exe`, for convenience you can also add `LT.exe` as a non-steam game to your SteamVR dashboard.

## Oculus Dreamdeck

Credit goes to @rjoudrey for implementing the injector.

1. [Download the Revive Injector here.](https://github.com/LibreVR/Revive/releases/download/0.2/ReviveInjector.zip)

2. Install Oculus Dreamdeck from Oculus Home, then go to `C:\Program Files (x86)\Oculus\Software\oculus-dreamdeck\WindowsNoEditor\Dreamdeck\Binaries\Win64`.

3. Extract the injector in that folder.

4. Make sure SteamVR is running and then drag `Dreamdeck-Win64-Shipping.exe` into `ReviveInjector.exe`.

5. For convenience you can also add `ReviveInjector.exe` as a non-steam game, but be sure to set `Dreamdeck-Win64-Shipping.exe` in the launch options.

# Implementation

It works by reimplementing functions from the Oculus Runtime and translating them to OpenVR calls.
Unfortunately Oculus has implemented a Code Signing check on the Runtime DLLs, therefore the Revive DLLs
cannot be used unless the application is patched.

The download includes a patched version of the OculusRoomTiny example to show it can correctly communicate with OpenVR.

The Revive DLLs already contain the necessary hooking code to work around the Code Signing check in any application.
However you will still need to patch the application to actually load the Revive DLLs.

# To-Do List
- Implement OpenGL and DX12 support
- Implement Oculus Touch support
- Support games that use Oculus Platform online functionality
