# Retrieve submodules
Write-Host "Updating git submodules..."
git submodule update --init --recursive

# Make download directory
mkdir -Force tmp_deps | Out-Null

# Retrieve Oculus SDK
Write-Host "Downloading Oculus SDK v1.43.0..."
curl -o tmp_deps\oculus_sdk.html https://developer.oculus.com/downloads/package/oculus-sdk-for-windows/1.43.0/
$url = select-string -Path 'tmp_deps\oculus_sdk.html' -Pattern 'https:\/\/securecdn\.oculus\.com\/binaries\/download\/\?id=[0-9]+&amp;access_token=[0-9A-Za-z%]+' -AllMatches | % {$_.Matches} | % {$_.Value}
Invoke-WebRequest -Uri $url -OutFile 'tmp_deps\oculus_sdk.zip'
Write-Host "Extracting Oculus SDK into Externals/..."
cd Externals
Expand-Archive -Force -Path ..\tmp_deps\oculus_sdk.zip -DestinationPath .
cd ..

# Retrieve NSIS plugins
Write-Host "Downloading required NSIS plugins..."
Invoke-WebRequest -Uri 'https://nsis.sourceforge.io/mediawiki/images/4/4a/AccessControl.zip' -OutFile 'tmp_deps\NSIS_AccessControl.zip'

Write-Host "Retrieved all dependencies."

# --------- Build
Write-Host "------------------------------------"
Write-Host "Attempting to build OpenXR & Revive"
Write-Host "------------------------------------"

# Find and bind msbuild - thank you Lex Li
Write-Host "Searching for msbuild.exe..."
$msBuild = "msbuild"
try
{
    & $msBuild /version
}
catch
{
    Write-Host "MSBuild doesn't exist. Using VSSetup instead."
    Install-Module VSSetup -Scope CurrentUser -Force
    $instance = Get-VSSetupInstance -All -Prerelease | Select-VSSetupInstance -Require 'Microsoft.Component.MSBuild' -Latest
    $installDir = $instance.installationPath
    Write-Host "Visual Studio is found at $installDir"
    $msBuild = $installDir + '\MSBuild\Current\Bin\MSBuild.exe' # VS2019
    if (![System.IO.File]::Exists($msBuild))
    {
        $msBuild = $installDir + '\MSBuild\15.0\Bin\MSBuild.exe' # VS2017
        if (![System.IO.File]::Exists($msBuild))
        {
            Write-Host "MSBuild doesn't exist. Exit."
            exit 1
        }
    }
}
Write-Host "MSBuild found"

# Build OpenXR libraries
Write-Host "Building OpenXR library..."
cd Externals/openxr
cmake -G "Visual Studio 15 2017 Win64" .
((Get-Content -path 'src\loader\openxr_loader-1_0.vcxproj' -Raw) -replace 'MultiThreadedDLL', 'MultiThreaded') | Set-Content -Path 'src\loader\openxr_loader-1_0.vcxproj'
& $msBuild ALL_BUILD.vcxproj /t:Build /p:Configuration=Release
cd ..\..

# Build Revive a la carte
Write-Host "Building Revive..."
& $msBuild Revive.sln /t:Build /p:Configuration=Release /p:Platform=x64
& $msBuild Revive.sln /t:Build /p:Configuration=Release /p:Platform=x86
