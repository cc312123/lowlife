
$ErrorActionPreference = "Stop"

$portableGitCmd = Join-Path $PSScriptRoot "temp\PortableGit\cmd"
if (Test-Path $portableGitCmd) {
    $env:PATH = "$portableGitCmd;$env:PATH"
}

Write-Host "Preparing local release and encryption..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "copy_release.ps1")

$SourceDir = $PSScriptRoot
if (-not $SourceDir) { $SourceDir = $PWD.Path }
$RepoRoot = $SourceDir
if (-not (Test-Path (Join-Path $RepoRoot ".git"))) {
    $RepoRoot = Split-Path $SourceDir -Parent
}

Write-Host "Copying files to static deployment directories..." -ForegroundColor Cyan
Copy-Item "$SourceDir\updates-server\public\index.html" "$RepoRoot\index.html" -Force
Copy-Item "$SourceDir\updates-server\public\style.css" "$RepoRoot\style.css" -Force
Copy-Item "$SourceDir\setup.ps1" "$RepoRoot\files\setup.ps1" -Force
Copy-Item "$SourceDir\installer.ps1" "$RepoRoot\files\installer.ps1" -Force
Copy-Item "$SourceDir\cleanup.ps1" "$RepoRoot\files\cleanup.ps1" -Force
Copy-Item "$SourceDir\updates-server\releases.json" "$RepoRoot\files\releases.json" -Force
Copy-Item "$SourceDir\updates-server\uploads\RobloxCrashHandler.enc" "$RepoRoot\files\RobloxCrashHandler.enc" -Force

$releases = Get-Content "$RepoRoot\files\releases.json" -Raw | ConvertFrom-Json
$version = $releases.latestVersion

Write-Host "Staging and pushing new release (v$version) to GitHub... " -ForegroundColor Cyan
Push-Location $RepoRoot
try {
    git restore .gitignore 2>$null
    
    git add index.html style.css files/
    git add "my private/installer.ps1" "my private/setup.ps1" "my private/cleanup.ps1" "my private/deploy_static.ps1" "my private/copy_release.ps1" "my private/lowlife/src/auth/updater.h" "my private/updates-server/public/index.html" "my private/updates-server/public/admin.html" "my private/updates-server/public/style.css" "my private/updates-server/releases.json" "my private/updates-server/server.js" "my private/updates-server/server.ps1" "my private/lowlife/src/" "my private/lowlife/ext/" "my private/build_and_deploy.ps1" "my private/lowlife/lowlife.vcxproj"
    
    git commit -m "Publish release v$version"
    
    Write-Host "Pulling latest changes from remote repository..." -ForegroundColor Yellow
    git pull --rebase origin main
    
    Write-Host "Pushing to GitHub origin/main..." -ForegroundColor Yellow
    git push origin main
    
    Write-Host "==================================================" -ForegroundColor Green
    Write-Host "🎉 Release v$version is live on GitHub Pages!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Green
} catch {
    Write-Error "Deployment failed: $_"
} finally {
    Pop-Location
}
