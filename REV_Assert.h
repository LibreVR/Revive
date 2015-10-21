#pragma once

#include <string.h>
#include <crtdbg.h>

#define REV_UNIMPLEMENTED _ASSERT_EXPR(false, L"Make VR compatible again.")
#define REV_UNIMPLEMENTED_STRUCT(s) REV_UNIMPLEMENTED; ##s stub; memset(&stub, 0, sizeof(stub)); return stub;
#define REV_UNIMPLEMENTED_NULL REV_UNIMPLEMENTED; return NULL;
#define REV_UNIMPLEMENTED_RUNTIME REV_UNIMPLEMENTED; return ovrError_RuntimeException;
