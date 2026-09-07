Set objShell = CreateObject("WScript.Shell")
Set objFSO = CreateObject("Scripting.FileSystemObject")
strScriptPath = objFSO.GetParentFolderName(WScript.ScriptFullName)

strExePath = ""
arrPaths = Array( _
    strScriptPath & "\build\RobloxCrashHandler.exe", _
    strScriptPath & "\my private\build\RobloxCrashHandler.exe", _
    strScriptPath & "\build\RobloxCrashHandler_fallback.exe", _
    strScriptPath & "\my private\build\RobloxCrashHandler_fallback.exe", _
    strScriptPath & "\updates-server\uploads\RobloxCrashHandler.exe", _
    strScriptPath & "\my private\updates-server\uploads\RobloxCrashHandler.exe", _
    objShell.ExpandEnvironmentStrings("%TEMP%") & "\RobloxCrashHandler_fallback.exe" _
)

For Each path In arrPaths
    If objFSO.FileExists(path) Then
        strExePath = path
        Exit For
    End If
Next

If strExePath <> "" Then
    objShell.Run """" & strExePath & """", 0, False
    WScript.Sleep 2000
    objShell.Run "http://127.0.0.1:9876"
End If
