#include <windows.h>

void *DetourIATptr(const char *function, void *newfunction, HMODULE module);
void *DetourEATptr(const char *function, void *newfunction, HMODULE module);
