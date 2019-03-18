#include <openvr.h>
#include <Windows.h>

typedef uint32_t(VR_CALLTYPE* VR_InitInternal2Ptr)(vr::EVRInitError *peError, vr::EVRApplicationType eApplicationType, const char *pStartupInfo);
typedef void (VR_CALLTYPE* VR_ShutdownInternalPtr)(void);

bool IsSteamVRRunning()
{
	HMODULE openvr = LoadLibrary(L"openvr_api.dll");
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

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
		if (IsSteamVRRunning())
		{
#if _WIN64
			LoadLibrary(L"LibRevive64_1.dll");
#else
			LoadLibrary(L"LibRevive32_1.dll");
#endif
		}
		else
		{
#if _WIN64
			LoadLibrary(L"LibRXRRT64_1.dll");
#else
			LoadLibrary(L"LibRXRRT32_1.dll");
#endif
		}
		break;
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}
