#include "OVR_CAPI_Audio.h"

#include <Mmddk.h>
#include <DSound.h>

#include "REV_Assert.h"

// TODO: Return the vive-specific audio devices instead of the default.

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
	GUID guid;
	GetDeviceID(&DSDEVID_DefaultPlayback, &guid);
	StringFromGUID2(guid, deviceOutStrBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceOutGuid(GUID* deviceOutGuid)
{
	GetDeviceID(&DSDEVID_DefaultPlayback, deviceOutGuid);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuidStr(WCHAR deviceInStrBuffer[OVR_AUDIO_MAX_DEVICE_STR_SIZE])
{
	GUID guid;
	GetDeviceID(&DSDEVID_DefaultCapture, &guid);
	StringFromGUID2(guid, deviceInStrBuffer, OVR_AUDIO_MAX_DEVICE_STR_SIZE);
	return ovrSuccess;
}

OVR_PUBLIC_FUNCTION(ovrResult) ovr_GetAudioDeviceInGuid(GUID* deviceInGuid)
{
	GetDeviceID(&DSDEVID_DefaultCapture, deviceInGuid);
	return ovrSuccess;
}
