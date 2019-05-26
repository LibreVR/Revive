#include <Windows.h>
#include <stdio.h>
#include <dxgi.h>
#include <d3d11.h>
#include <Shlwapi.h>
#include <string>

#include "MinHook.h"
#include "OVR_CAPI.h"
#include "OVR_Version.h"

// {EBA3BA6A-76A2-4A9B-B150-681FC1020EDE}
static const GUID RXR_RTV_DESC =
{ 0xeba3ba6a, 0x76a2, 0x4a9b,{ 0xb1, 0x50, 0x68, 0x1f, 0xc1, 0x2, 0xe, 0xde } };

typedef FARPROC(WINAPI* _GetProcAddress)(HMODULE hModule, LPCSTR lpProcName);
typedef HMODULE(WINAPI* _LoadLibrary)(LPCWSTR lpFileName);
typedef HANDLE(WINAPI* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef HRESULT(WINAPI* _CreateRenderTargetView)(
	ID3D11Device						*pDevice,
	ID3D11Resource                      *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView              **ppSRView
	);

_GetProcAddress TrueGetProcAddress;
_LoadLibrary TrueLoadLibrary;
_OpenEvent TrueOpenEvent;
_CreateRenderTargetView TrueCreateRenderTargetView;
PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN TrueCreateDevice;

HMODULE revModule;
WCHAR revModuleName[MAX_PATH];
WCHAR ovrModuleName[MAX_PATH];

LPVOID TargetCreateDevice;

HRESULT WINAPI HookCreateRenderTargetView(
	ID3D11Device						*pDevice,
	ID3D11Resource                      *pResource,
	const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	ID3D11RenderTargetView              **ppSRView
)
{
	D3D11_RENDER_TARGET_VIEW_DESC desc;
	UINT size = (UINT)sizeof(desc);
	if (!pDesc && SUCCEEDED(pResource->GetPrivateData(RXR_RTV_DESC, &size, &desc)))
	{
		return TrueCreateRenderTargetView(pDevice, pResource, &desc, ppSRView);
	}

	return TrueCreateRenderTargetView(pDevice, pResource, pDesc, ppSRView);
}

HRESULT HookCreateDevice(IDXGIAdapter *pAdapter, D3D_DRIVER_TYPE DriverType, HMODULE Software,
	UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels, UINT FeatureLevels, UINT SDKVersion,
	const DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, IDXGISwapChain **ppSwapChain, ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel, ID3D11DeviceContext  **ppImmediateContext)
{
	HRESULT hr = TrueCreateDevice(pAdapter, DriverType, Software, Flags,
		pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, ppDevice, pFeatureLevel, ppImmediateContext);

	static LPVOID target = nullptr;
	if (target)
		MH_RemoveHook(target);

	MH_CreateHookVirtualEx(*ppDevice, 9, HookCreateRenderTargetView, (PVOID*)&TrueCreateRenderTargetView, &target);
	MH_EnableHook(target);

	return hr;
}

FARPROC WINAPI HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
	if (hModule == revModule)
	{
		OutputDebugStringA(lpProcName);
		OutputDebugStringA("\n");
	}

	return TrueGetProcAddress(hModule, lpProcName);
}

HANDLE WINAPI HookOpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	// Don't touch this, it heavily affects performance in Unity games.
	if (wcscmp(lpName, OVR_HMD_CONNECTED_EVENT_NAME) == 0)
		return ::CreateEventW(NULL, TRUE, TRUE, NULL);

	return TrueOpenEvent(dwDesiredAccess, bInheritHandle, lpName);
}

HMODULE WINAPI HookLoadLibrary(LPCWSTR lpFileName)
{
	LPCWSTR name = PathFindFileNameW(lpFileName);
	LPCWSTR ext = PathFindExtensionW(name);
	size_t length = ext - name;

	// Load our own library again so the ref count is incremented.
	if (wcsncmp(name, ovrModuleName, length) == 0)
		return TrueLoadLibrary(revModuleName);

	return TrueLoadLibrary(lpFileName);
}

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
#if defined(_WIN64)
	const char* pBitDepth = "64";
#else
	const char* pBitDepth = "32";
#endif
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			revModule = (HMODULE)hModule;
			GetModuleFileName(revModule, revModuleName, MAX_PATH);
			swprintf(ovrModuleName, MAX_PATH, L"LibOVRRT%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
			MH_Initialize();
#if 0
			MH_CreateHook(GetProcAddress, HookGetProcAddress, (PVOID*)&TrueGetProcAddress);
#endif
			// D3D11CreateDevice is just a wrapper for D3D11CreateDeviceAndSwapChain
			MH_CreateHookApiEx(L"d3d11.dll", "D3D11CreateDeviceAndSwapChain", HookCreateDevice, (PVOID*)&TrueCreateDevice, &TargetCreateDevice);
			MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
			MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
			MH_EnableHook(MH_ALL_HOOKS);
			break;
		case DLL_PROCESS_DETACH:
			MH_Uninitialize();
			break;
		default:
			break;
	}
	return TRUE;
}
