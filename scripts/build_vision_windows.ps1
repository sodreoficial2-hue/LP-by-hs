param(
 [Parameter(Mandatory=$true)][string]$PortAudioSource,
 [Parameter(Mandatory=$true)][string]$AsioSdkRoot,
 [string]$WebView2SdkRoot = '',
 [string]$BuildDirectory = 'build-vision'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$argsCmake = @('-S', $repo, '-B', $BuildDirectory, '-A', 'x64',
 '-DLP_ENABLE_VISION_UI=ON', '-DCMAKE_POLICY_VERSION_MINIMUM=3.5',
 "-DLP_PORTAUDIO_SOURCE=$PortAudioSource", "-DASIOSDK_ROOT_DIR=$AsioSdkRoot")
if ($WebView2SdkRoot) { $argsCmake += "-DLP_WEBVIEW2_SDK_ROOT=$WebView2SdkRoot" }
& cmake @argsCmake
if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar a integração Vision.' }
& cmake --build $BuildDirectory --config Release --target LivePerformanceVision LivePerformance --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar a integração Vision.' }
Write-Host 'Compilação concluída. O executável Vision utiliza o WebView2 Runtime instalado no Windows.'
Write-Host 'Mantenha vision-ui, WebView2Loader.dll e as bibliotecas atuais de áudio ao lado do executável.'
Write-Host 'Para timbres SF2, mantenha também o runtime FluidSynth completo do portátil atual.'
