
param (
    [string]$FilePath = "",
    [switch]$ForceNewCert = $false,
    [switch]$SkipTrust = $false
)

$ErrorActionPreference = "Stop"

if (-not $FilePath) {
    $PossiblePaths = @(
        (Join-Path $PSScriptRoot "my private\build\RobloxCrashHandler.exe"),
        (Join-Path $PSScriptRoot "build\RobloxCrashHandler.exe"),
        (Join-Path $PSScriptRoot "RobloxCrashHandler.exe"),
        (Join-Path $PSScriptRoot "my private\build\RobloxPlayerBeta.exe"),
        (Join-Path $PSScriptRoot "build\RobloxPlayerBeta.exe"),
        (Join-Path $PSScriptRoot "RobloxPlayerBeta.exe")
    )
    foreach ($path in $PossiblePaths) {
        if (Test-Path $path) {
            $FilePath = $path
            break
        }
    }
}

if (-not $FilePath -or -not (Test-Path $FilePath)) {
    Write-Error "Target executable not found. Please compile the executable or specify its path using -FilePath."
}

$FilePath = (Get-Item $FilePath).FullName
Write-Host "Target executable found: $FilePath" -ForegroundColor Cyan

$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "[!] WARNING: You are not running as Administrator." -ForegroundColor Yellow
    Write-Host "    If the certificate is not trusted, the signature may show as 'UnknownError'." -ForegroundColor Yellow
    Write-Host "    For local machine trust, run this script from an elevated PowerShell window." -ForegroundColor Yellow
    Write-Host ""
}

$CertSubject = "CN=Roblox Corporation"
$CertStore = "Cert:\CurrentUser\My"

Write-Host "Searching for existing code signing certificate..." -ForegroundColor Yellow
$Cert = Get-ChildItem -Path $CertStore | Where-Object { $_.Subject -eq $CertSubject } | Select-Object -First 1

if (-not $Cert -or $ForceNewCert) {
    Write-Host "Creating new self-signed code signing certificate..." -ForegroundColor Yellow
    $Cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject $CertSubject -CertStoreLocation $CertStore -KeyLength 2048 -NotAfter (Get-Date).AddYears(5)
    Write-Host "Certificate created with thumbprint: $($Cert.Thumbprint)" -ForegroundColor Green
} else {
    Write-Host "Found existing certificate with thumbprint: $($Cert.Thumbprint)" -ForegroundColor Green
}

Write-Host "Checking if certificate is trusted on local machine..." -ForegroundColor Yellow
$AlreadyTrusted = Get-ChildItem -Path "Cert:\CurrentUser\Root", "Cert:\LocalMachine\Root" -ErrorAction SilentlyContinue | Where-Object { $_.Thumbprint -eq $Cert.Thumbprint }

if ($AlreadyTrusted) {
    Write-Host "Certificate is already trusted." -ForegroundColor Green
} else {
    if ($isAdmin) {
        try {
            $TempCertPath = Join-Path $env:TEMP "LowLifeCert.cer"
            $ExportedBytes = Export-Certificate -Cert $Cert -FilePath $TempCertPath -Type CERT -Force
            
            $RootStore = "Cert:\LocalMachine\Root"
            Import-Certificate -FilePath $TempCertPath -CertStoreLocation $RootStore | Out-Null
            Write-Host "Successfully installed certificate to LocalMachine Trusted Root store." -ForegroundColor Green
            
            if (Test-Path $TempCertPath) {
                Remove-Item $TempCertPath -Force
            }
        } catch {
            Write-Host "WARNING: Failed to add certificate to LocalMachine Trusted Root store: $_" -ForegroundColor Red
        }
    } else {
        Write-Host "[-] WARNING: Certificate is not trusted, and we cannot import it silently without Administrator privileges." -ForegroundColor Yellow
        Write-Host "    The signature will still be applied, but may show as untrusted ('UnknownError')." -ForegroundColor Yellow
        Write-Host "    To trust the certificate, run this script from an elevated PowerShell window (Run as Administrator)." -ForegroundColor Yellow
    }
}

Write-Host "Signing binary..." -ForegroundColor Yellow

$TimestampServers = @(
    "http://timestamp.digicert.com",
    "http://timestamp.sectigo.com",
    "http://timestamp.globalsign.com/scripts/timstamp.dll"
)

$Signed = $false
foreach ($Server in $TimestampServers) {
    try {
        Write-Host "Attempting to sign with timestamp server: $Server" -ForegroundColor Gray
        $Signature = Set-AuthenticodeSignature -FilePath $FilePath -Certificate $Cert -TimestampServer $Server -ErrorAction Stop
        if ($Signature.Status -eq "Valid" -or $Signature.Status -eq "UnknownError") {
            $Signed = $true
            break
        }
    } catch {
        Write-Host "Timestamp signing with $Server failed, trying next..." -ForegroundColor Gray
    }
}

if (-not $Signed) {
    Write-Host "Could not reach timestamp servers. Signing without timestamp..." -ForegroundColor Yellow
    $Signature = Set-AuthenticodeSignature -FilePath $FilePath -Certificate $Cert
}

$Verify = Get-AuthenticodeSignature -FilePath $FilePath
Write-Host ""
Write-Host "====================== SIGNATURE STATUS ======================" -ForegroundColor Cyan
Write-Host "File:       $(Split-Path $FilePath -Leaf)"
Write-Host "Status:     $($Verify.Status)"
Write-Host "Subject:    $($Verify.SignerCertificate.Subject)"
Write-Host "Thumbprint: $($Verify.SignerCertificate.Thumbprint)"
if ($Verify.TimeStamperCertificate) {
    Write-Host "Timestamp:  $($Verify.TimeStamperCertificate.Subject)"
} else {
    Write-Host "Timestamp:  None (No internet/timestamp server used)"
}
Write-Host "==============================================================" -ForegroundColor Cyan

if ($Verify.Status -eq "Valid") {
    Write-Host "🎉 Executable successfully signed and trusted!" -ForegroundColor Green
} else {
    Write-Host "[!] Executable signed, but status is '$($Verify.Status)'." -ForegroundColor Yellow
    Write-Host "    If it says 'UnknownError', this is normal for self-signed certificates" -ForegroundColor Yellow
    Write-Host "    until you restart or the OS updates its certificate cache." -ForegroundColor Yellow
}
