$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$portableGitCmd = Join-Path $PSScriptRoot "temp\PortableGit\cmd"
if (Test-Path $portableGitCmd) {
    $env:PATH = "$portableGitCmd;$env:PATH"
}

Write-Host "Stopping running loader and game processes..." -ForegroundColor Cyan
Stop-Process -Name "RobloxPlayerBeta" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "RobloxCrashHandler" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "mspdbsrv" -Force -ErrorAction SilentlyContinue
Stop-Process -Name "vctip" -Force -ErrorAction SilentlyContinue

$listening = Get-NetTCPConnection -LocalPort 9876 -State Listen -ErrorAction SilentlyContinue
if ($listening) {
    Write-Host "Stopping loader process listening on port 9876 (PID $($listening.OwningProcess))...." -ForegroundColor Yellow
    Stop-Process -Id $listening.OwningProcess -Force -ErrorAction SilentlyContinue
}

$localTemp = Join-Path $PSScriptRoot "temp"
if (-not (Test-Path $localTemp)) {
    New-Item -ItemType Directory -Path $localTemp -Force | Out-Null
}
$env:TEMP = $localTemp
$env:TMP = $localTemp

$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue
if (-not $msbuild) {
    $vsPath = 'C:\Program Files (x86)\Microsoft Visual Studio'
    if (-not (Test-Path $vsPath)) {
        $vsPath = 'C:\Program Files\Microsoft Visual Studio'
    }
    if (Test-Path $vsPath) {
        $msbuildPath = Get-ChildItem -Path $vsPath -Filter 'MSBuild.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
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

Write-Host "Cleaning solution..." -ForegroundColor Cyan
& $msbuild tung-ware.sln /t:Clean /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:TrackFileAccess=false

Write-Host "Building solution..." -ForegroundColor Cyan
& $msbuild tung-ware.sln /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v143 /p:TrackFileAccess=false

$BuildExe = Join-Path $PSScriptRoot "build\RobloxCrashHandler.exe"

if (-not (Test-Path $BuildExe)) {
    Write-Warning "Build failed or binary is locked. Proceeding with static web deployments..."
} else {
    Write-Host "Build succeeded! Binary found at $BuildExe" -ForegroundColor Green
    Write-Host "Signing built binary..." -ForegroundColor Cyan
    try {
        & (Join-Path $PSScriptRoot "..\sign_binary.ps1") -FilePath $BuildExe
    } catch {
        Write-Warning "Signing failed or process is locked. Proceeding..."
    }
}

$ReleasesJsonPath = Join-Path $PSScriptRoot "updates-server\releases.json"
$newVersion = "1.0.52"
$changelogText = "DB No Spread: clamp Handle position to <= 2.23 studs from HRP origin so distance-to-barrel always reads minimal, and enforce 1 pellet per shot for zero spread."

if (Test-Path $ReleasesJsonPath) {
    $json = Get-Content $ReleasesJsonPath -Raw | ConvertFrom-Json
    
    $json.latestVersion = $newVersion
    $json.latestChangelog = $changelogText
    $json.fileName = "RobloxCrashHandler.exe"
    
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

Write-Host "Running obfuscate_scripts.ps1 to encrypt frontend script tags..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "updates-server\obfuscate_scripts.ps1")

Write-Host "Running copy_release.ps1..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "copy_release.ps1")

Write-Host "Copying files to static deployment directories..." -ForegroundColor Cyan
Copy-Item "$PSScriptRoot\updates-server\public\index.html" "$PSScriptRoot\..\index.html" -Force
Copy-Item "$PSScriptRoot\updates-server\public\style.css" "$PSScriptRoot\..\style.css" -Force
Copy-Item "$PSScriptRoot\setup.ps1" "$PSScriptRoot\..\files\setup.ps1" -Force
Copy-Item "$PSScriptRoot\installer.ps1" "$PSScriptRoot\..\files\installer.ps1" -Force
Copy-Item "$PSScriptRoot\cleanup.ps1" "$PSScriptRoot\..\files\cleanup.ps1" -Force
Copy-Item "$PSScriptRoot\setup.ps1" "$PSScriptRoot\..\setup.ps1" -Force
Copy-Item "$PSScriptRoot\installer.ps1" "$PSScriptRoot\..\installer.ps1" -Force
Copy-Item "$PSScriptRoot\cleanup.ps1" "$PSScriptRoot\..\cleanup.ps1" -Force
Copy-Item "$PSScriptRoot\updates-server\releases.json" "$PSScriptRoot\..\files\releases.json" -Force
Copy-Item "$PSScriptRoot\updates-server\uploads\RobloxCrashHandler.enc" "$PSScriptRoot\..\files\RobloxCrashHandler.enc" -Force

$RepoRoot = Resolve-Path "$PSScriptRoot\.."
Write-Host "Staging and pushing new release (v$newVersion) to GitHub... " -ForegroundColor Cyan
Push-Location $RepoRoot.Path
try {
    git restore .gitignore 2>$null
    
    git add index.html style.css files/
    git add "installer.ps1" "setup.ps1" "cleanup.ps1" "my private/installer.ps1" "my private/setup.ps1" "my private/cleanup.ps1" "my private/deploy_static.ps1" "my private/copy_release.ps1" "my private/tung-ware/src/auth/updater.h" "my private/updates-server/public/index.html" "my private/updates-server/public/admin.html" "my private/updates-server/public/index.src.html" "my private/updates-server/public/admin.src.html" "my private/updates-server/obfuscate_scripts.ps1" "my private/updates-server/public/style.css" "my private/updates-server/releases.json" "my private/updates-server/server.js" "my private/updates-server/server.ps1" "my private/tung-ware/src/" "my private/tung-ware/ext/" "my private/build_and_deploy.ps1"
    
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
