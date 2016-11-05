#include <Windows.h>
#include <Xinput.h>

HMODULE GetXInputModule()
{
	static HMODULE module = NULL;
	if (module)
		return module;

	wchar_t XInputPath[MAX_PATH];
	GetWindowsDirectory(XInputPath, MAX_PATH);
	wcsncat(XInputPath, L"\\System32\\xinput1_3.dll", MAX_PATH);
	module = LoadLibrary(XInputPath);
	return module;
}

typedef DWORD(__stdcall* _XInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
DWORD WINAPI XInputGetState(DWORD dwUserIndex, XINPUT_STATE* pState)
{
	_XInputGetState func = NULL;
	if (!func)
		func = (_XInputGetState)GetProcAddress(GetXInputModule(), "XInputGetState");
	return func(dwUserIndex, pState);
}

typedef DWORD(__stdcall* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);
DWORD WINAPI XInputSetState(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
	_XInputSetState func = NULL;
	if (!func)
		func = (_XInputSetState)GetProcAddress(GetXInputModule(), "XInputSetState");
	return func(dwUserIndex, pVibration);
}

typedef DWORD(__stdcall* _XInputGetCapabilities)(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities);
DWORD WINAPI XInputGetCapabilities(DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES* pCapabilities)
{
	_XInputGetCapabilities func = NULL;
	if (!func)
		func = (_XInputGetCapabilities)GetProcAddress(GetXInputModule(), "XInputGetCapabilities");
	return func(dwUserIndex, dwFlags, pCapabilities);
}

typedef void(__stdcall* _XInputEnable)(BOOL enable);
void WINAPI XInputEnable(BOOL enable)
{
	_XInputEnable func = NULL;
	if (!func)
		func = (_XInputEnable)GetProcAddress(GetXInputModule(), "XInputEnable");
	return func(enable);
}

typedef DWORD(__stdcall* _XInputGetDSoundAudioDeviceGuids)(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid);
DWORD WINAPI XInputGetDSoundAudioDeviceGuids(DWORD dwUserIndex, GUID* pDSoundRenderGuid, GUID* pDSoundCaptureGuid)
{
	_XInputGetDSoundAudioDeviceGuids func = NULL;
	if (!func)
		func = (_XInputGetDSoundAudioDeviceGuids)GetProcAddress(GetXInputModule(), "XInputGetDSoundAudioDeviceGuids");
	return func(dwUserIndex, pDSoundRenderGuid, pDSoundCaptureGuid);
}

typedef DWORD(__stdcall* _XInputGetBatteryInformation)(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation);
DWORD WINAPI XInputGetBatteryInformation(DWORD dwUserIndex, BYTE devType, XINPUT_BATTERY_INFORMATION* pBatteryInformation)
{
	_XInputGetBatteryInformation func = NULL;
	if (!func)
		func = (_XInputGetBatteryInformation)GetProcAddress(GetXInputModule(), "XInputGetBatteryInformation");
	return func(dwUserIndex, devType, pBatteryInformation);
}

typedef DWORD(__stdcall* _XInputGetKeystroke)(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke);
DWORD WINAPI XInputGetKeystroke(DWORD dwUserIndex, DWORD dwReserved, PXINPUT_KEYSTROKE pKeystroke)
{
	_XInputGetKeystroke func = NULL;
	if (!func)
		func = (_XInputGetKeystroke)GetProcAddress(GetXInputModule(), "XInputGetKeystroke");
	return func(dwUserIndex, dwReserved, pKeystroke);
}
