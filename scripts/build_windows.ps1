param(
  [Parameter(Mandatory=$true)][string]$PortAudioSource,
  [Parameter(Mandatory=$true)][string]$AsioSdkRoot,
  [string]$Configuration = "Release"
)
$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$BuildDirectory = Join-Path $ProjectRoot "build"
cmake -S $ProjectRoot -B $BuildDirectory "-DLP_PORTAUDIO_SOURCE=$PortAudioSource" "-DASIOSDK_ROOT_DIR=$AsioSdkRoot" "-DCMAKE_BUILD_TYPE=$Configuration"
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed" }
cmake --build $BuildDirectory --config $Configuration --target LivePerformance
if ($LASTEXITCODE -ne 0) { throw "Windows build failed" }
