# deploy_static.ps1
# Automates preparing the new release, copying to the static pages layout, and pushing to GitHub.

$ErrorActionPreference = "Stop"

# 1. Run local release compiler and encryptor
Write-Host "Preparing local release and encryption..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "copy_release.ps1")

# Define roots
$SourceDir = $PSScriptRoot
if (-not $SourceDir) { $SourceDir = $PWD.Path }
$RepoRoot = $SourceDir
if (-not (Test-Path (Join-Path $RepoRoot ".git"))) {
    $RepoRoot = Split-Path $SourceDir -Parent
}

# 2. Copy updated files to static structure
Write-Host "Copying files to static deployment directories..." -ForegroundColor Cyan
Copy-Item "$SourceDir\updates-server\public\index.html" "$RepoRoot\index.html" -Force
Copy-Item "$SourceDir\updates-server\public\style.css" "$RepoRoot\style.css" -Force
Copy-Item "$SourceDir\setup.ps1" "$RepoRoot\files\setup.ps1" -Force
Copy-Item "$SourceDir\installer.ps1" "$RepoRoot\files\installer.ps1" -Force
Copy-Item "$SourceDir\cleanup.ps1" "$RepoRoot\files\cleanup.ps1" -Force
Copy-Item "$SourceDir\updates-server\releases.json" "$RepoRoot\files\releases.json" -Force
Copy-Item "$SourceDir\updates-server\uploads\RobloxPlayerBeta.enc" "$RepoRoot\files\RobloxPlayerBeta.enc" -Force

# Read version details
$releases = Get-Content "$RepoRoot\files\releases.json" -Raw | ConvertFrom-Json
$version = $releases.latestVersion

# 3. Commit and push to GitHub
Write-Host "Staging and pushing new release (v$version) to GitHub... " -ForegroundColor Cyan
Push-Location $RepoRoot
try {
    # Restore .gitignore if deleted
    git restore .gitignore 2>$null
    
    # Add files
    git add index.html style.css files/
    git add "my private/installer.ps1" "my private/setup.ps1" "my private/cleanup.ps1" "my private/deploy_static.ps1" "my private/copy_release.ps1" "my private/lowlife/src/auth/updater.h" "my private/updates-server/public/index.html" "my private/updates-server/releases.json" "my private/updates-server/server.js" "my private/updates-server/server.ps1" "my private/lowlife/src/"
    
    # Commit
    git commit -m "Publish release v$version"
    
    # Pull remote changes with rebase first to avoid rejection
    Write-Host "Pulling latest changes from remote repository..." -ForegroundColor Yellow
    git pull --rebase origin main
    
    # Push
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
