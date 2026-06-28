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

function Obfuscate-HtmlFile($srcPath, $outPath) {
    if (Test-Path $srcPath) {
        $content = [System.IO.File]::ReadAllText($srcPath)
        
        $pattern = "(?s)<script\b[^>]*>([\s\S]*?)<\/script>"
        $matches = [regex]::Matches($content, $pattern)
        
        foreach ($match in $matches) {
            $jsCode = $match.Groups[1].Value.Trim()
            if ([string]::IsNullOrWhiteSpace($jsCode)) {
                continue
            }
            
            $base64 = Encrypt-JS $jsCode $XorKey
            
            $oldBlock = $match.Value
            $newBlock = "<script>eval((function(){const k=`"$XorKey`",s=`"$base64`",b=atob(s);let r=`"`";for(let i=0;i<b.length;i++)r+=String.fromCharCode(b.charCodeAt(i)^k.charCodeAt(i%k.length));return r;})());</script>"
            $content = $content.Replace($oldBlock, $newBlock)
        }
        
        [System.IO.File]::WriteAllText($outPath, $content)
        Write-Host "XOR-encrypted script tags: $srcPath -> $outPath" -ForegroundColor Green
    } else {
        Write-Warning "Source file not found: $srcPath"
    }
}

Obfuscate-HtmlFile $indexSrc $indexOut
Obfuscate-HtmlFile $adminSrc $adminOut
