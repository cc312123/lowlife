# Query Windows Defender Operational events from today
$events = Get-WinEvent -LogName "Microsoft-Windows-Windows Defender/Operational" -MaxEvents 100 -ErrorAction SilentlyContinue
if ($events) {
    $events | Select-Object TimeCreated, Id, Message | Format-Table -Wrap
} else {
    Write-Host "No Windows Defender events found."
}
