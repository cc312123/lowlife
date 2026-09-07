$currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$KillTaskPath = Join-Path $PSScriptRoot "kill_processes.ps1"
$action = New-ScheduledTaskAction -Execute 'powershell.exe' -Argument "-WindowStyle Hidden -NoProfile -ExecutionPolicy Bypass -File `"$KillTaskPath`""
$principal = New-ScheduledTaskPrincipal -UserId $currentUser -RunLevel Highest -LogonType Interactive
Register-ScheduledTask -TaskName 'KillLoaderTask' -Action $action -Principal $principal -Force | Out-Null
Start-ScheduledTask -TaskName 'KillLoaderTask'
Start-Sleep -Seconds 3
Unregister-ScheduledTask -TaskName 'KillLoaderTask' -Confirm:$false | Out-Null
