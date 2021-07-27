param(
    # Replace default path with system Qt installation folder if necessary
    [string] $QtInstallPath = "C:\Qt\5.15.2",
    [string] $QtCompile32 = "msvc2019",
    [string] $QtCompile64 = "msvc2019_64",
    [string] $AsioSDKName = "ASIOSDK2.3.2",
    [string] $AsioSDKUrl = "https://www.steinberg.net/sdk_downloads/ASIOSDK2.3.2.zip",
    [string] $NsisName = "nsis-3.06.1",
    [string] $NsisUrl = "https://downloads.sourceforge.net/project/nsis/NSIS%203/3.06.1/nsis-3.06.1.zip",
    [string] $VsDistFile64Redist = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Redist\",
    [string] $VsDistFile64Path = "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Redist\MSVC\14.29.30036\x64\Microsoft.VC142.CRT"
)

# change directory to the directory above (if needed)
Set-Location -Path "$PSScriptRoot\..\"

# Global constants
$RootPath = "$PWD"
$BuildPath = "$RootPath\build"
$DeployPath = "$RootPath\deploy"
$WindowsPath ="$RootPath\windows"
$AppName = "Koord-RealTime"

# Stop at all errors
$ErrorActionPreference = "Stop"


# Execute native command with errorlevel handling
Function Invoke-Native-Command {
    Param(
        [string] $Command,
        [string[]] $Arguments
    )

    & "$Command" @Arguments

    if ($LastExitCode -Ne 0)
    {
        Throw "Native command $Command returned with exit code $LastExitCode"
    }
}

# Cleanup existing build folders
Function Clean-Build-Environment
{
    if (Test-Path -Path $BuildPath) { Remove-Item -Path $BuildPath -Recurse -Force }
    if (Test-Path -Path $DeployPath) { Remove-Item -Path $DeployPath -Recurse -Force }

    New-Item -Path $BuildPath -ItemType Directory
    New-Item -Path $DeployPath -ItemType Directory
}

# For sourceforge links we need to get the correct mirror (especially NISIS) Thanks: https://www.powershellmagazine.com/2013/01/29/pstip-retrieve-a-redirected-url/
Function Get-RedirectedUrl {

    Param (
        [Parameter(Mandatory=$true)]
        [String]$URL
    )

    $request = [System.Net.WebRequest]::Create($url)
    $request.AllowAutoRedirect=$false
    $response=$request.GetResponse()

    if ($response.StatusCode -eq "Found")
    {
        $response.GetResponseHeader("Location")
    }
}

function Initialize-Module-Here ($m) { # see https://stackoverflow.com/a/51692402

    # If module is imported say that and do nothing
    if (Get-Module | Where-Object {$_.Name -eq $m}) {
        Write-Output "Module $m is already imported."
    }
    else {

        # If module is not imported, but available on disk then import
        if (Get-Module -ListAvailable | Where-Object {$_.Name -eq $m}) {
            Import-Module $m
        }
        else {

            # If module is not imported, not available on disk, but is in online gallery then install and import
            if (Find-Module -Name $m | Where-Object {$_.Name -eq $m}) {
                Install-Module -Name $m -Force -Verbose -Scope CurrentUser
                Import-Module $m
            }
            else {

                # If module is not imported, not available and not in online gallery then abort
                Write-Output "Module $m not imported, not available and not in online gallery, exiting."
                EXIT 1
            }
        }
    }
}

# Download and uncompress dependency in ZIP format
Function Install-Dependency
{
    param(
        [Parameter(Mandatory=$true)]
        [string] $Uri,
        [Parameter(Mandatory=$true)]
        [string] $Name,
        [Parameter(Mandatory=$true)]
        [string] $Destination
    )

    if (Test-Path -Path "$WindowsPath\$Destination") { return }

    $TempFileName = [System.IO.Path]::GetTempFileName() + ".zip"
    $TempDir = [System.IO.Path]::GetTempPath()

    if ($Uri -Match "downloads.sourceforge.net")
    {
      $Uri = Get-RedirectedUrl -URL $Uri
    }

    Invoke-WebRequest -Uri $Uri -OutFile $TempFileName
    echo $TempFileName
    Expand-Archive -Path $TempFileName -DestinationPath $TempDir -Force
    echo $WindowsPath\$Destination
    Move-Item -Path "$TempDir\$Name" -Destination "$WindowsPath\$Destination" -Force
    Remove-Item -Path $TempFileName -Force
}

# Install VSSetup (Visual Studio detection), ASIO SDK and NSIS Installer
Function Install-Dependencies
{
    if (-not (Get-PackageProvider -Name nuget).Name -eq "nuget") {
      Install-PackageProvider -Name "Nuget" -Scope CurrentUser -Force
    }
    Initialize-Module-Here -m "VSSetup"
    Install-Dependency -Uri $AsioSDKUrl `
        -Name $AsioSDKName -Destination "ASIOSDK2"
    Install-Dependency -Uri $NsisUrl `
        -Name $NsisName -Destination "NSIS"
}

# Setup environment variables and build tool paths
Function Initialize-Build-Environment
{
    param(
        [Parameter(Mandatory=$true)]
        [string] $QtInstallPath,
        [Parameter(Mandatory=$true)]
        [string] $BuildArch
    )

    # Look for Visual Studio/Build Tools 2017 or later (version 15.0 or above)
    $VsInstallPath = Get-VSSetupInstance | `
        Select-VSSetupInstance -Product "*" -Version "15.0" -Latest | `
        Select-Object -ExpandProperty "InstallationPath"

    if ($VsInstallPath -Eq "") { $VsInstallPath = "<N/A>" }

    if ($BuildArch -Eq "x86_64")
    {
        $VcVarsBin = "$VsInstallPath\VC\Auxiliary\build\vcvars64.bat"
        $QtMsvcSpecPath = "$QtInstallPath\$QtCompile64\bin"
    }
    else
    {
        $VcVarsBin = "$VsInstallPath\VC\Auxiliary\build\vcvars32.bat"
        $QtMsvcSpecPath = "$QtInstallPath\$QtCompile32\bin"
    }

    # Setup Qt executables paths for later calls
    Set-Item Env:QtQmakePath "$QtMsvcSpecPath\qmake.exe"
    Set-Item Env:QtWinDeployPath "$QtMsvcSpecPath\windeployqt.exe"

    ""
    "**********************************************************************"
    "Using Visual Studio/Build Tools environment settings located at"
    $VcVarsBin
    "**********************************************************************"
    ""
    "**********************************************************************"
    "Using Qt binaries for Visual C++ located at"
    $QtMsvcSpecPath
    "**********************************************************************"
    ""

    if (-Not (Test-Path -Path $VcVarsBin))
    {
        Throw "Microsoft Visual Studio ($BuildArch variant) is not installed. " + `
            "Please install Visual Studio 2017 or above it before running this script."
    }

    if (-Not (Test-Path -Path $Env:QtQmakePath))
    {
        Throw "The Qt binaries for Microsoft Visual C++ 2017 or above could not be located at $QtMsvcSpecPath. " + `
            "Please install Qt with support for MSVC 2017 or above before running this script," + `
            "then call this script with the Qt install location, for example C:\Qt\5.15.2"
    }

    # Import environment variables set by vcvarsXX.bat into current scope
    $EnvDump = [System.IO.Path]::GetTempFileName()
    Invoke-Native-Command -Command "cmd" `
        -Arguments ("/c", "`"$VcVarsBin`" && set > `"$EnvDump`"")

    foreach ($_ in Get-Content -Path $EnvDump)
    {
        if ($_ -Match "^([^=]+)=(.*)$")
        {
            Set-Item "Env:$($Matches[1])" $Matches[2]
        }
    }

    Remove-Item -Path $EnvDump -Force
}

# Build Koord-RealTime x86_64 and x86
Function Build-App
{
    param(
        [Parameter(Mandatory=$true)]
        [string] $BuildConfig,
        [Parameter(Mandatory=$true)]
        [string] $BuildArch
    )

    # Build kdasioconfig Qt project with CMake / nmake
    Invoke-Native-Command -Command "$Env:QtCmakePath" `
        -Arguments ("-DCMAKE_PREFIX_PATH='$QtInstallPath\$QtCompile64\lib\cmake'", `
            "-DCMAKE_BUILD_TYPE=Release", `
            "-S", "$RootPath\KoordASIO\src\kdasioconfig", `
            "-B", "$BuildPath\$BuildConfig\kdasioconfig", `
            "-G", "NMake Makefiles")
    Set-Location -Path "$BuildPath\$BuildConfig\kdasioconfig"
    # Invoke-Native-Command -Command "nmake" -Arguments ("$BuildConfig")
    Invoke-Native-Command -Command "nmake"

    # Build FlexASIO dlls with CMake / nmake
    Invoke-Native-Command -Command "$Env:QtCmakePath" `
        -Arguments ("-DCMAKE_PREFIX_PATH='$QtInstallPath\$QtCompile64\lib\cmake:$RootPath\KoordASIO\src\dechamps_cpputil:$RootPath\KoordASIO\src\dechamps_ASIOUtil'", `
            "-DCMAKE_BUILD_TYPE=Release", `
            "-S", "$RootPath\KoordASIO\src", `
            "-B", "$BuildPath\$BuildConfig\flexasio", `
            "-G", "NMake Makefiles")
    Set-Location -Path "$BuildPath\$BuildConfig\flexasio"
    Invoke-Native-Command -Command "nmake"

    # Now build rest of Koord-Realtime
    Invoke-Native-Command -Command "$Env:QtQmakePath" `
        -Arguments ("$RootPath\$AppName.pro", "CONFIG+=$BuildConfig $BuildArch", `
        "-o", "$BuildPath\Makefile")
    Set-Location -Path $BuildPath
    Invoke-Native-Command -Command "nmake" -Arguments ("$BuildConfig")
    Invoke-Native-Command -Command "$Env:QtWinDeployPath" `
        -Arguments ("--$BuildConfig", "--no-compiler-runtime", "--dir=$DeployPath\$BuildArch",
        "$BuildPath\$BuildConfig\$AppName.exe")
    Move-Item -Path "$BuildPath\$BuildConfig\$AppName.exe" -Destination "$DeployPath\$BuildArch" -Force

    # Transfer VS dist DLLs for x64
    Copy-Item -Path "$VsDistFile64Path\*" -Destination "$DeployPath\$BuildArch"

    # all build files:
        # kdasioconfig files inc qt dlls now in 
            # D:/a/KoordASIO/KoordASIO/deploy/x86_64/
                # - kdasioconfig.exe
                # all qt dlls etc ...
        # flexasio files in:
            # D:\a\KoordASIO\KoordASIO\build\flexasio\install\bin
                # - FlexASIO.dll
                # - portaudio_x64.dll 

    # Move kdasioconfig.exe to deploy dir
    Move-Item -Path "$BuildPath\$BuildConfig\kdasioconfig\kdasioconfig.exe" -Destination "$DeployPath\$BuildArch" -Force
    # Move 2 x FlexASIO dlls to deploy dir, rename DLL here for separation
    Move-Item -Path "$BuildPath\$BuildConfig\flexasio\install\bin\KoordASIO.dll" -Destination "$DeployPath\$BuildArch" -Force
    Move-Item -Path "$BuildPath\$BuildConfig\flexasio\install\bin\portaudio_x64.dll" -Destination "$DeployPath\$BuildArch" -Force

    # clean up
    Invoke-Native-Command -Command "nmake" -Arguments ("clean")
    Set-Location -Path $RootPath
}

# Build and deploy Koord-RealTime 64bit and 32bit variants
function Build-App-Variants
{
    param(
        [Parameter(Mandatory=$true)]
        [string] $QtInstallPath
    )

    # # foreach ($_ in ("x86_64", "x86"))
    # foreach ($_ in ("x86_64"))
    # {
    #     $OriginalEnv = Get-ChildItem Env:
    #     Initialize-Build-Environment -QtInstallPath $QtInstallPath -BuildArch $_
    #     Build-App -BuildConfig "release" -BuildArch $_
    #     $OriginalEnv | % { Set-Item "Env:$($_.Name)" $_.Value }
    # }
    
    # do 64bit only
    $archbin="x86_64"
    $OriginalEnv = Get-ChildItem Env:
    Initialize-Build-Environment -QtInstallPath $QtInstallPath -BuildArch $archbin
    Build-App -BuildConfig "release" -BuildArch $archbin
    $OriginalEnv | % { Set-Item "Env:$($archbin.Name)" $archbin.Value }
    
}

# Build Windows installer
Function Build-Installer
{
    foreach ($_ in Get-Content -Path "$RootPath\$AppName.pro")
    {
        if ($_ -Match "^VERSION *= *(.*)$")
        {
            $AppVersion = $Matches[1]
            break
        }
    }

    Invoke-Native-Command -Command "$WindowsPath\NSIS\makensis" `
        -Arguments ("/v4", "/DAPP_NAME=$AppName", "/DAPP_VERSION=$AppVersion", `
        "/DROOT_PATH=$RootPath", "/DWINDOWS_PATH=$WindowsPath", "/DDEPLOY_PATH=$DeployPath", `
        "$WindowsPath\installer.nsi")
}

# Build and copy NS-Process dll
Function Build-NSProcess
{
    param(
        [Parameter(Mandatory=$true)]
        [string] $QtInstallPath
    )
    if (!(Test-Path -path "$WindowsPath\nsProcess.dll")) {

        echo "Building nsProcess..."

        $OriginalEnv = Get-ChildItem Env:
        Initialize-Build-Environment -QtInstallPath $QtInstallPath -BuildArch "x86"
    
        Invoke-Native-Command -Command "msbuild" `
            -Arguments ("$WindowsPath\nsProcess\nsProcess.sln", '/p:Configuration="Release UNICODE"', `
            "/p:Platform=Win32")
   
        Move-Item -Path "$WindowsPath\nsProcess\Release\nsProcess.dll" -Destination "$WindowsPath\nsProcess.dll" -Force
        Remove-Item -Path "$WindowsPath\nsProcess\Release\" -Force -Recurse
        $OriginalEnv | % { Set-Item "Env:$($_.Name)" $_.Value }
    }
}

Clean-Build-Environment
Install-Dependencies
Build-App-Variants -QtInstallPath $QtInstallPath
Build-NSProcess -QtInstallPath $QtInstallPath
Build-Installer
