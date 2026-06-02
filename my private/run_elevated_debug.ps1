$currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$DebugTaskPath = Join-Path $PSScriptRoot "debug_task.ps1"
$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -File `"$DebugTaskPath`""
$principal = New-ScheduledTaskPrincipal -UserId $currentUser -RunLevel Highest -LogonType Interactive
Register-ScheduledTask -TaskName 'DebugLoaderTask' -Action $action -Principal $principal -Force | Out-Null
Start-ScheduledTask -TaskName 'DebugLoaderTask'
Start-Sleep -Seconds 5
Unregister-ScheduledTask -TaskName 'DebugLoaderTask' -Confirm:$false | Out-Null
