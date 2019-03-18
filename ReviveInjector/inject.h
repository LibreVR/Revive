/* 
Note: The files should be arranged as follows:
   ReviveInjector_x64.exe
   ReviveInjector_x86.exe
   x86\openvr_api.dll
   x86\LibRevive32_1.dll
   x64\openvr_api.dll
   x64\LibRevive64_1.dll
*/
#pragma once

#include <stdio.h>

extern FILE* g_LogFile;
#define LOG(x, ...) if (g_LogFile) fprintf(g_LogFile, x, __VA_ARGS__); \
					printf(x, __VA_ARGS__); \
					fflush(g_LogFile);

int CreateProcessAndInject(wchar_t *programPath, bool xr);
int OpenProcessAndInject(wchar_t *processId, bool xr);
int GetLibraryPath(char *path, int length, const char *fileName);
