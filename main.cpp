#include <Windows.h>
#include <stdio.h>
#include "MinHook.h"
#include "Shlwapi.h"

#include <openvr.h>

#include "Extras\OVR_CAPI_Util.h"
#include "OVR_Version.h"
#include "OVR_CAPI.h"
#include "OVR_CAPI_GL.h"
#include "OVR_CAPI_D3D.h"
#include "OVR_CAPI_Audio.h"

/// This is the Windows Named Event name that is used to check for HMD connected state.
#define REV_HMD_CONNECTED_EVENT_NAME L"ReviveHMDConnected"

typedef HMODULE(__stdcall* _LoadLibrary)(LPCWSTR lpFileName);
typedef HANDLE(__stdcall* _OpenEvent)(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName);
typedef FARPROC(__stdcall* _GetProcAddress)(HMODULE hModule, LPCSTR  lpProcName);
typedef BOOL(__stdcall* _CreateProcess)(LPCTSTR lpApplicationName, LPTSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCTSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation);

_LoadLibrary TrueLoadLibrary;
_OpenEvent TrueOpenEvent;
_GetProcAddress TrueGetProcAddress;
_CreateProcess TrueCreateProcess;

HMODULE ReviveModule;
WCHAR ovrModuleName[MAX_PATH];

bool InjectDLL(HANDLE hProcess, const char *dllPath, int dllPathLength)
{
	HMODULE hModule = GetModuleHandle(L"kernel32.dll");
	LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hModule, "LoadLibraryA");
	if (loadLibraryAddr == NULL)
	{
		return false;
	}
	LPVOID loadLibraryArg = (LPVOID)VirtualAllocEx(hProcess, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (loadLibraryArg == NULL)
	{
		return false;
	}

	int n = WriteProcessMemory(hProcess, loadLibraryArg, dllPath, dllPathLength, NULL);
	if (n == 0)
	{
		return false;
	}

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, loadLibraryArg, NULL, NULL);
	if (hThread == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD waitReturnValue = WaitForSingleObject(hThread, INFINITE);
	if (waitReturnValue != WAIT_OBJECT_0)
	{
		return false;
	}

	DWORD loadLibraryReturnValue;
	GetExitCodeThread(hThread, &loadLibraryReturnValue);
	if (loadLibraryReturnValue == NULL)
	{
		return false;
	}

	return true;
}

FARPROC HookGetProcAddress(HMODULE hModule, LPCSTR lpProcName) 
{
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (wcscmp(moduleName, ovrModuleName) == 0) {
		FARPROC reviveFuncAddress = TrueGetProcAddress(ReviveModule, lpProcName);
		if (reviveFuncAddress) {
			return reviveFuncAddress;
		}
	}

	return TrueGetProcAddress(hModule, lpProcName);
}

HANDLE WINAPI HookOpenEvent(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCWSTR lpName)
{
	if (wcscmp(lpName, OVR_HMD_CONNECTED_EVENT_NAME) == 0)
		return ::CreateEventW(NULL, TRUE, TRUE, REV_HMD_CONNECTED_EVENT_NAME);

	return TrueOpenEvent(dwDesiredAccess, bInheritHandle, lpName);
}

HMODULE WINAPI HookLoadLibrary(LPCWSTR lpFileName)
{
	HMODULE hModule = TrueLoadLibrary(lpFileName);

	// Only enable the GetProcAddress hook when the Oculus Runtime was loaded.
	WCHAR modulePath[MAX_PATH];
	GetModuleFileName(hModule, modulePath, sizeof(modulePath));
	LPCWSTR moduleName = PathFindFileNameW(modulePath);
	if (wcscmp(moduleName, ovrModuleName) == 0)
		MH_EnableHook(GetProcAddress);

	return hModule;
}

BOOL HookCreateProcess(LPCTSTR lpApplicationName, LPTSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes,
	BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCTSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation)
{
	if (TrueCreateProcess(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes,
		bInheritHandles, dwCreationFlags | CREATE_SUSPENDED, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation))
	{
		CHAR modulePath[MAX_PATH];
		GetModuleFileNameA(ReviveModule, modulePath, MAX_PATH);

		CHAR openvrPath[MAX_PATH];
		strncpy(openvrPath, modulePath, MAX_PATH);
		CHAR* fileName = PathFindFileNameA(openvrPath);
		size_t offset = fileName - openvrPath;
		strncpy(fileName, "openvr_api.dll", MAX_PATH - offset);

		if (InjectDLL(lpProcessInformation->hProcess, openvrPath, MAX_PATH))
			InjectDLL(lpProcessInformation->hProcess, modulePath, MAX_PATH);

		if (!(dwCreationFlags & CREATE_SUSPENDED))
			ResumeThread(lpProcessInformation->hThread);

		return TRUE;
	}
	return FALSE;
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
			ReviveModule = (HMODULE)hModule;
			swprintf(ovrModuleName, MAX_PATH, L"LibOVRRT%hs_%d.dll", pBitDepth, OVR_MAJOR_VERSION);
			MH_Initialize();
			MH_CreateHook(LoadLibraryW, HookLoadLibrary, (PVOID*)&TrueLoadLibrary);
			MH_CreateHook(OpenEventW, HookOpenEvent, (PVOID*)&TrueOpenEvent);
			MH_CreateHook(CreateProcessW, HookCreateProcess, (PVOID*)&TrueCreateProcess);
			MH_CreateHook(GetProcAddress, HookGetProcAddress, (PVOID*)&TrueGetProcAddress);
			MH_EnableHook(LoadLibraryW);
			MH_EnableHook(OpenEventW);
			MH_EnableHook(CreateProcessW);
			break;
		case DLL_PROCESS_DETACH:
			MH_RemoveHook(LoadLibraryW);
			MH_RemoveHook(OpenEventW);
			MH_RemoveHook(CreateProcessW);
			MH_RemoveHook(GetProcAddress);
			MH_Uninitialize();
			break;
		default:
			break;
	}
	return TRUE;
}
