#include "OVR_CAPI_Audio.h"
#include "Common.h"

#include <openvr.h>
#include <wrl/client.h>
#include <Windows.h>
#include <DSound.h>
#include <initguid.h>
#include <Mmddk.h>
#include <Mmdeviceapi.h>

ovrResult AudioEndPointToGuid(char* deviceStrBuffer, int deviceStrSize, GUID* deviceGuid)
{
	if (!deviceGuid)
		return ovrError_InvalidParameter;

	if (deviceStrSize > OVR_AUDIO_MAX_DEVICE_STR_SIZE)
		return ovrError_InsufficientArraySize;

	WCHAR strEndpointId[OVR_AUDIO_MAX_DEVICE_STR_SIZE];
	if (!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, deviceStrBuffer, deviceStrSize, strEndpointId, OVR_AUDIO_MAX_DEVICE_STR_SIZE))
		return ovrError_MemoryAllocationFailure;

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
	hr = pEnumerator->GetDevice(strEndpointId, &pDevice);
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
	if (!cachedId)
	{
		MMRESULT mmr;
		char endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		vr::ETrackedPropertyError error = vr::TrackedProp_Success;
		uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(
			vr::k_unTrackedDeviceIndex_Hmd,
			vr::Prop_Audio_DefaultPlaybackDeviceId_String,
			endpoint, OVR_AUDIO_MAX_DEVICE_STR_SIZE);
		if (error != vr::TrackedProp_Success)
		{
			// Return the default without caching so we can attempt again later
			mmr = waveOutMessage((HWAVEOUT)UIntToPtr(WAVE_MAPPER), DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)deviceOutId, NULL);
			return mmr == MMSYSERR_NOERROR ? ovrSuccess : ovrError_AudioDeviceNotFound;
		}

		WCHAR strEndpointIdKey[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		if (size > OVR_AUDIO_MAX_DEVICE_STR_SIZE)
			return ovrError_InsufficientArraySize;
		if (!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, endpoint, size, strEndpointIdKey, OVR_AUDIO_MAX_DEVICE_STR_SIZE))
			return ovrError_MemoryAllocationFailure;

		// Get the size of the endpoint ID string.
		size_t cbEndpointIdKey = size * sizeof(WCHAR);

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
		MMRESULT mmr;
		char endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		vr::ETrackedPropertyError error = vr::TrackedProp_Success;
		uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(
			vr::k_unTrackedDeviceIndex_Hmd,
			vr::Prop_Audio_DefaultRecordingDeviceId_String,
			endpoint, OVR_AUDIO_MAX_DEVICE_STR_SIZE, &error);
		if (error != vr::TrackedProp_Success)
		{
			// Return the default without caching so we can attempt again later
			mmr = waveInMessage((HWAVEIN)UIntToPtr(WAVE_MAPPER), DRVM_MAPPER_PREFERRED_GET, (DWORD_PTR)deviceInId, NULL);
			return mmr == MMSYSERR_NOERROR ? ovrSuccess : ovrError_AudioDeviceNotFound;
		}

		WCHAR strEndpointIdKey[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		if (size > OVR_AUDIO_MAX_DEVICE_STR_SIZE)
			return ovrError_InsufficientArraySize;
		if (!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, endpoint, size, strEndpointIdKey, OVR_AUDIO_MAX_DEVICE_STR_SIZE))
			return ovrError_MemoryAllocationFailure;

		// Get the size of the endpoint ID string.
		size_t cbEndpointIdKey = size * sizeof(WCHAR);

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
	*deviceInId = cachedId;
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuidStr(WCHAR deviceOutStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	REV_TRACE(ovr_GetAudioDeviceOutGuidStr);

	if (!deviceOutStrBuffer)
		return ovrError_InvalidParameter;

	char endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
	vr::ETrackedPropertyError error = vr::TrackedProp_Success;
	uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(
		vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_Audio_DefaultPlaybackDeviceId_String,
		endpoint, OVR_AUDIO_MAX_DEVICE_STR_SIZE);
	if (error == vr::TrackedProp_Success)
	{
		if (size > OVR_AUDIO_MAX_DEVICE_STR_SIZE)
			return ovrError_InsufficientArraySize;
		if (!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, endpoint, size, deviceOutStrBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE))
			return ovrError_MemoryAllocationFailure;
		return ovrSuccess;
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
		char endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		vr::ETrackedPropertyError error = vr::TrackedProp_Success;
		uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(
			vr::k_unTrackedDeviceIndex_Hmd,
			vr::Prop_Audio_DefaultPlaybackDeviceId_String,
			endpoint, OVR_AUDIO_MAX_DEVICE_STR_SIZE, &error);
		if (error == vr::TrackedProp_Success)
		{
			HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
			ovrResult result = AudioEndPointToGuid(endpoint, size, &cachedGuid);
			if (SUCCEEDED(com))
				CoUninitialize();
			if (OVR_FAILURE(result))
				return result;
		}
		else
		{
			// Return the default without caching so we can attempt again later
			HRESULT hr = GetDeviceID(&DSDEVID_DefaultPlayback, deviceOutGuid);
			return SUCCEEDED(hr) ? ovrSuccess : ovrError_AudioInputDeviceNotFound;
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

	char endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
	vr::ETrackedPropertyError error = vr::TrackedProp_Success;
	uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(
		vr::k_unTrackedDeviceIndex_Hmd,
		vr::Prop_Audio_DefaultRecordingDeviceId_String,
		endpoint, OVR_AUDIO_MAX_DEVICE_STR_SIZE, &error);
	if (error == vr::TrackedProp_Success)
	{
		if (size > OVR_AUDIO_MAX_DEVICE_STR_SIZE)
			return ovrError_InsufficientArraySize;
		if (!MultiByteToWideChar(CP_UTF8, MB_PRECOMPOSED, endpoint, size, deviceInStrBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE))
			return ovrError_MemoryAllocationFailure;
		return ovrSuccess;
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
		char endpoint[OVR_AUDIO_MAX_DEVICE_STR_SIZE] = {};
		vr::ETrackedPropertyError error = vr::TrackedProp_Success;
		uint32_t size = vr::VRSystem()->GetStringTrackedDeviceProperty(
			vr::k_unTrackedDeviceIndex_Hmd,
			vr::Prop_Audio_DefaultRecordingDeviceId_String,
			endpoint, OVR_AUDIO_MAX_DEVICE_STR_SIZE, &error);
		if (error == vr::TrackedProp_Success)
		{
			HRESULT com = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
			ovrResult result = AudioEndPointToGuid(endpoint, size, &cachedGuid);
			if (SUCCEEDED(com))
				CoUninitialize();
			if (OVR_FAILURE(result))
				return result;
		}
		else
		{
			// Return the default without caching so we can attempt again later
			HRESULT hr = GetDeviceID(&DSDEVID_DefaultCapture, deviceInGuid);
			return SUCCEEDED(hr) ? ovrSuccess : ovrError_AudioInputDeviceNotFound;
		}
	}
	*deviceInGuid = cachedGuid;
	return ovrSuccess;
}
