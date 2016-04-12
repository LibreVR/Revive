#include <windows.h>
#include "ReviveInject.h"
#include <stdio.h>
#include <fstream>
#include <windows.h>
#include <direct.h>
#include <stdlib.h>

const LPWSTR GetLPWSTR(char *c)
{
    const size_t cSize = strlen(c)+1;
    wchar_t* wc = new wchar_t[cSize];
    mbstowcs(wc, c, cSize);
    return wc;
}

int CreateProcessAndInject(char *programPath)
{
	char *cwd;
	if ((cwd = _getcwd(NULL, 0)) == NULL) {
		printf("Failed to get cwd\n");
		return -1;
	}

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
	// TODO: close the created process if something goes wrong
	if (!CreateProcess(NULL, GetLPWSTR(programPath), NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi))
	{
		printf("Failed to create process\n");
		return -1;
	}

	const char *dllName = "LibRevive32_1.dll";
	const char *platform = "x86";
#if _WIN64
	BOOL b32bit = FALSE;
	if (!IsWow64Process(pi.hProcess, &b32bit))
	{
		printf("Failed to query bit depth\n");
		return -1;
	}
	if (!b32bit) {
		dllName = "LibRevive64_1.dll";
		platform = "x64";
	}
#endif
	char dllPath[MAX_PATH];
	snprintf(dllPath, sizeof(dllPath), "%s\\Revive\\%s\\%s", cwd, platform, dllName);
	int dllPathLength = sizeof(dllPath);

	HMODULE hModule = GetModuleHandle(L"kernel32.dll");
	LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hModule, "LoadLibraryA");
	if (loadLibraryAddr == NULL)
	{
		printf("Unable to locate LoadLibraryA\n");
		return -1;
	}
	LPVOID loadLibraryArg = (LPVOID)VirtualAllocEx(pi.hProcess, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (loadLibraryArg == NULL)
	{
		printf("Could not allocate memory in process %u\n", pi.dwProcessId);
		return -1;
	}

	int n = WriteProcessMemory(pi.hProcess, loadLibraryArg, dllPath, dllPathLength, NULL);
	if (n == 0)
	{
		printf("Could not write to process's address space\n");
		return -1;
	}

	HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, loadLibraryArg, NULL, NULL);
	if (hThread == INVALID_HANDLE_VALUE)
	{
		printf("Failed to create remote thread in process\n");
		return -1;
	}

	DWORD waitReturnValue = WaitForSingleObject(hThread, INFINITE);
	if (waitReturnValue != WAIT_OBJECT_0) {
		printf("Failed to wait for LoadLibrary to exit\n");
		return -1;
	}
	DWORD loadLibraryReturnValue;
	GetExitCodeThread(hThread, &loadLibraryReturnValue);
	if (loadLibraryReturnValue == NULL) {
		printf("LoadLibrary failed to return module handle\n");
		return -1;
	}

	printf("Injected dll succesfully\n");
	ResumeThread(pi.hThread);
	return 0;
}
