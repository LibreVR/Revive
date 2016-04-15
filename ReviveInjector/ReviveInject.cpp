#include <windows.h>
#include "ReviveInject.h"
#include <stdio.h>
#include <fstream>
#include <windows.h>
#include <direct.h>
#include <stdlib.h>

bool InjectLibRevive(HANDLE processHandle, HANDLE threadHandle);
bool InjectOpenVR(HANDLE processHandle, HANDLE threadHandle);
bool InjectDLL(HANDLE processHandle, HANDLE threadHandle, const char *dllPath, int dllPathLength);
bool InjectUnityOvrPlugin(HANDLE processHandle, HANDLE threadHandle);

const LPWSTR GetLPWSTR(char *c)
{
    const size_t cSize = strlen(c)+1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);
    return wc;
}

int CreateProcessAndInject(char *programPath) {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
	if (!CreateProcess(NULL, GetLPWSTR(programPath), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
	{
		printf("Failed to create process\n");
		return -1;
	}	
	if (!InjectOpenVR(pi.hProcess, pi.hThread) ||
		!InjectLibRevive(pi.hProcess, pi.hThread) ||
		!InjectUnityOvrPlugin(pi.hProcess, pi.hThread)) {
		ResumeThread(pi.hThread);
		return -1;
	}
	printf("Injected dlls succesfully\n");
	ResumeThread(pi.hThread);
	return 0;
}

bool InjectDLL(HANDLE processHandle, HANDLE threadHandle, const char *dllName32, const char *dllName64) {
	char *cwd;
	if ((cwd = _getcwd(NULL, 0)) == NULL) {
		printf("Failed to get cwd\n");
		return -1;
	}
	BOOL is32Bit = TRUE;
#if _WIN64
	if (!IsWow64Process(processHandle, &is32Bit))
	{
		printf("Failed to query bit depth\n");
		return -1;
	}
#endif
	char *platform = is32Bit ? "x86" : "x64";
	const char *dllName = is32Bit ? dllName32 : dllName64;
	char dllPath[MAX_PATH];
	snprintf(dllPath, sizeof(dllPath), "%s\\Revive\\%s\\%s", cwd, platform, dllName);
	int dllPathLength = sizeof(dllPath);
	return InjectDLL(processHandle, threadHandle, dllPath, dllPathLength);
}

bool InjectUnityOvrPlugin(HANDLE processHandle, HANDLE threadHandle) {
	char *dllName = "OVRPlugin.dll";
	return InjectDLL(processHandle, threadHandle, dllName, dllName);
}

bool InjectLibRevive(HANDLE processHandle, HANDLE threadHandle) {
	return InjectDLL(processHandle, threadHandle, "LibRevive32_1.dll", "LibRevive64_1.dll");
}

bool InjectOpenVR(HANDLE processHandle, HANDLE threadHandle) {
	const char *dllName = "openvr_api.dll";
	return InjectDLL(processHandle, threadHandle, dllName, dllName);
}

bool InjectDLL(HANDLE processHandle, HANDLE threadHandle, const char *dllPath, int dllPathLength)
{
	HMODULE hModule = GetModuleHandle(L"kernel32.dll");
	LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hModule, "LoadLibraryA");
	if (loadLibraryAddr == NULL)
	{
		printf("Unable to locate LoadLibraryA\n");
		return false;
	}
	LPVOID loadLibraryArg = (LPVOID)VirtualAllocEx(processHandle, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (loadLibraryArg == NULL)
	{
		printf("Could not allocate memory in process\n");
		return false;
	}

	int n = WriteProcessMemory(processHandle, loadLibraryArg, dllPath, dllPathLength, NULL);
	if (n == 0)
	{
		printf("Could not write to process's address space\n");
		return false;
	}

	HANDLE hThread = CreateRemoteThread(processHandle, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, loadLibraryArg, NULL, NULL);
	if (hThread == INVALID_HANDLE_VALUE)
	{
		printf("Failed to create remote thread in process\n");
		return false;
	}

	DWORD waitReturnValue = WaitForSingleObject(hThread, INFINITE);
	if (waitReturnValue != WAIT_OBJECT_0) {
		printf("Failed to wait for LoadLibrary to exit\n");
		return false;
	}
	DWORD loadLibraryReturnValue;
	GetExitCodeThread(hThread, &loadLibraryReturnValue);
	if (loadLibraryReturnValue == NULL) {
		printf("LoadLibrary failed to return module handle\n");
		return false;
	}
	return true;
}
