@echo off
echo ==================================================
echo Requesting administrator privileges to install ATL...
echo ==================================================
powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process powershell -ArgumentList '-NoProfile -ExecutionPolicy Bypass -Command \"Write-Host ''Installing C++ ATL Component...'' -ForegroundColor Cyan; Start-Process ''C:\Program Files (x86)\Microsoft Visual Studio\Installer\vs_installer.exe'' -ArgumentList ''modify --installPath \\\"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\\\" --add Microsoft.VisualStudio.Component.VC.ATL --passive --norestart'' -Wait -NoNewWindow; Write-Host ''ATL installation complete!'' -ForegroundColor Green; Start-Sleep -Seconds 3\"' -Verb RunAs"
