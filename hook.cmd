@echo off
if not exist "%~dp0ReviveInjector.exe" (
	echo ReviveInjector.exe not found
	pause
	exit
)
set /p exe="Enter executable name to hook: "
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\%exe%" /v Debugger 2>NUL | Find "ReviveInjector.exe"
if %errorlevel% equ 0 (
	echo Removing existing hook on %exe%
	reg delete "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\%exe%" /f
	pause
	exit
)
reg query "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\%exe%" 2>NUL 1>NUL
if %errorlevel% equ 0 (
	echo Cannot hook a system program
	pause
	exit
)
reg add "HKLM\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Image File Execution Options\%exe%" /v Debugger /t REG_SZ /d "%~dp0ReviveInjector.exe /debug"
if %errorlevel% neq 0 echo Right-click this script and select "Run as administrator" to try again
pause