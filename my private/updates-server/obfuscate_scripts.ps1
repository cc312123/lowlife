$scriptDir = Split-Path $MyInvocation.MyCommand.Path -Parent
$indexSrc = Join-Path $scriptDir "public\index.src.html"
$adminSrc = Join-Path $scriptDir "public\admin.src.html"
$indexOut = Join-Path $scriptDir "public\index.html"
$adminOut = Join-Path $scriptDir "public\admin.html"

$XorKey = "TUNG_WARE_SECURE_KEY_2026"

function Encrypt-JS($jsCode, $key) {
    $jsBytes = [System.Text.Encoding]::UTF8.GetBytes($jsCode)
    $keyBytes = [System.Text.Encoding]::UTF8.GetBytes($key)
    $xorBytes = New-Object byte[] $jsBytes.Length
    for ($i = 0; $i -lt $jsBytes.Length; $i++) {
        $xorBytes[$i] = $jsBytes[$i] -bxor $keyBytes[$i % $keyBytes.Length]
    }
    return [Convert]::ToBase64String($xorBytes)
}

# Clean HTML build script without web script injection / eval obfuscation
function Process-HtmlFile($srcPath, $outPath) {
    if (Test-Path $srcPath) {
        Copy-Item -Path $srcPath -Destination $outPath -Force
        Write-Host "Clean HTML output generated (web injection removed): $srcPath -> $outPath" -ForegroundColor Green
    } else {
        Write-Warning "Source file not found: $srcPath"
    }
}

Process-HtmlFile $indexSrc $indexOut
Process-HtmlFile $adminSrc $adminOut

