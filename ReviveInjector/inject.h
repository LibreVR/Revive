#pragma once

#include <stdio.h>

extern FILE* g_LogFile;
#define LOG(x, ...) if (g_LogFile) fprintf(g_LogFile, x, __VA_ARGS__); \
					printf(x, __VA_ARGS__); \
					fflush(g_LogFile);

unsigned int CreateProcessAndInject(wchar_t *programPath, bool xr, bool apc);
int OpenProcessAndInject(wchar_t *processId, bool xr);
int GetAbsolutePath(char *path, int length, const char *fileName);
