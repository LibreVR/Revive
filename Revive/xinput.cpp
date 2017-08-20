#include <Windows.h>
#include <Xinput.h>

bool GetOculusBasePath(PWCHAR path, DWORD length)
{
	LONG error = ERROR_SUCCESS;

	HKEY oculusKey;
	error = RegOpenKeyEx(HKEY_LOCAL_MACHINE, L"Software\\Oculus VR, LLC\\Oculus", 0, KEY_READ | KEY_WOW64_32KEY, &oculusKey);
	if (error != ERROR_SUCCESS)
		return false;
	error = RegQueryValueEx(oculusKey, L"Base", NULL, NULL, (PBYTE)path, &length);
	if (error != ERROR_SUCCESS)
		return false;
	RegCloseKey(oculusKey);

	return true;
}

HMODULE GetXInputModule()
{
	static HMODULE module = NULL;
	if (module)
		return module;

	wchar_t XInputPath[MAX_PATH];
	if (GetOculusBasePath(XInputPath, MAX_PATH))
	{
		wcsncat(XInputPath, L"Support\\oculus-runtime\\xinput1_3.dll", MAX_PATH);
		module = LoadLibraryW(XInputPath);
		if (module)
			return module;
	}

    if (GetWindowsDirectory(XInputPath, MAX_PATH))
    {
        wcsncat(XInputPath, L"\\System32\\xinput1_3.dll", MAX_PATH);
        module = LoadLibraryW(XInputPath);
    }
	return module;
}

typedef DWORD(__stdcall* _XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	static _XInputGetState func = NULL;
	if (!func)
		func = (_XInputGetState)GetProcAddress(GetXInputModule(), "XInputGetState");
	return func(dwUserIndex, pState);
}

typedef DWORD(__stdcall* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
	static _XInputSetState func = NULL;
	if (!func)
		func = (_XInputSetState)GetProcAddress(GetXInputModule(), "XInputSetState");
	return func(dwUserIndex, pVibration);
}

typedef DWORD(__stdcall* _XInputGetCapabilities)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities)
{
	static _XInputGetCapabilities func = NULL;
	if (!func)
		func = (_XInputGetCapabilities)GetProcAddress(GetXInputModule(), "XInputGetCapabilities");
	return func(dwUserIndex, dwFlags, pCapabilities);
}

#pragma warning(push)
#pragma warning(disable:4995)
typedef void(__stdcall* _XInputEnable)(BOOL enable);
void WINAPI XInputEnable(BOOL enable)
{
	static _XInputEnable func = NULL;
	if (!func)
		func = (_XInputEnable)GetProcAddress(GetXInputModule(), "XInputEnable");
	return func(enable);
}
#pragma warning(pop)

typedef DWORD(__stdcall* _XInputGetDSoundAudioDeviceGuids)(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);
DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid)
{
	static _XInputGetDSoundAudioDeviceGuids func = NULL;
	if (!func)
		func = (_XInputGetDSoundAudioDeviceGuids)GetProcAddress(GetXInputModule(), "XInputGetDSoundAudioDeviceGuids");
	return func(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid);
}

typedef DWORD(__stdcall* _XInputGetBatteryInformation)(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation);
DWORD WINAPI XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation)
{
	static _XInputGetBatteryInformation func = NULL;
	if (!func)
		func = (_XInputGetBatteryInformation)GetProcAddress(GetXInputModule(), "XInputGetBatteryInformation");
	return func(dwUserIndex, devType, pBatteryInformation);
}

typedef DWORD(__stdcall* _XInputGetKeystroke)(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke);
DWORD WINAPI XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke)
{
	static _XInputGetKeystroke func = NULL;
	if (!func)
		func = (_XInputGetKeystroke)GetProcAddress(GetXInputModule(), "XInputGetKeystroke");
	return func(dwUserIndex, dwReserved, pKeystroke);
}
