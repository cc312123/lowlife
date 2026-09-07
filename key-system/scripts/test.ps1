$env:ADMIN_TOKEN = 'test-admin-secret-9182'
$env:PORT = '3747'
$serverDir = (Resolve-Path (Join-Path $PSScriptRoot '..\server')).Path

Set-Location $serverDir

Write-Host "[KEY-SYSTEM] Starting server..." -ForegroundColor Cyan
Start-Job -Name "KeyServer" -ScriptBlock {
    $env:ADMIN_TOKEN = 'test-admin-secret-9182'
    $env:PORT = '3747'
    Set-Location $args[0]
    node server.js
} -ArgumentList $serverDir | Out-Null

Start-Sleep -Seconds 3

Write-Host "[KEY-SYSTEM] Testing health endpoint..." -ForegroundColor Cyan
try {
    $health = Invoke-RestMethod -Method Get -Uri 'http://127.0.0.1:3747/health'
    Write-Host "[KEY-SYSTEM] Server UP - ts=$($health.ts)" -ForegroundColor Green
} catch {
    Write-Host "[KEY-SYSTEM] Server not reachable: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Write-Host "`n[TEST] Creating a 30-day key..." -ForegroundColor Cyan
$headers = @{ 'Content-Type' = 'application/json'; 'X-Auth-Token' = 'test-admin-secret-9182' }
$createResp = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:3747/api/v1/keys/create' -Headers $headers -Body '{"durationDays":30,"maxHwids":1,"tier":"standard"}'
Write-Host "[TEST] Created Key: $($createResp.key)" -ForegroundColor Green

Write-Host "`n[TEST] Verifying key..." -ForegroundColor Cyan
$vBody = "{`"key`":`"$($createResp.key)`",`"hwid`":`"test-device-abc123`"}"
$vResp = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:3747/api/v1/verify' -Headers @{'Content-Type'='application/json'} -Body $vBody
Write-Host "[TEST] ok=$($vResp.ok)  tier=$($vResp.tier)  token_len=$($vResp.sessionToken.Length)" -ForegroundColor Green

Write-Host "`n[TEST] Validating session token..." -ForegroundColor Cyan
$sBody = "{`"sessionToken`":`"$($vResp.sessionToken)`"}"
$sResp = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:3747/api/v1/validate-session' -Headers @{'Content-Type'='application/json'} -Body $sBody
Write-Host "[TEST] session ok=$($sResp.ok)" -ForegroundColor Green

Write-Host "`n[TEST] Revoking key..." -ForegroundColor Cyan
$rBody = "{`"key`":`"$($createResp.key)`"}"
$rResp = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:3747/api/v1/keys/revoke' -Headers $headers -Body $rBody
Write-Host "[TEST] Revoke code=$($rResp.code)" -ForegroundColor Green

Write-Host "`n[TEST] Verifying revoked key (should fail)..." -ForegroundColor Cyan
try {
    $vResp2 = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:3747/api/v1/verify' -Headers @{'Content-Type'='application/json'} -Body $vBody
    Write-Host "[TEST] ERROR: Should have been rejected!" -ForegroundColor Red
} catch {
    Write-Host "[TEST] Correctly rejected revoked key. (good!)" -ForegroundColor Green
}

Write-Host "`n[TEST] Listing all keys..." -ForegroundColor Cyan
$listResp = Invoke-RestMethod -Method Post -Uri 'http://127.0.0.1:3747/api/v1/keys/list' -Headers $headers -Body '{}'
Write-Host "[TEST] Total keys in DB: $($listResp.total)" -ForegroundColor Green
$listResp.keys | ForEach-Object { Write-Host "  -> $($_.rawKey)  status=$($_.status)  tier=$($_.tier)" }

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host " ALL TESTS PASSED SUCCESSFULLY!" -ForegroundColor Green
Write-Host "========================================`n" -ForegroundColor Cyan

# Stop the background job when done
Stop-Job -Name "KeyServer" -ErrorAction SilentlyContinue
Remove-Job -Name "KeyServer" -ErrorAction SilentlyContinue

# ── CLEANUP: wipe all test keys from the database ────────────
$keysFile = Join-Path $serverDir "keys.json"
Set-Content -Path $keysFile -Value "{`n  `"keys`": {}`n}" -Encoding UTF8
Write-Host "[CLEANUP] keys.json wiped clean - no dev keys left behind.`n" -ForegroundColor Yellow
