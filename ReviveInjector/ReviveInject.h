/* 
Note: The files should be arranged as follows:
   ReviveInjector.exe
   Revive\x86\openvr_api.dll
   Revive\x86\LibRevive32_1.dll
   Revive\x86\OVRPlugin.dll
   Revive\x64\openvr_api.dll
   Revive\x64\LibRevive64_1.dll
   Revive\x64\OVRPlugin.dll

   The OVRPlugin and openvr_api dlls are located in the 'Revive' folder in the solution root
*/
int CreateProcessAndInject(char *programPath);