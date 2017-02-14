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
  InstallDir "$PROGRAMFILES64\Revive"
  
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
IfSilent install
  DetailPrint "Terminating dashboard overlay..."
  nsExec::ExecToLog '"taskkill" /F /IM ReviveOverlay.exe'
  Sleep 2000 ; give 2 seconds for the application to finish exiting
  
install:
  SectionIn RO
  
  ; If the directory already exists, use a subfolder
  IfFileExists $INSTDIR\ReviveOverlay.exe +3 0
  IfFileExists $INSTDIR\*.* 0 +2
  StrCpy $INSTDIR "$INSTDIR\Revive"
  
  SetOutPath "$INSTDIR"
  
  ; Main application files
  File "..\LICENSE"
  File "${BASE_DIR}\app.vrmanifest"
  File "${BASE_DIR}\support.vrmanifest"
  File /r "${BASE_DIR}\*.exe"
  File /r "${BASE_DIR}\*.dll"
  File /r "${BASE_DIR}\*.pdb"
  File /r "${BASE_DIR}\*.jpg"
  File /r "${BASE_DIR}\Qt*"
  File /r "${BASE_DIR}\translations"
  
  ; Create an empty manifest file
  FileOpen $0 "$INSTDIR\revive.vrmanifest" w
  FileWrite $0 ""
  FileClose $0
  
  ; Ensure all users have access to the manifest file
  AccessControl::GrantOnFile \
    "$INSTDIR\revive.vrmanifest" "(S-1-5-32-545)" "GenericRead + GenericWrite"
  Pop $0
  
  ; Install redistributable
  ExecWait '"$INSTDIR\vcredist_x64.exe" /install /quiet'
  
  ; Execute the dashboard to add the application manifest
  ExecWait '"$INSTDIR\ReviveOverlay.exe" -manifest'
  
  ;Store installation folder
  WriteRegStr HKCU "Software\Revive" "" $INSTDIR
  
  ;Create uninstaller
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  
  ; Add uninstaller to Programs and Features
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Revive" \
                   "DisplayName" "Revive Dashboard"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Revive" \
                   "UninstallString" "$\"$INSTDIR\Uninstall.exe$\""
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Revive" \
                   "DisplayIcon" "$INSTDIR\ReviveOverlay.exe,0"
  
  !insertmacro MUI_STARTMENU_WRITE_BEGIN Application
    
    ;Create shortcuts
    CreateDirectory "$SMPROGRAMS\$StartMenuFolder"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk" "$INSTDIR\Uninstall.exe"
    CreateShortCut "$SMPROGRAMS\$StartMenuFolder\Revive Dashboard.lnk" "$INSTDIR\ReviveOverlay.exe"
  
  !insertmacro MUI_STARTMENU_WRITE_END

  ; If SteamVR is already running, execute the dashboard as the user
  FindWindow $0 "Qt5QWindowIcon" "SteamVR Status"
  StrCmp $0 0 +2
  Exec '"$WINDIR\explorer.exe" "$INSTDIR\ReviveOverlay.exe"'
SectionEnd
 
;--------------------------------
;Uninstaller Section

Section "Uninstall"

  DetailPrint "Terminating dashboard overlay..."
  nsExec::ExecToLog '"taskkill" /F /IM ReviveOverlay.exe'
  Sleep 2000 ; give 2 seconds for the application to finish exiting

  RMDir /r "$INSTDIR"
  
  !insertmacro MUI_STARTMENU_GETFOLDER Application $StartMenuFolder
    
  Delete "$SMPROGRAMS\$StartMenuFolder\Uninstall.lnk"
  Delete "$SMPROGRAMS\$StartMenuFolder\Revive Dashboard.lnk"
  RMDir "$SMPROGRAMS\$StartMenuFolder"
  
  DeleteRegKey HKCU "Software\Revive"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\Revive"

SectionEnd