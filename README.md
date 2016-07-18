# Revive Compatibility Layer

This is a compatibility layer between the Oculus SDK and OpenVR. It allows you to play Oculus-exclusive games on your HTC Vive.

You can find a [community-compiled list of working games on the wiki](https://github.com/LibreVR/Revive/wiki/Compatibility-list), feel free to add your own results. There's also a [troubleshooting page](https://github.com/LibreVR/Revive/wiki/Troubleshooting) if you run into any problems.

# Installation

*You need to have [Oculus Home](https://www.oculus.com/en-us/setup/) installed.*

If you need to recenter the headset you can do so in the Steam VR dashboard by holding down the `Dashboard` button and selecting `Recenter seated position`.

Newly installed games will give you an `Entitlement check failed` error until you reboot the Oculus service, quickest way to do that is through Oculus Home `Settings > Beta > Restart Oculus`.


## Oculus Home games

1. Install Oculus Home and remember to skip the first-time setup.
2. Download the games you want to play, check the [compatibility list](https://github.com/LibreVR/Revive/wiki/Compatibility-list) before making a purchase.
3. Download the [Revive installer here.](https://github.com/LibreVR/Revive/releases/download/0.8.6/ReviveInstaller.exe)
4. Install Revive in your preferred directory, you can overwrite existing installations without uninstalling Revive first.
5. Start or reboot SteamVR.
6. Open the dashboard and click the new Revive tab.

*If you don't see the Revive tab, go to the start menu on your desktop and start the Revive Dashboard.*

## Standalone games (including the Steam version of Dirt Rally)

Credit goes to @rjoudrey for implementing the injector.

1. Install Oculus Home and remember to skip the first-time setup.
2. Download the [Revive Injector here.](https://github.com/LibreVR/Revive/releases/download/0.8.6/ReviveInjector.zip)
3. Extract all files to a folder.
4. Go to the installation directory for the game.
5. Find the main executable. For Unreal Games the executable ends with `...-Shipping.exe`.
6. Make sure SteamVR is running and then drag main executable onto `ReviveInjector_x64.exe`.

## Steam games

Revive is not yet compatible with Steam, but for some games the following may work.

1. Install Oculus Home and remember to skip the first-time setup.
2. [Download the Revive patch here.](https://github.com/LibreVR/Revive/releases/download/0.8.6/RevivePatch.zip)
3. Go to the properties of the game in the Steam Library.
4. Uncheck "Use Desktop Game Theatre while SteamVR is active".
5. Go to the Local Files tab and click "Browse Local Files...".
6. Extract the patch into the folder that opened next to the executable.
7. Make sure SteamVR is running and start the game.
8. A dialog will incorrectly state the game does not support VR, ignore that and press OK.

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

![logo](revive_black.png)
