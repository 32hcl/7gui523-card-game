# Build a portable release without depending on the original checkout location.
[CmdletBinding()]
param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$MinGWBin = $env:MINGW_BIN,
    [string]$CMakeExe = 'cmake',
    [string]$NinjaExe = 'ninja'
)
$ErrorActionPreference = 'Stop'
$ProjectRoot = $PSScriptRoot
$BuildDir = Join-Path $ProjectRoot 'build-package'

if (-not $QtRoot -or -not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin/windeployqt.exe'))) {
    throw 'Specify -QtRoot (or QT_ROOT) pointing to a Qt MinGW kit containing bin/windeployqt.exe.'
}
if (-not $MinGWBin -or -not (Test-Path -LiteralPath (Join-Path $MinGWBin 'g++.exe'))) {
    throw 'Specify -MinGWBin (or MINGW_BIN) pointing to the matching MinGW bin directory.'
}
$CMakePath = (Get-Command $CMakeExe -ErrorAction Stop).Source
$NinjaPath = (Get-Command $NinjaExe -ErrorAction Stop).Source
$OriginalPath = $env:Path
try {
    $env:Path = "$(Join-Path $QtRoot 'bin');$MinGWBin;$OriginalPath"
    & $CMakePath -S $ProjectRoot -B $BuildDir -G Ninja `
        '-DCMAKE_BUILD_TYPE=Release' '-DBUILD_GUI=ON' `
        "-DCMAKE_PREFIX_PATH=$QtRoot" "-DCMAKE_MAKE_PROGRAM=$NinjaPath" `
        "-DCMAKE_CXX_COMPILER=$(Join-Path $MinGWBin 'g++.exe')"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $CMakePath --build $BuildDir --target game unit_tests --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
    & (Join-Path $BuildDir 'unit_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed; packaging stopped.' }

    # Keep existing releases and saved games intact. Each package gets a new directory.
    $ReleaseDir = Join-Path $ProjectRoot ('release/package-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $ReleaseDir | Out-Null
    Copy-Item -LiteralPath (Join-Path $BuildDir 'game.exe') -Destination $ReleaseDir
    foreach ($asset in @('cards', 'music')) {
        $Source = Join-Path $BuildDir $asset
        if (-not (Test-Path -LiteralPath $Source)) { throw "Missing runtime assets: $Source" }
        Copy-Item -LiteralPath $Source -Destination $ReleaseDir -Recurse
    }
    Copy-Item -LiteralPath (Join-Path $ProjectRoot 'LICENSE') -Destination $ReleaseDir
    & (Join-Path $QtRoot 'bin/windeployqt.exe') --release --no-translations (Join-Path $ReleaseDir 'game.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
    foreach ($dll in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $MinGWBin $dll) -Destination $ReleaseDir
    }
    Write-Host "Package ready: $ReleaseDir" -ForegroundColor Green
} finally {
    $env:Path = $OriginalPath
}
