(Get-Content ..\Externals\LibOVR\Shim\OVR_CAPIShim.c) `
    -replace '(#include "OVR_CAPI.h")', "#include `"microprofile.h`"`n#define ovr_GetRenderDesc ovr_GetRenderDesc2`n#define ovr_SubmitFrame ovr_SubmitFrame2`n`$1" `
    -replace '(#include "OVR_Version.h")', "`$1`n#undef OVR_MIN_REQUESTABLE_MINOR_VERSION`n#define OVR_MIN_REQUESTABLE_MINOR_VERSION 0`nvoid AttachDetours();`nvoid DetachDetours();" `
    -replace '(result = OVR_LoadSharedLibrary\(OVR_PRODUCT_VERSION, OVR_MAJOR_VERSION\);)', "MicroProfileSetForceEnable(true);`n  MicroProfileSetEnableAllGroups(true);`n  MicroProfileSetForceMetaCounters(true);`n  MicroProfileWebServerStart();`n  DetachDetours();`n  `$1`n  AttachDetours();" `
    -replace 'return API.([A-Z|_|a-z]*)', "MICROPROFILE_SCOPEI(`"OVR`", `"`$1`", 0xff0000);`n  return API.`$1" `
    -replace '(MICROPROFILE_SCOPEI\("OVR", "ovr_(Submit|End)Frame", 0xff0000\);)', "`$1`n  MICROPROFILE_META_CPU(`"Frame Index`", (int)frameIndex);`n  MicroProfileFlip();" |
  Out-File OVR_CAPIShim.c