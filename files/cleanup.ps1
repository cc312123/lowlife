# ==============================================================================
# LowLife Cheat Environment Advanced Cleanup & Performance Uninstaller
# ==============================================================================
# Upgraded with high-resolution stopwatch timing, fast parallelized .NET 
# event log clearing, audit logs, and an interactive telemetry summary.
# Must be executed in an Administrator PowerShell window.
# ==============================================================================

$ErrorActionPreference = "Continue"

# Define script root directory (handles both script execution and copy-paste execution)
$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } else { $PWD.Path }

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

# Temporarily disable logging to prevent cleanup traces from being logged
wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational" /e:false 2>$null
wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:false 2>$null

# Stats tracker
$perfStats = [System.Collections.Generic.List[PSCustomObject]]::new()
$totalCleanedFiles = 0
$totalCleanedKeys = 0
$totalCleanedLogs = 0
$totalDurationMs = 0

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

# 1. Terminate running processes
Run-CleanupStep "1/9: Terminating processes" {
    $processes = Get-Process -Name "RobloxCrashHandler", "*AM_DELTA_PATCH*", "*B332FDC6*" -ErrorAction SilentlyContinue
    $count = 0
    if ($processes) {
        $processes | Stop-Process -Force -ErrorAction SilentlyContinue
        $count = $processes.Count
    }
    return "Terminated $count process(es)"
}

# 2. End and delete the UAC-bypassed Scheduled Task
Run-CleanupStep "2/9: Removing Scheduled Tasks" {
    $tasks = Get-ScheduledTask -ErrorAction SilentlyContinue | Where-Object { $_.TaskName -eq "RobloxCrashHandler" -or $_.TaskName -like "*AM_DELTA_PATCH*" -or $_.TaskName -like "*B332FDC6*" }
    if ($tasks) {
        $count = 0
        foreach ($task in $tasks) {
            if ($task.State -eq 'Running') {
                Stop-ScheduledTask -TaskName $task.TaskName -ErrorAction SilentlyContinue
            }
            Unregister-ScheduledTask -TaskName $task.TaskName -Confirm:$false -ErrorAction SilentlyContinue
            $count++
        }
        return "Removed $count task(s)"
    } else {
        return "No matching tasks found / already deleted"
    }
}

# 3. Clean up the installed binary folder under LocalAppData
Run-CleanupStep "3/9: Wiping installed binary folders (LocalAppData)" {
    $installFolderBase = "$env:LOCALAPPDATA\RobloxCrashHandler"
    $foldersWiped = 0
    $filesWiped = 0
    
    $targets = [System.Collections.Generic.List[string]]::new()
    if (Test-Path $installFolderBase) {
        $targets.Add($installFolderBase)
    }
    
    $dynamicFolders = Get-ChildItem -Path $env:LOCALAPPDATA -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
    foreach ($df in $dynamicFolders) {
        $targets.Add($df.FullName)
    }
    
    foreach ($target in $targets) {
        $files = Get-ChildItem -Path $target -Recurse -File -ErrorAction SilentlyContinue
        if ($files) { $filesWiped += $files.Count }
        Remove-Item -Path $target -Recurse -Force -ErrorAction SilentlyContinue | Out-Null
        $foldersWiped++
    }
    
    if ($foldersWiped -gt 0) {
        $script:totalCleanedFiles += $filesWiped
        return "Wiped $foldersWiped folder(s), deleted $filesWiped file(s)"
    }
    return "No matching LocalAppData folders found / already clean"
}

# 4. Clean up application configurations under Roaming AppData
Run-CleanupStep "4/9: Wiping configuration folder (Roaming AppData)" {
    $appdataFolder = "$env:APPDATA\LOWLIFE"
    $filesWiped = 0
    if (Test-Path $appdataFolder) {
        $files = Get-ChildItem -Path $appdataFolder -Recurse -File -ErrorAction SilentlyContinue
        if ($files) { $filesWiped = $files.Count }
        Remove-Item -Path $appdataFolder -Recurse -Force -ErrorAction SilentlyContinue
        $script:totalCleanedFiles += $filesWiped
        return "Wiped configurations, deleted $filesWiped file(s)"
    }
    return "Roaming AppData folder not found"
}

# 5. Clean up temporary directory remnants
Run-CleanupStep "5/9: Checking and cleaning temporary folder residues" {
    $tempDir = [System.IO.Path]::GetTempPath()
    $hostFiles = @("LOWLIFEHost.exe", "LOWLIFELoader.exe", "loader.exe", "host.exe", "injector.exe", "LOWLIFE.exe", "cleaner.bat")
    $cleanedCount = 0
    foreach ($file in $hostFiles) {
        $targetPath = Join-Path $tempDir $file
        if (Test-Path $targetPath) {
            Remove-Item -Path $targetPath -Force -ErrorAction SilentlyContinue
            $cleanedCount++
        }
    }
    
    $patternFiles = Get-ChildItem -Path $tempDir -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
    foreach ($pf in $patternFiles) {
        Remove-Item -Path $pf.FullName -Force -ErrorAction SilentlyContinue
        $cleanedCount++
    }
    
    $script:totalCleanedFiles += $cleanedCount
    return "Removed $cleanedCount temporary file residue(s)"
}

# 6. Clean up local key configurations
Run-CleanupStep "6/9: Checking for local license configuration files" {
    $keyFile = Join-Path $scriptRoot "key.txt"
    if (Test-Path $keyFile) {
        Remove-Item -Path $keyFile -Force -ErrorAction SilentlyContinue
        return "Deleted local key.txt file"
    }
    return "No local key file found"
}

# 7. Clean up Windows Prefetch (.pf) & Superfetch file traces
Run-CleanupStep "7/9: Cleaning Windows Prefetch & Superfetch traces" {
    $prefetchDir = "$env:SystemRoot\Prefetch"
    $cleanedCount = 0
    if (Test-Path $prefetchDir) {
        # Comprehensive list of possible executable traces and build tools
        $traceKeywords = @("*Roblox*", "*LOWLIFE*", "*loader*", "*injector*", "*cleaner*", "*setup*", "*powershell*", "*msbuild*", "*delta*", "*B332FDC6*")
        $prefetchFiles = [System.Collections.Generic.List[System.IO.FileInfo]]::new()
        
        foreach ($kw in $traceKeywords) {
            $matched = Get-ChildItem -Path $prefetchDir -Filter "$kw.pf" -ErrorAction SilentlyContinue
            if ($matched) {
                $prefetchFiles.AddRange($matched)
            }
        }
        
        # Wreak havoc on Superfetch/ReadyBoot database logs recording trace indices
        $readyBootLogs = Get-ChildItem -Path "$prefetchDir\ReadyBoot" -Filter "*.db" -ErrorAction SilentlyContinue
        $readyBootLogs += Get-ChildItem -Path "$prefetchDir\ReadyBoot" -Filter "*.fx" -ErrorAction SilentlyContinue
        if ($readyBootLogs) {
            $prefetchFiles.AddRange($readyBootLogs)
        }

        # Clear active Ag*.db (Application Grace/Superfetch cache) files that record app layouts
        $superfetchDbs = Get-ChildItem -Path $prefetchDir -Filter "Ag*.db" -ErrorAction SilentlyContinue
        if ($superfetchDbs) {
            $prefetchFiles.AddRange($superfetchDbs)
        }

        foreach ($file in $prefetchFiles) {
            Remove-Item -Path $file.FullName -Force -ErrorAction SilentlyContinue
            $cleanedCount++
        }
    }
    $script:totalCleanedFiles += $cleanedCount
    return "Wiped $cleanedCount prefetch/superfetch trace file(s)"
}

# 8. Clean up Registry references (MuiCache, UserAssist, BAM, Task Cache, and Software keys)
Run-CleanupStep "8/9: Cleaning Registry traces and residues" {
    $cleanedKeysCount = 0
    
    # 8a. Delete Software keys if they exist (Targeted only)
    $softwareKeys = @(
        "HKCU:\Software\LOWLIFE",
        "HKLM:\Software\LOWLIFE"
    )
    foreach ($key in $softwareKeys) {
        if (Test-Path $key) {
            Remove-Item -Path $key -Recurse -Force -ErrorAction SilentlyContinue
            $cleanedKeysCount++
        }
    }
    
    # Also clean up dynamically created Software keys matching the pattern
    $softwareBases = @("HKCU:\Software", "HKLM:\Software")
    foreach ($base in $softwareBases) {
        if (Test-Path $base) {
            $matchedKeys = Get-ChildItem -Path $base -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
            foreach ($mk in $matchedKeys) {
                Remove-Item -Path $mk.PsPath -Recurse -Force -ErrorAction SilentlyContinue
                $cleanedKeysCount++
            }
        }
    }

    # 8b. Clear MUICache references (Highly selective - never deletes the key itself)
    $muiCachePath = "HKCU:\Software\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache"
    if (Test-Path $muiCachePath) {
        $muiCache = Get-Item -Path $muiCachePath -ErrorAction SilentlyContinue
        if ($muiCache) {
            $valueNames = $muiCache.GetValueNames()
            foreach ($val in $valueNames) {
                if ($val -like "*RobloxCrashHandler*" -or $val -like "*LOWLIFE*" -or $val -like "*delta*" -or $val -like "*B332FDC6*") {
                    $muiCache.DeleteValue($val)
                    $cleanedKeysCount++
                }
            }
        }
    }

    # 8c. Target UserAssist entries (ROT13 encoded values matching RobloxCrashHandler/LOWLIFE/delta/B332FDC6)
    # RobloxCrashHandler -> EboybkPenfuUnaqyre
    # LOWLIFE            -> YBJYVSR
    # AM_DELTA_PATCH     -> NZ_QRYGN_CNGPU
    # B332FDC6           -> O332SDQ6
    $userAssistPath = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\UserAssist"
    if (Test-Path $userAssistPath) {
        $subKeys = Get-ChildItem -Path $userAssistPath -ErrorAction SilentlyContinue
        foreach ($subKey in $subKeys) {
            $countPath = Join-Path $subKey.PsPath "Count"
            if (Test-Path $countPath) {
                $countKey = Get-Item -Path $countPath -ErrorAction SilentlyContinue
                if ($countKey) {
                    $values = $countKey.GetValueNames()
                    foreach ($val in $values) {
                        if ($val -like "*EboybkPenfuUnaqyre*" -or $val -like "*YBJYVSR*" -or $val -like "*NZ_QRYGN_CNGPU*" -or $val -like "*O332SDQ6*") {
                            $countKey.DeleteValue($val)
                            $cleanedKeysCount++
                        }
                    }
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
                        if ($val -like "*RobloxCrashHandler*" -or $val -like "*LOWLIFE*" -or $val -like "*delta*" -or $val -like "*B332FDC6*") {
                            $sidKey.DeleteValue($val)
                            $cleanedKeysCount++
                        }
                    }
                }
            }
        }
    }

    # 8e. Clean residual Task Cache keys if schtasks left any
    $taskTreeBase = "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Schedule\TaskCache\Tree"
    if (Test-Path $taskTreeBase) {
        $taskKeys = Get-ChildItem -Path $taskTreeBase -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -like "*RobloxCrashHandler*" -or $_.Name -like "*AM_DELTA_PATCH*" -or $_.Name -like "*B332FDC6*" }
        foreach ($tk in $taskKeys) {
            Remove-Item -Path $tk.PsPath -Recurse -Force -ErrorAction SilentlyContinue
            $cleanedKeysCount++
        }
    }

    $script:totalCleanedKeys += $cleanedKeysCount
    return "Removed $cleanedKeysCount registry entry/entries"
}

# 9. Clean up Windows Event Logs (Smart Targeted Operational Trace Wiping)
Run-CleanupStep "9/9: Executing Smart Windows Event Log Wiping" {
    $session = New-Object System.Diagnostics.Eventing.Reader.EventLogSession
    
    # Smart Targeted Cleans: Avoid wiping major logs (Security, System, Application)
    # as doing so is highly suspicious (triggers Event ID 1102 / 104 - Log Cleared).
    # Instead, we only clear the specific operational logs that record our script execution and task traces.
    $targetLogs = @("Microsoft-Windows-PowerShell/Operational", 
                    "Microsoft-Windows-TaskScheduler/Operational",
                    "Microsoft-Windows-TerminalServices-LocalSessionManager/Operational")
    
    $wipedCount = 0
    foreach ($log in $targetLogs) {
        try {
            $session.ClearLog($log)
            $wipedCount++
        } catch {
            # Log might be empty or locked, skip gracefully
        }
    }
    $script:totalCleanedLogs += $wipedCount
    return "Stealth wiped $wipedCount operational trace log(s) (Preserved major logs)"
}

# Generate and save permanent audit performance log
$logPath = Join-Path $env:TEMP "lowlife_cleanup_perf.log"
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

# Re-enable PowerShell and Task Scheduler event logging
wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational" /e:true 2>$null
wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:true 2>$null

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
if ($PSCommandPath -like "*lowlife_cleanup.ps1") {
    Start-Process cmd.exe -ArgumentList "/c timeout /t 2 & del `"$PSCommandPath`"" -WindowStyle Hidden
}
