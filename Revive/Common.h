#pragma once

#include "microprofile.h"

#if 0
#include <Windows.h>
#define REV_TRACE(x) OutputDebugStringA("Revive: " #x "\n");
#else
#define REV_TRACE(x) MICROPROFILE_SCOPEI("Revive", #x, 0xff0000);
#endif
