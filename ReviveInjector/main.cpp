#include "inject.h"

#include <Windows.h>
#include <stdio.h>
#include <string.h>
#include <Shlobj.h>
#include <Shlwapi.h>
#include <openvr.h>

FILE* g_LogFile = NULL;

bool GetOculusBasePath(PWCHAR path, DWORD length)
{
	LONG error = ERROR_SUCCESS;

	HKEY oculusKey;
	error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Oculus VR, LLC\\Oculus", 0, KEY_READ | KEY_WOW64_32KEY, &oculusKey);
	if (error != ERROR_SUCCESS)
	{
		LOG("Unable to open Oculus key.");
		return false;
	}
	error = RegQueryValueEx(oculusKey, L"Base", NULL, NULL, (PBYTE)path, &length);
	if (error != ERROR_SUCCESS)
	{
		LOG("Unable to read Base path.");
		return false;
	}
	RegCloseKey(oculusKey);

	return true;
}

bool GetDefaultLibraryPath(PWCHAR path, DWORD length)
{
	LONG error = ERROR_SUCCESS;

	// Open the libraries key
	WCHAR keyPath[MAX_PATH] = { L"Software\\Oculus VR, LLC\\Oculus\\Libraries\\" };
	HKEY oculusKey;
	error = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &oculusKey);
	if (error != ERROR_SUCCESS)
	{
		LOG("Unable to open Libraries key.");
		return false;
	}

	// Get the default library
	WCHAR guid[40] = { L'\0' };
	DWORD guidSize = sizeof(guid);
	error = RegQueryValueExW(oculusKey, L"DefaultLibrary", NULL, NULL, (PBYTE)guid, &guidSize);
	RegCloseKey(oculusKey);
	if (error != ERROR_SUCCESS)
	{
		LOG("Unable to read DefaultLibrary guid.");
		return false;
	}

	// Open the default library key
	wcsncat(keyPath, guid, MAX_PATH);
	error = RegOpenKeyExW(HKEY_CURRENT_USER, keyPath, 0, KEY_READ, &oculusKey);
	if (error != ERROR_SUCCESS)
	{
		LOG("Unable to open Library path key.");
		return false;
	}

	// Get the volume path to this library
	DWORD pathSize;
	error = RegQueryValueExW(oculusKey, L"Path", NULL, NULL, NULL, &pathSize);
	PWCHAR volumePath = (PWCHAR)malloc(pathSize);
	error = RegQueryValueExW(oculusKey, L"Path", NULL, NULL, (PBYTE)volumePath, &pathSize);
	RegCloseKey(oculusKey);
	if (error != ERROR_SUCCESS)
	{
		free(volumePath);
		LOG("Unable to read Library path.");
		return false;
	}

	// Resolve the volume path to a mount point
	DWORD total;
	WCHAR volume[50] = { L'\0' };
	wcsncpy(volume, volumePath, 49);
	GetVolumePathNamesForVolumeNameW(volume, path, length, &total);
	wcsncat(path, volumePath + 49, MAX_PATH);
	free(volumePath);

	return true;
}

typedef uint32_t (VR_CALLTYPE* VR_InitInternal2Ptr)(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo);
typedef void (VR_CALLTYPE* VR_ShutdownInternalPtr)(void);

bool IsSteamVRRunning()
{
	char dllPath[MAX_PATH];
	GetLibraryPath(dllPath, MAX_PATH, "openvr_api.dll");
	HMODULE openvr = LoadLibraryA(dllPath);
	if (!openvr)
		return false;

	vr::EVRInitError err = vr::VRInitError_Unknown;
	VR_InitInternal2Ptr init = (VR_InitInternal2Ptr)GetProcAddress(openvr, "VR_InitInternal2");
	VR_ShutdownInternalPtr shutdown = (VR_ShutdownInternalPtr)GetProcAddress(openvr, "VR_ShutdownInternal");
	if (init)
		init(&err, vr::VRApplication_Background, nullptr);

	if (shutdown && err == vr::VRInitError_None)
		shutdown();

	FreeLibrary(openvr);
	return err == vr::VRInitError_None;
}

int wmain(int argc, wchar_t *argv[]) {
	if (argc < 2) {
		printf("usage: ReviveInjector.exe [/handle] <process path/process handle>\n");
		return -1;
	}

	WCHAR LogPath[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, LogPath)))
	{
		wcsncat(LogPath, L"\\Revive", MAX_PATH);
		
		BOOL exists = PathFileExists(LogPath);
		if (!exists)
			exists = CreateDirectory(LogPath, NULL);

		wcsncat(LogPath, L"\\ReviveInjector.txt", MAX_PATH);
		if (exists)
			g_LogFile = _wfopen(LogPath, L"w");
	}

	LOG("Launched injector with: %ls\n", GetCommandLine());

	bool xr = !IsSteamVRRunning();
	WCHAR path[MAX_PATH] = { 0 };
	for (int i = 1; i < argc; i++)
	{
		if (wcscmp(argv[i], L"/xr") == 0)
		{
			xr = true;
		}
		else if (wcscmp(argv[i], L"/revive") == 0)
		{
			xr = false;
		}
		else if (wcscmp(argv[i], L"/handle") == 0)
		{
			return OpenProcessAndInject(argv[++i], xr);
		}
		else if (wcscmp(argv[i], L"/base") == 0)
		{
			if (!GetOculusBasePath(path, MAX_PATH))
				return -1;
			wnsprintf(path, MAX_PATH, L"%s\\%s ", path, argv[++i]);
		}
		else if (wcscmp(argv[i], L"/library") == 0)
		{
			if (!GetDefaultLibraryPath(path, MAX_PATH))
				return -1;
			wnsprintf(path, MAX_PATH, L"%s\\%s ", path, argv[++i]);
		}
		else
		{
			// Concatenate all other arguments
			wcsncat(path, argv[i], MAX_PATH);
			wcsncat(path, L" ", MAX_PATH);
		}
	}

	return CreateProcessAndInject(path, xr);
}
