!define BASE_DIR "..\Release"

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

;--------------------------------
;General

  ;Name and file
  Name "Revive"
  OutFile "ReviveInstaller.exe"

  ;Default installation folder
  InstallDir "$PROGRAMFILES\Revive"
  
  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\Revive" ""

  ;Request application privileges for Windows Vista
  RequestExecutionLevel admin

;--------------------------------
;Variables

  Var StartMenuFolder

;--------------------------------
;Interface Settings

  !define MUI_ABORTWARNING

;--------------------------------
;Pages

  !insertmacro MUI_PAGE_LICENSE "..\LICENSE"
  !insertmacro MUI_PAGE_DIRECTORY
  
  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "HKCU" 
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\Revive" 
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"
  
  !insertmacro MUI_PAGE_STARTMENU Application $StartMenuFolder
  
  !insertmacro MUI_PAGE_INSTFILES
  
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES

;--------------------------------
;Languages
 
  !insertmacro MUI_LANGUAGE "English"

;--------------------------------
;Installer Sections

Section "Revive" SecRevive

  SectionIn RO
  
  SetOutPath "$INSTDIR"
  
  ; Main application files
  File "..\LICENSE"
  File /r "${BASE_DIR}\*.exe"
  File /r "${BASE_DIR}\*.dll"
  File /r "${BASE_DIR}\Qt*"
  File /r "${BASE_DIR}\translations"
  
  ; Create an empty manifest file
  FileOpen $0 "$INSTDIR\revive.vrmanifest" w
  FileClose $0
  
  ; Ensure all users have access to the manifest file
  AccessControl::GrantOnFile \
    "$INSTDIR\revive.vrmanifest" "(S-1-5-32-545)" "GenericRead + GenericWrite"
  Pop $0
  
  ; Reboot the Oculus service to prevent entitlement checks failing
  ExecWait "OVRServiceLauncher -stop"
  ExecWait "OVRServiceLauncher -start"
  
  ; Execute the dashboard overlay
  Exec '"$INSTDIR\ReviveOverlay.exe"'
  
  ;Store installation folder
  WriteRegStr HKCU "Software\Revive" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
    ;Create shortcuts
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Revive.lnk" "$INSTDIR\ReviveOverlay.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_END

SectionEnd
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"

  DetailPrint "Terminating dashboard overlay..."
  nsExec::ExecToLog '"taskkill" /F /IM ReviveOverlay.exe'

  RMDir /r "$INSTDIR"
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
    
  Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Revive.lnk"
  RMDir "$SMPROGRAMS\$StartMenuFolder"
  
  DeleteRegKey HKCU "Software\Revive"

SectionEnd