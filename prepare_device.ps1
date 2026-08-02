<#
.SYNOPSIS
    Prepares a Windows device to run LowLife development, setup, and build scripts.
.DESCRIPTION
    This script ensures the PowerShell Execution Policy allows script execution for
    the current user and adds the project folder to the Windows Defender exclusions.
    It performs these actions safely without modifying other global system settings.
#>
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "=========================================================" -ForegroundColor Yellow
    Write-Host " Running this script requires Administrator privileges    " -ForegroundColor Yellow
    Write-Host " to configure Windows Defender exclusions.               " -ForegroundColor Yellow
    Write-Host " Relaunching in an elevated PowerShell prompt...        " -ForegroundColor Yellow
    Write-Host "=========================================================" -ForegroundColor Yellow
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`"" -Verb RunAs
    exit
}

Write-Host "=========================================================" -ForegroundColor Cyan
Write-Host "           PREPARING DEVICE FOR DEVELOPMENT              " -ForegroundColor Cyan
Write-Host "=========================================================" -ForegroundColor Cyan

$currentDir = $PSScriptRoot
if (-not $currentDir) {
    $currentDir = Get-Location
}
$currentDir = (Get-Item $currentDir).FullName

Write-Host "[1/2] Configuring PowerShell Execution Policy..." -ForegroundColor Yellow
try {
    Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope CurrentUser -Force
    Set-ExecutionPolicy -ExecutionPolicy Bypass -Scope Process -Force
    Write-Host "    PowerShell Execution Policy successfully set to Bypass." -ForegroundColor Green
} catch {
    Write-Host "    WARNING: Failed to set Execution Policy globally. You may need to run PowerShell with '-ExecutionPolicy Bypass'." -ForegroundColor Red
    Write-Host "    Details: $_" -ForegroundColor DarkGray
}

Write-Host "[2/2] Whitelisting workspace directory in Windows Defender..." -ForegroundColor Yellow
try {
    Add-MpPreference -ExclusionPath $currentDir -ErrorAction Stop
    Write-Host "    Successfully whitelisted workspace: $currentDir" -ForegroundColor Green
    Write-Host "    Windows Defender will not block files/compilations in this folder." -ForegroundColor Green
} catch {
    Write-Host "    ERROR: Failed to add Windows Defender exclusion." -ForegroundColor Red
    Write-Host "    Details: $_" -ForegroundColor DarkGray
    Write-Host ""
    Write-Host "    To manually add the exclusion:" -ForegroundColor Yellow
    Write-Host "    1. Open 'Windows Security' -> 'Virus & threat protection'." -ForegroundColor Yellow
    Write-Host "    2. Under 'Virus & threat protection settings', click 'Manage settings'." -ForegroundColor Yellow
    Write-Host "    3. Scroll down to 'Exclusions' and click 'Add or remove exclusions'." -ForegroundColor Yellow
    Write-Host "    4. Click 'Add an exclusion' -> 'Folder' and select: $currentDir" -ForegroundColor Yellow
}

Write-Host "=========================================================" -ForegroundColor Green
Write-Host " DEVICE PREPARATION COMPLETE!                            " -ForegroundColor Green
Write-Host " You can now run 'setup.ps1', 'installer.ps1', and       " -ForegroundColor Green
Write-Host " builds without scripts or files being blocked.          " -ForegroundColor Green
Write-Host "=========================================================" -ForegroundColor Green

Read-Host "Press Enter to exit..."
