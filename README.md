# Revive Compatibility Layer

This is a proof-of-concept compatibility layer between the Oculus SDK and OpenVR.

It works by reimplementing functions from the Oculus Runtime and translating them to OpenVR calls.
Unfortunately Oculus has implemented a Code Signing check on the Runtime DLLs, therefore the Revive DLLs
cannot be used unless the application is patched.

The download includes a patched version of the OculusRoomTiny example to show it can correctly communicate with OpenVR.

The Revive DLLs already contain the necessary hooking code to work around the Code Signing check in any application.
However you will still need to patch the application to actually load the Revive DLLs.

# To-Do List
- Fix stereoscopy in Unity Engine games
- Implement OpenGL and DX12 support
- Implement Oculus Touch support
