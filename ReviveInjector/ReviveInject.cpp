#include "ReviveInject.h"
#include <windows.h>
#include <direct.h>
#include <stdlib.h>
#include <Shlwapi.h>

bool InjectLibRevive(HANDLE hProcess);
bool InjectOpenVR(HANDLE hProcess);
bool InjectDLL(HANDLE hProcess, const char *dllPath, int dllPathLength);

int CreateProcessAndInject(wchar_t *programPath) {
	LOG("Creating process: %ls\n", programPath);

	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	SECURITY_ATTRIBUTES sa;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&sa, sizeof(sa));
	sa.nLength = sizeof(sa);
	sa.bInheritHandle = TRUE;

	wchar_t workingDir[MAX_PATH];
	wcsncpy(workingDir, programPath, MAX_PATH);
	PathRemoveFileSpec(workingDir);
	if (!CreateProcess(NULL, programPath, &sa, NULL, FALSE, CREATE_SUSPENDED, NULL, workingDir, &si, &pi))
	{
		LOG("Failed to create process\n");
		return -1;
	}

#if _WIN64
	BOOL is32Bit = FALSE;
	if (!IsWow64Process(pi.hProcess, &is32Bit))
	{
		LOG("Failed to query bit depth\n");
		return -1;
	}
	if (is32Bit)
	{
		LOG("Delegating 32-bit process (%d) to ReviveInjector_x86\n", pi.dwProcessId);

		PROCESS_INFORMATION injector;
		ZeroMemory(&injector, sizeof(injector));
		wchar_t commandLine[MAX_PATH];
		swprintf(commandLine, sizeof(commandLine), L"ReviveInjector_x86.exe /handle %d", pi.hProcess);
		if (!CreateProcess(NULL, commandLine, NULL, NULL, TRUE, NULL, NULL, NULL, &si, &injector))
		{
			LOG("Failed to create ReviveInjector_x86\n");
			return -1;
		}

		DWORD waitReturnValue = WaitForSingleObject(injector.hThread, INFINITE);
		if (waitReturnValue != WAIT_OBJECT_0) {
			LOG("Failed to wait for ReviveInjector_x86 to exit\n");
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

	LOG("Injected dlls succesfully\n");
	ResumeThread(pi.hThread);
	return 0;
}

int OpenProcessAndInject(wchar_t *processId) {
	LOG("Injecting process handle: %ls\n", processId);

	HANDLE hProcess = (HANDLE)wcstol(processId, nullptr, 0);
	if (hProcess == NULL)
	{
		LOG("Failed to get process handle\n");
		return -1;
	}

	if (!InjectOpenVR(hProcess) ||
		!InjectLibRevive(hProcess)) {
		return -1;
	}

	LOG("Injected dlls succesfully\n");
	return 0;
}

bool InjectDLL(HANDLE hProcess, const char *dllName) {
	char dllPath[MAX_PATH];
	char cwd[MAX_PATH];
	GetModuleFileNameA(NULL, cwd, MAX_PATH);
	PathRemoveFileSpecA(cwd);
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
	LOG("Injecting DLL: %s\n", dllPath);

	HMODULE hModule = GetModuleHandle(L"kernel32.dll");
	LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(hModule, "LoadLibraryA");
	if (loadLibraryAddr == NULL)
	{
		LOG("Unable to locate LoadLibraryA\n");
		return false;
	}
	LOG("LoadLibrary found at address: 0x%x\n", loadLibraryAddr);

	LPVOID loadLibraryArg = (LPVOID)VirtualAllocEx(hProcess, NULL, dllPathLength, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	if (loadLibraryArg == NULL)
	{
		LOG("Could not allocate memory in process\n");
		return false;
	}

	int n = WriteProcessMemory(hProcess, loadLibraryArg, dllPath, dllPathLength, NULL);
	if (n == 0)
	{
		LOG("Could not write to process's address space\n");
		return false;
	}

	HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, loadLibraryArg, NULL, NULL);
	if (hThread == INVALID_HANDLE_VALUE)
	{
		LOG("Failed to create remote thread in process\n");
		return false;
	}

	DWORD waitReturnValue = WaitForSingleObject(hThread, INFINITE);
	if (waitReturnValue != WAIT_OBJECT_0) {
		LOG("Failed to wait for LoadLibrary to exit\n");
		return false;
	}
	DWORD loadLibraryReturnValue;
	GetExitCodeThread(hThread, &loadLibraryReturnValue);
	if (loadLibraryReturnValue == NULL) {
		LOG("LoadLibrary failed to return module handle\n");
		return false;
	}
	return true;
}
