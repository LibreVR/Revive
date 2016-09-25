#include "OVR_CAPI_Audio.h"

#include <Windows.h>
#include <Mmddk.h>
#include <Mmdeviceapi.h>
#include <DSound.h>

#include "Assert.h"

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutWaveId(UINT* deviceOutId)
{
	REV_TRACE(ovr_GetAudioDeviceOutWaveId);

	if (!deviceOutId)
		return ovrError_InvalidParameter;

#pragma warning( disable : 4312 )
	if (waveOutMessage((HWAVEOUT)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)deviceOutId, NULL) != 0)
		return ovrError_RuntimeException;
#pragma warning( default : 4312 )

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInWaveId(UINT* deviceInId)
{
	REV_TRACE(ovr_GetAudioDeviceInWaveId);

	if (!deviceInId)
		return ovrError_InvalidParameter;

#pragma warning( disable : 4312 )
	if (waveInMessage((HWAVEIN)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)deviceInId, NULL) != 0)
		return ovrError_RuntimeException;
#pragma warning( default : 4312 )

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuidStr(WCHAR deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceOutGuidStr);

	if (!deviceOutStrBuffer)
		return ovrError_InvalidParameter;

	HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	IMMDeviceEnumerator* pEnumerator;
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	IMMDevice* pDevice;
	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	LPWSTR pGuid;
	hr = pDevice->GetId(&pGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	wcsncpy(deviceOutStrBuffer, pGuid, OVR_AUDIO_MAX_DEVICE_STR_SIZE);

	// Cleanup
	pDevice->Release();
	pEnumerator->Release();
	if (SUCCEEDED(com))
		CoUninitialize();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuid(GUID* deviceOutGuid)
{
	REV_TRACE(ovr_GetAudioDeviceOutGuid);

	if (!deviceOutGuid)
		return ovrError_InvalidParameter;

	HRESULT hr = GetDeviceID(&DSDEVID_DefaultPlayback, deviceOutGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuidStr(WCHAR deviceInStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceInGuidStr);

	if (!deviceInStrBuffer)
		return ovrError_InvalidParameter;

	HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

	IMMDeviceEnumerator* pEnumerator;
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		(void**)&pEnumerator);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	IMMDevice* pDevice;
	hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	LPWSTR pGuid;
	hr = pDevice->GetId(&pGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	wcsncpy(deviceInStrBuffer, pGuid, OVR_AUDIO_MAX_DEVICE_STR_SIZE);

	// Cleanup
	pDevice->Release();
	pEnumerator->Release();
	if (SUCCEEDED(com))
		CoUninitialize();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuid(GUID* deviceInGuid)
{
	REV_TRACE(ovr_GetAudioDeviceInGuid);

	if (!deviceInGuid)
		return ovrError_InvalidParameter;

	HRESULT hr = GetDeviceID(&DSDEVID_DefaultCapture, deviceInGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	return ovrSuccess;
}
