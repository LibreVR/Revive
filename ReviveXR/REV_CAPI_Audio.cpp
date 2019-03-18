#include "OVR_CAPI_Audio.h"
#include "Common.h"

#include <Windows.h>
#include <Mmddk.h>
#include <Mmdeviceapi.h>
#include <DSound.h>

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutWaveId(UINT* deviceOutId)
{
	REV_TRACE(ovr_GetAudioDeviceOutWaveId);

	if (!deviceOutId)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static UINT cachedId = 0;
	if (!cachedId)
	{
#pragma warning( disable : 4312 )
		if (waveOutMessage((HWAVEOUT)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)&cachedId, NULL) != 0)
			return ovrError_AudioDeviceNotFound;
#pragma warning( default : 4312 )
	}

	*deviceOutId = cachedId;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInWaveId(UINT* deviceInId)
{
	REV_TRACE(ovr_GetAudioDeviceInWaveId);

	if (!deviceInId)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static UINT cachedId = 0;
	if (!cachedId)
	{
#pragma warning( disable : 4312 )
		if (waveInMessage((HWAVEIN)WAVE_MAPPER, DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)&cachedId, NULL) != 0)
			return ovrError_AudioDeviceNotFound;
#pragma warning( default : 4312 )
	}

	*deviceInId = cachedId;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuidStr(WCHAR deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceOutGuidStr);

	if (!deviceOutStrBuffer)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static WCHAR cachedBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
	if (wcslen(cachedBuffer) == 0)
	{
		HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

		IMMDeviceEnumerator* pEnumerator;
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
		HRESULT hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);
		if (FAILED(hr))
			return ovrError_AudioComError;

		IMMDevice* pDevice;
		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		if (FAILED(hr))
			return ovrError_AudioDeviceNotFound;

		LPWSTR pGuid;
		hr = pDevice->GetId(&pGuid);
		if (FAILED(hr))
			return ovrError_AudioComError;

		wcsncpy(cachedBuffer, pGuid, OVR_AUDIO_MAX_DEVICE_STR_SIZE);

		// Cleanup
		pDevice->Release();
		pEnumerator->Release();
		if (SUCCEEDED(com))
			CoUninitialize();
	}

	wcsncpy(deviceOutStrBuffer, cachedBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuid(GUID* deviceOutGuid)
{
	REV_TRACE(ovr_GetAudioDeviceOutGuid);

	if (!deviceOutGuid)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static GUID cachedGuid = GUID_NULL;
	if (cachedGuid == GUID_NULL)
	{
		HRESULT hr = GetDeviceID(&DSDEVID_DefaultPlayback, &cachedGuid);
		if (FAILED(hr))
			return ovrError_AudioDeviceNotFound;
	}

	*deviceOutGuid = cachedGuid;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuidStr(WCHAR deviceInStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceInGuidStr);

	if (!deviceInStrBuffer)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static WCHAR cachedBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
	if (wcslen(cachedBuffer) == 0)
	{
		HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

		IMMDeviceEnumerator* pEnumerator;
		const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
		const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
		HRESULT hr = CoCreateInstance(
			CLSID_MMDeviceEnumerator, NULL,
			CLSCTX_ALL, IID_IMMDeviceEnumerator,
			(void**)&pEnumerator);
		if (FAILED(hr))
			return ovrError_AudioComError;

		IMMDevice* pDevice;
		hr = pEnumerator->GetDefaultAudioEndpoint(eCapture, eConsole, &pDevice);
		if (FAILED(hr))
			return ovrError_AudioDeviceNotFound;

		LPWSTR pGuid;
		hr = pDevice->GetId(&pGuid);
		if (FAILED(hr))
			return ovrError_AudioComError;

		wcsncpy(cachedBuffer, pGuid, OVR_AUDIO_MAX_DEVICE_STR_SIZE);

		// Cleanup
		pDevice->Release();
		pEnumerator->Release();
		if (SUCCEEDED(com))
			CoUninitialize();
	}

	wcsncpy(deviceInStrBuffer, cachedBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuid(GUID* deviceInGuid)
{
	REV_TRACE(ovr_GetAudioDeviceInGuid);

	if (!deviceInGuid)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static GUID cachedGuid = GUID_NULL;
	if (cachedGuid == GUID_NULL)
	{
		HRESULT hr = GetDeviceID(&DSDEVID_DefaultCapture, &cachedGuid);
		if (FAILED(hr))
			return ovrError_AudioDeviceNotFound;
	}

	*deviceInGuid = cachedGuid;
	return ovrSuccess;
}
