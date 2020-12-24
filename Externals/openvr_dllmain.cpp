#include <Windows.h>
#include <detours.h>

BOOL APIENTRY DllMain(HANDLE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	DetourIsHelperProcess();
	return TRUE;
}
