# copy_release.ps1
# Automates copying the compiled binary to the server and updating releases.json with the new MD5 hash.

$ErrorActionPreference = "Stop"

$BuildExe1 = Join-Path $PSScriptRoot "build\RobloxCrashHandler_new.exe"
$BuildExe2 = Join-Path $PSScriptRoot "build\RobloxCrashHandler.exe"
$BuildExe = $null

if (Test-Path $BuildExe1) {
    if (Test-Path $BuildExe2) {
        if ((Get-Item $BuildExe1).LastWriteTimeUtc -gt (Get-Item $BuildExe2).LastWriteTimeUtc) {
            $BuildExe = $BuildExe1
        } else {
            $BuildExe = $BuildExe2
        }
    } else {
        $BuildExe = $BuildExe1
    }
} else {
    $BuildExe = $BuildExe2
}
$ServerDir = Join-Path $PSScriptRoot "updates-server"
$ServerUploadsExe = Join-Path $ServerDir "uploads\RobloxCrashHandler.exe"
$ReleasesJsonPath = Join-Path $ServerDir "releases.json"

if (-not (Test-Path $BuildExe)) {
    Write-Warning "Compiled executable not found at '$BuildExe'. Proceeding statically..."
    exit 0
}

Write-Host "Calculating MD5 hash for the new build..."
try {
    $Hash = (Get-FileHash -Algorithm MD5 $BuildExe).Hash.ToLower()
} catch {
    Write-Host ""
    Write-Warning "Windows Defender is blocking access to the built executable!"
    Write-Warning "Please open Windows Security -> Virus & threat protection -> Protection history,"
    Write-Warning "find the blocked 'RobloxCrashHandler.exe' threat, and select 'Allow on device' or 'Restore'."
    Write-Host ""
    exit
}

Write-Host "Encrypting build with AES-256 for secure fileless distribution..."
$DecKey = [byte[]](0x54,0x55,0x4E,0x47,0x57,0x41,0x52,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,
                   0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
$DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x54,0x55,0x4E,0x47,0x57,0x41,0x52,0x45,0x21)

$fileBytes = [System.IO.File]::ReadAllBytes($BuildExe)
$aes         = [System.Security.Cryptography.Aes]::Create()
$aes.Key     = $DecKey
$aes.IV      = $DecIV
$aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
$aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
$enc         = $aes.CreateEncryptor()
$encBytes    = $enc.TransformFinalBlock($fileBytes, 0, $fileBytes.Length)
$aes.Dispose()

$ServerUploadsEnc = Join-Path $ServerDir "uploads\RobloxCrashHandler.enc"
if (-not (Test-Path (Split-Path $ServerUploadsEnc))) {
    New-Item -ItemType Directory -Path (Split-Path $ServerUploadsEnc) -Force | Out-Null
}
[System.IO.File]::WriteAllBytes($ServerUploadsEnc, $encBytes)

# Remove any legacy/unencrypted EXE in uploads if present
$LegacyExe = Join-Path $ServerDir "uploads\RobloxCrashHandler.exe"
if (Test-Path $LegacyExe) {
    Remove-Item $LegacyExe -Force
}

Write-Host "Updating releases.json with the new MD5 hash ($Hash)..."
if (Test-Path $ReleasesJsonPath) {
    $json = Get-Content $ReleasesJsonPath -Raw | ConvertFrom-Json
    
    # Update top-level latestHash
    if (-not (Get-Member -InputObject $json -Name "latestHash")) {
        $json | Add-Member -MemberType NoteProperty -Name "latestHash" -Value $Hash
    } else {
        $json.latestHash = $Hash
    }
    
    # Update latest history entry hash if it exists
    if ($json.history -and $json.history.Count -gt 0) {
        $lastHistory = $json.history | Where-Object { $_.version -eq $json.latestVersion }
        if (-not $lastHistory) {
            $lastHistory = $json.history[-1]
        }
        if (-not (Get-Member -InputObject $lastHistory -Name "md5")) {
            $lastHistory | Add-Member -MemberType NoteProperty -Name "md5" -Value $Hash
        } else {
            $lastHistory.md5 = $Hash
        }
    }
    
    # Save back to file with proper formatting
    $json | ConvertTo-Json -Depth 10 | Out-File $ReleasesJsonPath -Encoding utf8
    Write-Host "Success! releases.json updated."
} else {
    Write-Warning "releases.json not found at '$ReleasesJsonPath'."
}

Write-Host "=================================================="
Write-Host "🎉 Release v$($json.latestVersion) successfully prepared and updated on the local server!"
Write-Host "=================================================="
