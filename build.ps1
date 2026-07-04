# Build PPsVaultTracker (Release) with the VS Build Tools CMake.
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsInstall = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$cmake = Join-Path $vsInstall 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'

Write-Host "CMake: $cmake"
Write-Host "Configuring..."
# local build: module import enabled via the libopenmpt devel package
& $cmake -S $root -B (Join-Path $root 'build') -G "Visual Studio 17 2022" -A x64 `
    -DPVT_LIBOPENMPT_DIR="D:/Audio/libopenmpt"
if ($LASTEXITCODE -ne 0) { throw "Configure failed" }

Write-Host "Building (Release)..."
& $cmake --build (Join-Path $root 'build') --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host "Testing..."
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
& $ctest --test-dir (Join-Path $root 'build') -C Release --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed" }

Write-Host "Done."
