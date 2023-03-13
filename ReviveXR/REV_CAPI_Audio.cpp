#include "OVR_CAPI_Audio.h"
#include "Common.h"
#include "Runtime.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <wrl/client.h>
#include <Windows.h>
#include <DSound.h>
#include <initguid.h>
#include <Mmddk.h>
#include <Mmdeviceapi.h>

extern XrInstance g_Instance;

static_assert(XR_MAX_AUDIO_DEVICE_STR_SIZE_OCULUS == OVR_AUDIO_MAX_DEVICE_STR_SIZE,
	"OpenXR maximum audio device string size should match OVR");

ovrResult AudioEndPointToGuid(WCHAR* deviceStrBuffer, GUID* deviceGuid)
{
	if (!deviceGuid)
		return ovrError_InvalidParameter;

	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> pEnumerator;
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, nullptr,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		&pEnumerator);
	if (FAILED(hr))
		return ovrError_AudioComError;

	Microsoft::WRL::ComPtr<IMMDevice> pDevice;
	hr = pEnumerator->GetDevice(deviceStrBuffer, &pDevice);
	if (FAILED(hr))
	{
		return ovrError_AudioOutputDeviceNotFound;
	}

	Microsoft::WRL::ComPtr<IPropertyStore> pPropertyStore;
	hr = pDevice->OpenPropertyStore(STGM_READ, &pPropertyStore);
	if (FAILED(hr))
	{
		return ovrError_AccessDenied;
	}

	PROPVARIANT pv;
	hr = pPropertyStore->GetValue(PKEY_AudioEndpoint_GUID, &pv);
	if (FAILED(hr))
	{
		return ovrError_AccessDenied;
	}

	hr = IIDFromString(pv.pwszVal, deviceGuid);
	if (FAILED(hr))
		return ovrError_RuntimeException;
	return ovrSuccess;
}

ovrResult GetDefaultAudioEndpoint(EDataFlow endpoint, WCHAR deviceStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	if (!deviceStrBuffer)
		return ovrError_InvalidParameter;

	Microsoft::WRL::ComPtr<IMMDeviceEnumerator> pEnumerator;
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	HRESULT hr = CoCreateInstance(
		CLSID_MMDeviceEnumerator, NULL,
		CLSCTX_ALL, IID_IMMDeviceEnumerator,
		&pEnumerator);
	if (FAILED(hr))
		return ovrError_AudioComError;

	Microsoft::WRL::ComPtr<IMMDevice> pDevice;
	hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
	if (FAILED(hr))
		return ovrError_AudioDeviceNotFound;

	LPWSTR pGuid;
	hr = pDevice->GetId(&pGuid);
	if (FAILED(hr))
		return ovrError_AudioComError;
	wcscpy_s(deviceStrBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE, pGuid);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutWaveId(UINT* deviceOutId)
{
	REV_TRACE(ovr_GetAudioDeviceOutWaveId);

	if (!deviceOutId)
		return ovrError_InvalidParameter;

	// Query and cache the result
	static UINT cachedId = 0;
	if (!cachedId && Runtime::Get().AudioDevice)
	{
		XR_FUNCTION(g_Instance, GetAudioOutputDeviceGuidOculus);

		MMRESULT mmr;
		WCHAR strEndpointIdKey[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		XrResult result = GetAudioOutputDeviceGuidOculus(g_Instance, strEndpointIdKey);
		if (XR_FAILED(result))
			return ovrError_AudioOutputDeviceNotFound;

		// Get the size of the endpoint ID string.
		size_t cbEndpointIdKey = wcsnlen_s(strEndpointIdKey, OVR_AUDIO_MAX_DEVICE_STR_SIZE) * sizeof(WCHAR);

		// Include terminating null in string size.
		cbEndpointIdKey += sizeof(WCHAR);

		// Each for-loop iteration below compares the endpoint ID
		// string of the audio endpoint device to the endpoint ID
		// string of an enumerated waveOut device. If the strings
		// match, then we've found the waveOut device that is
		// assigned to the specified device role.
		UINT waveOutId;
		UINT cWaveOutDevices = waveOutGetNumDevs();
		WCHAR strEndpointId[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		for (waveOutId = 0; waveOutId < cWaveOutDevices; waveOutId++)
		{
			size_t cbEndpointId = 0;

			// Get the size (including the terminating null) of
			// the endpoint ID string of the waveOut device.
			mmr = waveOutMessage((HWAVEOUT)UIntToPtr(waveOutId),
				DRV_QUERYFUNCTIONINSTANCEIDSIZE,
				(DWORD_PTR)&cbEndpointId, NULL);
			if (mmr != MMSYSERR_NOERROR ||
				cbEndpointIdKey != cbEndpointId)  // do sizes match?
			{
				continue;  // not a matching device
			}

			// Get the endpoint ID string for this waveOut device.
			mmr = waveOutMessage((HWAVEOUT)UIntToPtr(waveOutId),
				DRV_QUERYFUNCTIONINSTANCEID,
				(DWORD_PTR)strEndpointId,
				cbEndpointId);
			if (mmr != MMSYSERR_NOERROR)
			{
				continue;
			}

			// Check whether the endpoint ID string of this waveOut
			// device matches that of the audio endpoint device.
			if (lstrcmpi(strEndpointId, strEndpointIdKey) == 0)
			{
				cachedId = waveOutId;  // found match
				break;
			}
		}

		if (waveOutId == cWaveOutDevices)
		{
			// We reached the end of the for-loop above without
			// finding a waveOut device with a matching endpoint
			// ID string. This behavior is quite unexpected.
			return ovrError_AudioOutputDeviceNotFound;
		}
	}
	else if (!cachedId)
	{
		MMRESULT mmr = waveOutMessage((HWAVEOUT)UIntToPtr(WAVE_MAPPER), DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)&cachedId, NULL);
		if (mmr != MMSYSERR_NOERROR)
			return ovrError_AudioOutputDeviceNotFound;
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
	if (!cachedId && Runtime::Get().AudioDevice)
	{
		XR_FUNCTION(g_Instance, GetAudioInputDeviceGuidOculus);

		MMRESULT mmr;
		WCHAR strEndpointIdKey[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		XrResult result = GetAudioInputDeviceGuidOculus(g_Instance, strEndpointIdKey);
		if (XR_FAILED(result))
			return ovrError_AudioInputDeviceNotFound;

		// Get the size of the endpoint ID string.
		size_t cbEndpointIdKey = wcsnlen_s(strEndpointIdKey, OVR_AUDIO_MAX_DEVICE_STR_SIZE) * sizeof(WCHAR);

		// Include terminating null in string size.
		cbEndpointIdKey += sizeof(WCHAR);

		// Each for-loop iteration below compares the endpoint ID
		// string of the audio endpoint device to the endpoint ID
		// string of an enumerated waveOut device. If the strings
		// match, then we've found the waveIn device that is
		// assigned to the specified device role.
		UINT waveInId;
		UINT cWaveInDevices = waveInGetNumDevs();
		WCHAR strEndpointId[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		for (waveInId = 0; waveInId < cWaveInDevices; waveInId++)
		{
			size_t cbEndpointId = 0;

			// Get the size (including the terminating null) of
			// the endpoint ID string of the waveOut device.
			mmr = waveInMessage((HWAVEIN)UIntToPtr(waveInId),
				DRV_QUERYFUNCTIONINSTANCEIDSIZE,
				(DWORD_PTR)&cbEndpointId, NULL);
			if (mmr != MMSYSERR_NOERROR ||
				cbEndpointIdKey != cbEndpointId)  // do sizes match?
			{
				continue;  // not a matching device
			}

			// Get the endpoint ID string for this waveOut device.
			mmr = waveInMessage((HWAVEIN)UIntToPtr(waveInId),
				DRV_QUERYFUNCTIONINSTANCEID,
				(DWORD_PTR)strEndpointId,
				cbEndpointId);
			if (mmr != MMSYSERR_NOERROR)
			{
				continue;
			}

			// Check whether the endpoint ID string of this waveOut
			// device matches that of the audio endpoint device.
			if (lstrcmpi(strEndpointId, strEndpointIdKey) == 0)
			{
				cachedId = waveInId;  // found match
				break;
			}
		}

		if (waveInId == cWaveInDevices)
		{
			// We reached the end of the for-loop above without
			// finding a waveOut device with a matching endpoint
			// ID string. This behavior is quite unexpected.
			return ovrError_AudioInputDeviceNotFound;
		}
	}
	else if (!cachedId)
	{
		MMRESULT mmr = waveInMessage((HWAVEIN)UIntToPtr(WAVE_MAPPER), DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)&cachedId, NULL);
		if (mmr != MMSYSERR_NOERROR)
			return ovrError_AudioInputDeviceNotFound;
	}
	*deviceInId = cachedId;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuidStr(WCHAR deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceOutGuidStr);

	if (!deviceOutStrBuffer)
		return ovrError_InvalidParameter;

	if (Runtime::Get().AudioDevice)
	{
		XR_FUNCTION(g_Instance, GetAudioOutputDeviceGuidOculus);
		XrResult result = GetAudioOutputDeviceGuidOculus(g_Instance, deviceOutStrBuffer);
		return XR_SUCCEEDED(result) ? ovrSuccess : ovrError_AudioOutputDeviceNotFound;
	}

	HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ovrResult result = GetDefaultAudioEndpoint(eRender, deviceOutStrBuffer);
	if (SUCCEEDED(com))
		CoUninitialize();
	return result;
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
		if (Runtime::Get().AudioDevice)
		{
			XR_FUNCTION(g_Instance, GetAudioOutputDeviceGuidOculus);

			WCHAR endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE];
			if (XR_FAILED(GetAudioOutputDeviceGuidOculus(g_Instance, endpoint)))
				return ovrError_AudioOutputDeviceNotFound;

			HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
			ovrResult result = AudioEndPointToGuid(endpoint, &cachedGuid);
			if (SUCCEEDED(com))
				CoUninitialize();
			if (OVR_FAILURE(result))
				return result;
		}
		else
		{
			HRESULT hr = GetDeviceID(&DSDEVID_DefaultPlayback, &cachedGuid);
			if (FAILED(hr))
				ovrError_AudioOutputDeviceNotFound;
		}
	}
	*deviceOutGuid = cachedGuid;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuidStr(WCHAR deviceInStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceInGuidStr);

	if (!deviceInStrBuffer)
		return ovrError_InvalidParameter;

	if (Runtime::Get().AudioDevice)
	{
		XR_FUNCTION(g_Instance, GetAudioInputDeviceGuidOculus);
		XrResult result = GetAudioInputDeviceGuidOculus(g_Instance, deviceInStrBuffer);
		return XR_SUCCEEDED(result) ? ovrSuccess : ovrError_AudioOutputDeviceNotFound;
	}

	HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	ovrResult result = GetDefaultAudioEndpoint(eCapture, deviceInStrBuffer);
	if (SUCCEEDED(com))
		CoUninitialize();
	return result;
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
		if (Runtime::Get().AudioDevice)
		{
			XR_FUNCTION(g_Instance, GetAudioInputDeviceGuidOculus);

			WCHAR endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE];
			if (XR_FAILED(GetAudioInputDeviceGuidOculus(g_Instance, endpoint)))
				return ovrError_AudioInputDeviceNotFound;

			HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
			ovrResult result = AudioEndPointToGuid(endpoint, &cachedGuid);
			if (SUCCEEDED(com))
				CoUninitialize();
			if (OVR_FAILURE(result))
				return result;
		}
		else
		{
			HRESULT hr = GetDeviceID(&DSDEVID_DefaultCapture, &cachedGuid);
			if (FAILED(hr))
				return ovrError_AudioInputDeviceNotFound;
		}
	}
	*deviceInGuid = cachedGuid;
	return ovrSuccess;
}
