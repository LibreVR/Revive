#include "OVR_CAPI_Audio.h"

#include <Windows.h>
#include <Mmddk.h>
#include <Mmdeviceapi.h>

#include "REV_Assert.h"

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutWaveId(UINT* deviceOutId)
{
	waveOutMessage((HWAVEOUT)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)deviceOutId, NULL);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInWaveId(UINT* deviceInId)
{
	waveInMessage((HWAVEIN)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)deviceInId, NULL);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuidStr(WCHAR deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
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
	pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	LPWSTR pGuid;
	hr = pDevice->GetId(&pGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	wcsncpy(deviceOutStrBuffer, pGuid, OVR_AUDIO_MAX_DEVICE_STR_SIZE);

	// Cleanup
	pDevice->Release();
	pEnumerator->Release();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuid(GUID* deviceOutGuid)
{
	WCHAR deviceOut[OVR_AUDIO_MAX_DEVICE_STR_SIZE];
	ovr_GetAudioDeviceOutGuidStr(deviceOut);
	UuidFromString((RPC_WSTR)deviceOut, deviceOutGuid);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuidStr(WCHAR deviceInStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
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
	pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
	LPWSTR pGuid;
	hr = pDevice->GetId(&pGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;

	wcsncpy(deviceInStrBuffer, pGuid, OVR_AUDIO_MAX_DEVICE_STR_SIZE);

	// Cleanup
	pDevice->Release();
	pEnumerator->Release();
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuid(GUID* deviceInGuid)
{
	WCHAR deviceIn[OVR_AUDIO_MAX_DEVICE_STR_SIZE];
	ovr_GetAudioDeviceInGuidStr(deviceIn);
	UuidFromString((RPC_WSTR)deviceIn, deviceInGuid);
	return ovrSuccess;
}
