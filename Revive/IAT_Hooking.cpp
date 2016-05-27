#include <stdio.h>
#include "IAT_Hooking.h"

// Slightly modified from source: https://en.wikipedia.org/wiki/Hooking

// Find the IAT (Import Address Table) entry specific to the given function.
void **IATfind(const char *function, HMODULE module)
{
	PIMAGE_DOS_HEADER pImgDosHeaders = (PIMAGE_DOS_HEADER)module;
	PIMAGE_NT_HEADERS pImgNTHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)pImgDosHeaders + pImgDosHeaders->e_lfanew);
	PIMAGE_IMPORT_DESCRIPTOR pImgImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((LPBYTE)pImgDosHeaders + pImgNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

	if (pImgDosHeaders->e_magic != IMAGE_DOS_SIGNATURE)
	{
		printf("libPE Error : e_magic is no valid DOS signature\n");
		return 0;
	}

	for (PIMAGE_IMPORT_DESCRIPTOR iid = pImgImportDesc; iid->Name != NULL; iid++)
	{
		for (int funcIdx = 0; *(funcIdx + (LPVOID*)(iid->FirstThunk + (SIZE_T)module)) != NULL; funcIdx++)
		{
			char *modFuncName = (char*)(*(funcIdx + (SIZE_T*)(iid->OriginalFirstThunk + (SIZE_T)module)) + (SIZE_T)module + 2);
			const uintptr_t nModFuncName = (uintptr_t)modFuncName;
			int isString = !(nModFuncName & (sizeof(nModFuncName) == 4 ? 0x80000000 : 0x8000000000000000));
			if (isString)
			{
				if (!_stricmp(function, modFuncName))
					return funcIdx + (LPVOID*)(iid->FirstThunk + (SIZE_T)module);
			}
		}
	}
	return 0;
}

void **EATfind(const char *function, HMODULE module)
{
	PIMAGE_DOS_HEADER pImgDosHeaders = (PIMAGE_DOS_HEADER)module;
	PIMAGE_NT_HEADERS pImgNTHeaders = (PIMAGE_NT_HEADERS)((LPBYTE)pImgDosHeaders + pImgDosHeaders->e_lfanew);
	PIMAGE_EXPORT_DIRECTORY pImgExportDesc = (PIMAGE_EXPORT_DIRECTORY)((LPBYTE)pImgDosHeaders + pImgNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

	if (pImgDosHeaders->e_magic != IMAGE_DOS_SIGNATURE) {
		printf("libPE Error : e_magic is no valid DOS signature\n");
		return 0;
	}

	PDWORD pFuncNames = (PDWORD)((DWORD_PTR)module + pImgExportDesc->AddressOfNames);
	PDWORD pFuncPtrs = (PDWORD)((DWORD_PTR)module + pImgExportDesc->AddressOfFunctions);
	for (int i = 0; i < pImgExportDesc->NumberOfFunctions; i++)
	{
		char *modFuncName = (char*)((DWORD_PTR)module + pFuncNames[i]);
		if (!_stricmp(function, modFuncName))
		{
			return (LPVOID*)&pFuncPtrs[i];
		}
	}
	return 0;
}

void *DetourIATptr(const char *function, void *newfunction, HMODULE module)
{
	void **funcptr = IATfind(function, module);
	if (funcptr == 0)
		return 0;
	if (*funcptr == newfunction)
		return 0;

	DWORD oldrights, newrights = PAGE_READWRITE;
	//Update the protection to READWRITE
	VirtualProtect(funcptr, sizeof(LPVOID), newrights, &oldrights);

	void *originalFunc = nullptr;
	if (funcptr != nullptr)
		originalFunc = *funcptr;

	*funcptr = newfunction;

	//Restore the old memory protection flags.
	VirtualProtect(funcptr, sizeof(LPVOID), oldrights, &newrights);
	return originalFunc;
}

void *DetourEATptr(const char *function, void *newfunction, HMODULE module)
{
	void **funcptr = EATfind(function, module);
	if (funcptr == 0)
		return 0;
	if (*funcptr == newfunction)
		return 0;

	DWORD oldrights, newrights = PAGE_READWRITE;
	//Update the protection to READWRITE
	VirtualProtect(funcptr, sizeof(LPVOID), newrights, &oldrights);

	void *originalFunc = nullptr;
	if (funcptr != nullptr)
		originalFunc = (PVOID)((DWORD)*funcptr + DWORD_PTR(module));

	*funcptr = (PVOID)((DWORD_PTR)newfunction - DWORD_PTR(module));

	//Restore the old memory protection flags.
	VirtualProtect(funcptr, sizeof(LPVOID), oldrights, &newrights);
	return originalFunc;
}
