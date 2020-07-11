# Building Revive

For your convenience, there is a script to help retrieve and set up
dependencies in the project root. This script will also attempt to build OpenXR
and Revive.

Instructions for both scripted and manual build are below.

## Script

- Clone this repository:
  ```
  git clone git@github.com:LibreVR/Revive.git
  ```
- Install Visual Studio 2017, CMake, and Git, and ensure all three are in your PATH from within PowerShell.
- Run the setup script (PowerShell):
  ```
  cd Revive
  .\setup.ps1
  ```

## Manual

Before the project can be built, you must retrieve and set up vendored dependencies.

In bash for Windows:

```
git clone git@github.com:LibreVR/Revive.git
git submodule update --init --recursive
```

Download the Oculus SDK for Windows
[here](https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/)
and place it in `Revive/Externals/'. Then:

```
cd Revive/Externals
unzip ovr_sdk_win_<version>.zip
```

The core Revive library project should then build normally in VS.

To build ReviveInjector, you need to build ReviveXR. To build ReviveXR, you
need to build the OpenXR SDK :)

1. Download and install [Cmake for Windows](https://cmake.org/download/).
2. In Powershell, `cd` to `Revive/Externals/openxr` and do the following:

```
cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio [VS Version] [arch]"
```

For instance, under VS 2017 on 64-bit Windows the build target would be
`Visual Studio 15 2017 Win64`.

3. Open the generated `OPENXR.sln` in VS
4. In the C/C++ properties for project `openxr_loader-1_0`, change `Runtime
   Library` to `Multi-threaded Debug (/MTd)`
5. Build the solution. This will perform an in-tree build of the OpenXR sdk and
   generate the loader library.
6. ReviveXR and ReviveInjector should now build normally.

Notes:

- You must build OpenXR with the same configuration (Debug / Release)
  that you build ReviveInjector and ReviveXR in to satisfy configured linker
  paths
- Out of tree openxr builds will not work for the same reason, you must build
  in tree
