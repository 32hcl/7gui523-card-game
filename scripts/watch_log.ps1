param(
    [string]$LogFile = "D:\code\c++\.vscode\7gui523\build\debug.log",
    [int]$IntervalSec = 2,
    [int]$TailLines = 80
)

$ErrorActionPreference = "Continue"

$problemPatterns = @(
    @{ Name = "ILLEGAL_CARD";  Pat = "Invalid|illegal|error|Error|WARNING|FATAL" }
    @{ Name = "STUCK_LOOP";    Pat = "doAITurn.*count=\d{3,}" }
    @{ Name = "WIDGET_MISMATCH"; Pat = "Widgets= \d+.*hand= \d+" }
    @{ Name = "NULL_PTR";      Pat = "nullptr|0x0|SIGSEGV|access violation" }
    @{ Name = "REFILL_ZERO";   Pat = "refillBoth.*gotW= 0.*gotL= 0" }
    @{ Name = "AUTO_FAIL";     Pat = "autoPlayAllCards" }
)

$problemCount = 0

if (-not (Test-Path $LogFile)) {
    Write-Host "[WAIT] Log file not found, waiting..." -ForegroundColor Yellow
    while (-not (Test-Path $LogFile)) { Start-Sleep 1 }
}

while ($true) {
    if (-not (Test-Path $LogFile)) {
        Start-Sleep $IntervalSec
        continue
    }

    $content = Get-Content $LogFile -Tail $TailLines -Encoding UTF8 -ErrorAction SilentlyContinue
    if (-not $content) { Start-Sleep $IntervalSec; continue }

    $newProblems = [System.Collections.ArrayList]::new()
    $currentRound = 0
    $currentPhase = "?"
    $playerAHand = 0
    $playerBHand = 0
    $lastAction = ""

    foreach ($line in $content) {
        if ($line -match "phase= (\d)") { $currentPhase = $Matches[1] }
        if ($line -match "playerA\.hand= (\d+)") { $playerAHand = [int]$Matches[1] }
        if ($line -match "playerB\.hand= (\d+)") { $playerBHand = [int]$Matches[1] }

        foreach ($pat in $problemPatterns) {
            if ($line -match $pat.Pat) {
                [void]$newProblems.Add("[$($pat.Name)] $line")
            }
        }
    }

    Clear-Host
    $ts = Get-Date -Format "HH:mm:ss"
    Write-Host "=== 7gui523 Log Monitor $ts ===" -ForegroundColor Cyan
    Write-Host ""

    $phaseStr = switch ($currentPhase) {
        "0" { "DealAnim" }
        "1" { "PlayerTurn" }
        "2" { "AITurn" }
        "3" { "GameOver" }
        default { "?" }
    }
    Write-Host "Phase: $phaseStr | A: ${playerAHand} cards | B: ${playerBHand} cards" -ForegroundColor Yellow

    if ($newProblems.Count -gt 0) {
        Write-Host ""
        Write-Host "--- PROBLEMS ($($newProblems.Count)) ---" -ForegroundColor Red
        foreach ($p in $newProblems) {
            Write-Host "  $p" -ForegroundColor Red
        }
        $problemCount += $newProblems.Count
    } else {
        Write-Host "Problems: None" -ForegroundColor Green
    }

    Write-Host ""
    Write-Host "--- Last 15 lines ---" -ForegroundColor DarkGray
    $tail = $content | Select-Object -Last 15
    foreach ($line in $tail) {
        $c = "Gray"
        if ($line -match "Error|error|Invalid|FATAL") { $c = "Red" }
        elseif ($line -match "finishGame|GameOver") { $c = "Magenta" }
        elseif ($line -match "appendLog|出牌|不要|补牌|回合") { $c = "Cyan" }
        Write-Host "  $line" -ForegroundColor $c
    }

    Write-Host ""
    Write-Host "Total problems: $problemCount" -ForegroundColor $(if ($problemCount -gt 0) {"Red"} else {"Green"})

    Start-Sleep $IntervalSec
}