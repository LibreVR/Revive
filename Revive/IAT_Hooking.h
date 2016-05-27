#include <windows.h>

void *DetourIATptr(const char *function, void *newfunction, HMODULE module);
