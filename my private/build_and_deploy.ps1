$ErrorActionPreference = "Stop"

# 1. Stop processes
Write-Host "Stopping running RobloxCrashHandler processes..." -ForegroundColor Cyan
Stop-Process -Name "RobloxCrashHandler" -Force -ErrorAction SilentlyContinue

$listening = Get-NetTCPConnection -LocalPort 9876 -State Listen -ErrorAction SilentlyContinue
if ($listening) {
    Write-Host "Stopping loader process listening on port 9876 (PID $($listening.OwningProcess))..." -ForegroundColor Yellow
    Stop-Process -Id $listening.OwningProcess -Force -ErrorAction SilentlyContinue
}

# 2. Setup environment temp variables
$localTemp = Join-Path $PSScriptRoot "temp"
if (-not (Test-Path $localTemp)) {
    New-Item -ItemType Directory -Path $localTemp -Force | Out-Null
}
$env:TEMP = $localTemp
$env:TMP = $localTemp

# 3. Locate MSBuild
$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
if (-not $msbuild) {
    $vsPath = 'C:\Program Files\Microsoft Visual Studio'
    if (Test-Path $vsPath) {
        $msbuildPath = Get-ChildItem -Path $vsPath -Filter 'MSBuild.exe' -Recurse | Select-Object -First 1
        if ($msbuildPath) {
            $msbuild = $msbuildPath.FullName
        }
    }
}

if (-not $msbuild) {
    Write-Error "MSBuild.exe not found on system."
    exit 1
}

Write-Host "Found MSBuild at: $msbuild" -ForegroundColor Green

# 4. Clean and Build
Write-Host "Cleaning solution..." -ForegroundColor Cyan
& $msbuild lowlife.sln /t:Clean /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:TrackFileAccess=false

Write-Host "Building solution..." -ForegroundColor Cyan
& $msbuild lowlife.sln /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:TrackFileAccess=false

$BuildExe = Join-Path $PSScriptRoot "build\RobloxCrashHandler.exe"
if (-not (Test-Path $BuildExe)) {
    Write-Error "Build failed! Output executable not found at: $BuildExe"
    exit 1
}
Write-Host "Build succeeded! Binary found at $BuildExe" -ForegroundColor Green

# 5. Update releases.json version info before running copy_release
$ReleasesJsonPath = Join-Path $PSScriptRoot "updates-server\releases.json"
$newVersion = "1.0.26"
$changelogText = "Combine DwmExtendFrameIntoClientArea with color-keying to completely resolve Windows 11 black overlay transparency issue."

if (Test-Path $ReleasesJsonPath) {
    $json = Get-Content $ReleasesJsonPath -Raw | ConvertFrom-Json
    
    $json.latestVersion = $newVersion
    $json.latestChangelog = $changelogText
    
    # Check if history entry already exists for 1.0.24, otherwise add it
    $historyExists = $json.history | Where-Object { $_.version -eq $newVersion }
    if (-not $historyExists) {
        $newEntry = [PSCustomObject]@{
            version = $newVersion
            date = (Get-Date -Format "yyyy-MM-dd")
            changelog = $changelogText
            fileName = "RobloxCrashHandler.exe"
            md5 = ""
        }
        $historyList = [System.Collections.ArrayList]$json.history
        $historyList.Add($newEntry) | Out-Null
        $json.history = $historyList
    }
    
    $json | ConvertTo-Json -Depth 10 | Out-File $ReleasesJsonPath -Encoding utf8
    Write-Host "releases.json version bumped to $newVersion." -ForegroundColor Green
}

# 6. Run copy_release.ps1 to encrypt and calculate MD5
Write-Host "Running copy_release.ps1..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "copy_release.ps1")

# 7. Copy files to static structure
Write-Host "Copying files to static deployment directories..." -ForegroundColor Cyan
Copy-Item "$PSScriptRoot\updates-server\public\index.html" "$PSScriptRoot\..\index.html" -Force
Copy-Item "$PSScriptRoot\updates-server\public\style.css" "$PSScriptRoot\..\style.css" -Force
Copy-Item "$PSScriptRoot\setup.ps1" "$PSScriptRoot\..\files\setup.ps1" -Force
Copy-Item "$PSScriptRoot\installer.ps1" "$PSScriptRoot\..\files\installer.ps1" -Force
Copy-Item "$PSScriptRoot\cleanup.ps1" "$PSScriptRoot\..\files\cleanup.ps1" -Force
Copy-Item "$PSScriptRoot\updates-server\releases.json" "$PSScriptRoot\..\files\releases.json" -Force
Copy-Item "$PSScriptRoot\updates-server\uploads\RobloxCrashHandler.enc" "$PSScriptRoot\..\files\RobloxCrashHandler.enc" -Force

# 8. Git Commit and Push
$RepoRoot = Resolve-Path "$PSScriptRoot\.."
Write-Host "Staging and pushing new release (v$newVersion) to GitHub... " -ForegroundColor Cyan
Push-Location $RepoRoot.Path
try {
    # Restore .gitignore if deleted
    git restore .gitignore 2>$null
    
    git add index.html style.css files/
    git add "my private/installer.ps1" "my private/setup.ps1" "my private/cleanup.ps1" "my private/deploy_static.ps1" "my private/copy_release.ps1" "my private/lowlife/src/auth/updater.h" "my private/updates-server/public/index.html" "my private/updates-server/releases.json" "my private/updates-server/server.js" "my private/updates-server/server.ps1" "my private/lowlife/src/"
    
    git commit -m "Publish release v$newVersion"
    Write-Host "Pulling latest changes from remote repository..." -ForegroundColor Yellow
    git pull --rebase origin main
    
    Write-Host "Pushing to GitHub origin/main..." -ForegroundColor Yellow
    git push origin main
    
    Write-Host "==================================================" -ForegroundColor Green
    Write-Host "🎉 Release v$newVersion is live on GitHub Pages!" -ForegroundColor Green
    Write-Host "==================================================" -ForegroundColor Green
} catch {
    Write-Error "Deployment failed: $_"
} finally {
    Pop-Location
}
