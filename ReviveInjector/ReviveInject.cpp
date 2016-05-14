#include <windows.h>
#include "ReviveInject.h"
#include <stdio.h>
#include <fstream>
#include <windows.h>
#include <direct.h>
#include <stdlib.h>

bool InjectLibRevive(HANDLE hProcess);
bool InjectOpenVR(HANDLE hProcess);
bool InjectDLL(HANDLE hProcess, const char *dllPath, int dllPathLength);

int CreateProcessAndInject(wchar_t *programPath) {
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	if (!CreateProcess(NULL, programPath, &sa, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
	{
		printf("Failed to create process\n");
		return -1;
	}

#if _WIN64
	BOOL is32Bit = FALSE;
	if (!IsWow64Process(pi.hProcess, &is32Bit))
	{
		printf("Failed to query bit depth\n");
		return -1;
	}
	if (is32Bit)
	{
		printf("32-bit process detected, delegating to ReviveInjector_x86\n");

		PROCESS_INFORMATION injector;
		ZeroMemory(&injector, sizeof(injector));
		wchar_t commandLine[MAX_PATH];
		swprintf(commandLine, sizeof(commandLine), L"ReviveInjector_x86.exe /handle %d", pi.hProcess);
		if (!CreateProcess(NULL, commandLine, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &injector))
		{
			printf("Failed to create ReviveInjector_x86\n");
			return -1;
		}

		DWORD waitReturnValue = WaitForSingleObject(injector.hThread, INFINITE);
		if (waitReturnValue != WAIT_OBJECT_0) {
			printf("Failed to wait for ReviveInjector_x86 to exit\n");
			return false;
		}

		DWORD injectorReturnValue;
		GetExitCodeThread(injector.hThread, &injectorReturnValue);
		ResumeThread(pi.hThread);
		return injectorReturnValue;
	}
#endif

	if (!InjectOpenVR(pi.hProcess) ||
		!InjectLibRevive(pi.hProcess)) {
		ResumeThread(pi.hThread);
		return -1;
	}

	printf("Injected dlls succesfully\n");
	ResumeThread(pi.hThread);
	return 0;
}

int OpenProcessAndInject(wchar_t *processId) {
	HANDLE hProcess = (HANDLE)wcstol(processId, nullptr, 0);
	if (hProcess == NULL)
	{
		printf("Failed to get process handle\n");
		return -1;
	}

	if (!InjectOpenVR(hProcess) ||
		!InjectLibRevive(hProcess)) {
		return -1;
	}

	printf("Injected dlls succesfully\n");
	return 0;
}

bool InjectDLL(HANDLE hProcess, const char *dllName) {
	char dllPath[MAX_PATH];
	char *cwd;
	if ((cwd = _getcwd(NULL, 0)) == NULL) {
		printf("Failed to get cwd\n");
		return false;
	}
#if _WIN64
	snprintf(dllPath, sizeof(dllPath), "%s\\x64\\%s", cwd, dllName);
#else
	snprintf(dllPath, sizeof(dllPath), "%s\\x86\\%s", cwd, dllName);
#endif
	int dllPathLength = sizeof(dllPath);
	return InjectDLL(hProcess, dllPath, dllPathLength);
}

bool InjectLibRevive(HANDLE hProcess) {
#if _WIN64
	return InjectDLL(hProcess, "LibRevive64_1.dll");
#else
	return InjectDLL(hProcess, "LibRevive32_1.dll");
#endif
}

bool InjectOpenVR(HANDLE hProcess) {
	return InjectDLL(hProcess, "openvr_api.dll");
}

bool InjectDLL(HANDLE hProcess, const char *dllPath, int dllPathLength)
{
	HMODULE hModule = GetModuleHandle(L"kernel32.dll");
	LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hModule, "LoadLibraryA");
	if (loadLibraryAddr == NULL)
	{
		printf("Unable to locate LoadLibraryA\n");
		return false;
	}
	LPVOID loadLibraryArg = (LPVOID)VirtualAllocEx(hProcess, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (loadLibraryArg == NULL)
	{
		printf("Could not allocate memory in process\n");
		return false;
	}

	int n = WriteProcessMemory(hProcess, loadLibraryArg, dllPath, dllPathLength, NULL);
	if (n == 0)
	{
		printf("Could not write to process's address space\n");
		return false;
	}

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, loadLibraryArg, NULL, NULL);
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
