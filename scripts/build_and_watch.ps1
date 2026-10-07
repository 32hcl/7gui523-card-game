param(
    [int]$WatchInterval = 2
)

$projectDir = "D:\code\c++\.vscode\7gui523"
$buildDir = "$projectDir\build"
$logFile = "$buildDir\debug.log"
$qtPath = "D:\app\qt-c++\6.11.2\mingw_64\bin"
$mingwPath = "D:\app\qt-c++\Tools\mingw1310_64\bin"

$env:PATH = "$mingwPath;$qtPath;$env:PATH"

Write-Host "=== 7gui523 Build + Run + Watch ===" -ForegroundColor Cyan

# Stop existing game
Stop-Process -Name "game" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# Build
Write-Host "[BUILD] Compiling..." -ForegroundColor Yellow
Push-Location $projectDir
& cmake --build build --config Debug 2>&1 | ForEach-Object { Write-Host $_ }
if ($LASTEXITCODE -ne 0) {
    Write-Host "[BUILD] FAILED!" -ForegroundColor Red
    Pop-Location
    exit 1
}
Pop-Location
Write-Host "[BUILD] OK" -ForegroundColor Green

# Clear log
if (Test-Path $logFile) { Remove-Item $logFile -Force }

# Launch game
Write-Host "[RUN] Starting game.exe..." -ForegroundColor Yellow
Push-Location $buildDir
Start-Process -FilePath ".\game.exe" -RedirectStandardError "$buildDir\stderr.log"
Pop-Location
Start-Sleep -Seconds 2

# Watch log
Write-Host "[WATCH] Monitoring log (Ctrl+C to stop)..." -ForegroundColor Yellow
& "$projectDir\scripts\watch_log.ps1" -LogFile $logFile -IntervalSec $WatchInterval