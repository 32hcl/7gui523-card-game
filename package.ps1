# package.ps1
# 一键编译 + 打包发布

$ErrorActionPreference = "Stop"

# ========== 路径配置 ==========
$ProjectRoot = "D:\code\c++\.vscode\7gui523"
$BuildDir    = "$ProjectRoot\build"
$ReleaseDir  = "$ProjectRoot\release"
$QtBin       = "D:\app\qt-c++\6.11.2\mingw_64\bin"
$QtTools     = "D:\app\qt-c++\Tools"
$MinGWBin    = "$QtTools\mingw1310_64\bin"

# ========== 1. 设置环境变量 ==========
Write-Host "[1/6] 设置环境变量..." -ForegroundColor Cyan
$env:Path = "$QtBin;$MinGWBin;$QtTools\CMake_64\bin;$QtTools\Ninja;" + $env:Path

# ========== 2. 检查 build 目录 ==========
Write-Host "[2/6] 检查 build 目录..." -ForegroundColor Cyan
if (-not (Test-Path "$BuildDir\CMakeCache.txt")) {
    Write-Host "build 目录不存在或未配置，正在初始化..." -ForegroundColor Yellow
    if (-not (Test-Path $BuildDir)) {
        New-Item -ItemType Directory -Path $BuildDir | Out-Null
    }
    cd $BuildDir
    cmake .. -G Ninja `
        -DCMAKE_PREFIX_PATH=D:/app/qt-c++/6.11.2/mingw_64 `
        -DCMAKE_MAKE_PROGRAM=$QtTools/Ninja/ninja.exe `
        -DCMAKE_CXX_COMPILER=$MinGWBin/g++.exe
    if ($LASTEXITCODE -ne 0) {
        Write-Host "CMake 配置失败。" -ForegroundColor Red
        exit 1
    }
} else {
    cd $BuildDir
}

# ========== 3. 编译 ==========
Write-Host "[3/6] 编译项目..." -ForegroundColor Cyan
cmake --build .
if ($LASTEXITCODE -ne 0) {
    Write-Host "编译失败，停止打包。" -ForegroundColor Red
    exit 1
}

# 检查 game.exe 是否生成
if (-not (Test-Path "$BuildDir\game.exe")) {
    Write-Host "找不到 game.exe，编译可能失败。" -ForegroundColor Red
    exit 1
}

# ========== 4. 准备 release 目录 ==========
Write-Host "[4/6] 准备 release 目录..." -ForegroundColor Cyan
if (Test-Path $ReleaseDir) {
    Remove-Item -Recurse -Force $ReleaseDir
}
New-Item -ItemType Directory -Path $ReleaseDir | Out-Null

# ========== 5. 拷贝 exe ==========
Write-Host "[5/6] 拷贝 game.exe..." -ForegroundColor Cyan
Copy-Item "$BuildDir\game.exe" "$ReleaseDir\game.exe"

# ========== 6. 用 windeployqt 复制依赖 ==========
Write-Host "[6/6] 用 windeployqt 复制 Qt 依赖..." -ForegroundColor Cyan
& "$QtBin\windeployqt.exe" --release --no-translations "$ReleaseDir\game.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "windeployqt 失败。" -ForegroundColor Red
    exit 1
}

# ========== 补充 MinGW 运行库（防止 windeployqt 漏拷）==========
$mingwDlls = @(
    "libgcc_s_seh-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)
foreach ($dll in $mingwDlls) {
    $src = Join-Path $MinGWBin $dll
    $dst = Join-Path $ReleaseDir $dll
    if ((Test-Path $src) -and (-not (Test-Path $dst))) {
        Copy-Item $src $dst
        Write-Host "  补充: $dll" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "打包完成！" -ForegroundColor Green
Write-Host "发布目录: $ReleaseDir" -ForegroundColor Green
Write-Host "双击 $ReleaseDir\game.exe 即可运行。" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green