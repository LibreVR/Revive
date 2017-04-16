#pragma once

#include "microprofile.h"
#include <crtdbg.h>

#define REV_ASSERT _ASSERT
#if 0
#include <Windows.h>
#define REV_TRACE(x) OutputDebugStringA("Revive: " #x "\n");
#else
#define REV_TRACE(x) MICROPROFILE_SCOPEI("Revive", #x, 0xff0000);
#endif
