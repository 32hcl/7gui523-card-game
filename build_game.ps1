# 七鬼523 构建脚本（唯一正确版本，取代已删除的 build_ui.ps1）
# ⚠️ 必须用 Qt 自带的 64 位 MinGW 13.1：
#    PATH 中默认的 D:\code\w64devkit 是 32 位(i686)，与 64 位 Qt6 位数校验失败，
#    会报 "Qt6Config.cmake version 6.11.2 (64bit) not compatible" 导致 game 目标不生成。
#    旧 32 位构建目录（build / build_7gui523）已删除，唯一构建目录为 build-mingw64。

$ErrorActionPreference = 'Stop'

# ── 1. 配置（指定 64 位编译器） ──
Write-Host "`n=== CMake 配置 ===" -ForegroundColor Cyan
cmake -B d:\code\c++\.vscode\7gui523\build-mingw64 -S d:\code\c++\.vscode\7gui523 -G Ninja `
    -DCMAKE_C_COMPILER="D:/app/qt-c++/Tools/mingw1310_64/bin/gcc.exe" `
    -DCMAKE_CXX_COMPILER="D:/app/qt-c++/Tools/mingw1310_64/bin/g++.exe" `
    -DCMAKE_MAKE_PROGRAM="D:/app/qt-c++/Tools/Ninja/ninja.exe" `
    -DQt6_DIR="D:/app/qt-c++/6.11.2/mingw_64/lib/cmake/Qt6" `
    -DCMAKE_PREFIX_PATH="D:/app/qt-c++/6.11.2/mingw_64"

if ($LASTEXITCODE -ne 0) { Write-Host "`n配置失败！" -ForegroundColor Red; exit 1 }

# ── 2. 构建（默认 game，可传参如 .\build_game.ps1 all） ──
$Target = if ($args.Count -gt 0) { $args[0] } else { 'game' }
Write-Host "`n=== 构建目标: $Target ===" -ForegroundColor Cyan
$env:PATH = "D:\app\qt-c++\Tools\mingw1310_64\bin;$env:PATH"
cmake --build d:\code\c++\.vscode\7gui523\build-mingw64 --target $Target

if ($LASTEXITCODE -ne 0) { Write-Host "`n❌ 构建失败！" -ForegroundColor Red; exit 1 }

# ── 3. game 目标额外部署 Qt 运行时（首次或 Qt 更新后需执行） ──
if ($Target -eq 'game') {
    $deployStamp = "d:\code\c++\.vscode\7gui523\build-mingw64\.deployed"
    if (-not (Test-Path $deployStamp)) {
        Write-Host "`n=== windeployqt 部署运行时 ===" -ForegroundColor Cyan
        & "D:\app\qt-c++\6.11.2\mingw_64\bin\windeployqt.exe" `
            --no-translations --no-system-d3d-compiler --no-opengl-sw `
            d:\code\c++\.vscode\7gui523\build-mingw64\game.exe
        Copy-Item "D:\app\qt-c++\Tools\mingw1310_64\bin\libgcc_s_seh-1.dll",
                  "D:\app\qt-c++\Tools\mingw1310_64\bin\libstdc++-6.dll",
                  "D:\app\qt-c++\Tools\mingw1310_64\bin\libwinpthread-1.dll" `
                  -Destination d:\code\c++\.vscode\7gui523\build-mingw64\ -Force
        New-Item -ItemType File -Path $deployStamp | Out-Null
    }
    Write-Host "`n✅ 构建成功: d:\code\c++\.vscode\7gui523\build-mingw64\game.exe（双击直接运行）" -ForegroundColor Green
} else {
    Write-Host "`n✅ 构建成功: $Target" -ForegroundColor Green
}
