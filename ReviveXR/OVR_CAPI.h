#define ovr_GetRenderDesc ovr_GetRenderDesc2
#define ovr_SubmitFrame ovr_SubmitFrame2
#include <OVR_CAPI.h>
#undef ovr_SubmitFrame
#undef ovr_GetRenderDesc
