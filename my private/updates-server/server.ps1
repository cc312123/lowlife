
$Port = 3000
$scriptRoot = if ($PSScriptRoot) { $PSScriptRoot } elseif ($PWD -and $PWD.Path) { $PWD.Path } else { (Get-Location).Path }
$PublicDir = Join-Path $scriptRoot "public"
$UploadsDir = Join-Path $scriptRoot "uploads"
$ReleasesFile = Join-Path $scriptRoot "releases.json"
$CleanupFile = Join-Path $scriptRoot "..\cleanup.ps1"

if (-not (Test-Path $UploadsDir)) {
    New-Item -ItemType Directory -Path $UploadsDir -Force | Out-Null
}

$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:$Port/")

try {
    $listener.Start()
    Write-Host "==================================================" -ForegroundColor Green
    Write-Host "[*] LowLife PowerShell Server running on Port $Port" -ForegroundColor Green
    Write-Host "[-] Developer Portal: http://localhost:$Port/admin.html" -ForegroundColor Cyan
    Write-Host "[-] User Landing:      http://localhost:$Port/index.html" -ForegroundColor Cyan
    Write-Host "==================================================" -ForegroundColor Green
} catch {
    Write-Host "[-] ERROR: Failed to start listener. Is Port $Port already in use?" -ForegroundColor Red
    Exit
}

function Send-TextResponse($response, $content, $statusCode = 200, $contentType = "text/html") {
    $buffer = [System.Text.Encoding]::UTF8.GetBytes($content)
    $response.StatusCode = $statusCode
    $response.ContentType = "$contentType; charset=utf-8"
    $response.ContentLength64 = $buffer.Length
    $response.OutputStream.Write($buffer, 0, $buffer.Length)
    $response.OutputStream.Close()
}

function Send-FileResponse($response, $filePath, $contentType = "application/octet-stream", $asAttachment = $false, $attachmentName = "") {
    if (Test-Path $filePath) {
        $fileBytes = [System.IO.File]::ReadAllBytes($filePath)
        $response.StatusCode = 200
        $response.ContentType = $contentType
        if ($asAttachment) {
            $response.AddHeader("Content-Disposition", "attachment; filename=`"$attachmentName`"")
        }
        $response.ContentLength64 = $fileBytes.Length
        $response.OutputStream.Write($fileBytes, 0, $fileBytes.Length)
        $response.OutputStream.Close()
    } else {
        Send-TextResponse $response "404 Not Found" 404 "text/plain"
    }
}

while ($listener.IsListening) {
    try {
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response
        $url = $request.RawUrl

        $response.AddHeader("Access-Control-Allow-Origin", "*")
        $response.AddHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        $response.AddHeader("Access-Control-Allow-Headers", "Content-Type")

        if ($request.HttpMethod -eq "OPTIONS") {
            $response.StatusCode = 200
            $response.OutputStream.Close()
            continue
        }

        if ($url -eq "/" -or $url.StartsWith("/index.html")) {
            Send-FileResponse $response (Join-Path $PublicDir "index.html") "text/html"
        }
        elseif ($url.StartsWith("/admin.html")) {
            Send-FileResponse $response (Join-Path $PublicDir "admin.html") "text/html"
        }
        elseif ($url.StartsWith("/style.css")) {
            Send-FileResponse $response (Join-Path $PublicDir "style.css") "text/css"
        }
        elseif ($url.StartsWith("/cleanup")) {
            Send-FileResponse $response $CleanupFile "application/octet-stream" $true "cleanup.ps1"
        }
        elseif ($url.StartsWith("/download")) {
            $userAgent = $request.UserAgent
            if ($userAgent -and $userAgent.Contains("LOWLIFE-SelfUpdater")) {
                $response.Redirect("/api/release/download-binary")
                $response.Close()
                continue
            }

            $setupPath = Join-Path $scriptRoot "..\setup.ps1"
            if (Test-Path $setupPath) {
                $content = Get-Content $setupPath -Raw
                $hostHeader = $request.UserHostAddress
                if ($request.Headers.Get("Host")) { $hostHeader = $request.Headers.Get("Host") }
                $protocol = if ($request.IsSecureConnection) { "https" } else { "http" }
                $hostUrl = "$($protocol)://$hostHeader"
                
                $content = $content -replace '(?i)\$ServerBaseUrl\s*=\s*"[^"]*"', ('$$ServerBaseUrl = "' + $hostUrl + '"')
                
                $response.StatusCode = 200
                $response.ContentType = "text/plain; charset=utf-8"
                $response.AddHeader("Content-Disposition", "attachment; filename=`"setup.ps1`"")
                $buffer = [System.Text.Encoding]::UTF8.GetBytes($content)
                $response.ContentLength64 = $buffer.Length
                $response.OutputStream.Write($buffer, 0, $buffer.Length)
                $response.OutputStream.Close()
            } else {
                Send-TextResponse $response "404 Not Found" 404 "text/plain"
            }
        }
        elseif ($url.EndsWith("/RobloxPlayerBeta.enc")) {
            $encPath = Join-Path $UploadsDir "RobloxPlayerBeta.enc"
            
            if (Test-Path $ReleasesFile) {
                $data = Get-Content $ReleasesFile -Raw | ConvertFrom-Json
                $data.totalDownloads = [int]$data.totalDownloads + 1
                $data | ConvertTo-Json -Depth 5 | Out-File $ReleasesFile -Encoding utf8 -Force
            }
            
            Send-FileResponse $response $encPath "application/octet-stream" $true "RobloxPlayerBeta.enc"
        }
        elseif ($url.StartsWith("/setup.ps1")) {
            $setupPath = Join-Path $scriptRoot "..\setup.ps1"
            if (Test-Path $setupPath) {
                $content = Get-Content $setupPath -Raw
                $hostHeader = $request.UserHostAddress
                if ($request.Headers.Get("Host")) { $hostHeader = $request.Headers.Get("Host") }
                $protocol = if ($request.IsSecureConnection) { "https" } else { "http" }
                $hostUrl = "$($protocol)://$hostHeader"
                $content = $content -replace '(?i)\$ServerBaseUrl\s*=\s*"[^"]*"', ('$$ServerBaseUrl = "' + $hostUrl + '"')
                Send-TextResponse $response $content 200 "text/plain"
            } else {
                Send-TextResponse $response "404 Not Found" 404 "text/plain"
            }
        }
        elseif ($url.StartsWith("/installer.ps1")) {
            $installerPath = Join-Path $scriptRoot "..\installer.ps1"
            if (Test-Path $installerPath) {
                $content = Get-Content $installerPath -Raw
                $hostHeader = $request.UserHostAddress
                if ($request.Headers.Get("Host")) { $hostHeader = $request.Headers.Get("Host") }
                $protocol = if ($request.IsSecureConnection) { "https" } else { "http" }
                $hostUrl = "$($protocol)://$hostHeader"
                $content = $content -replace '(?i)\$ServerBaseUrl\s*=\s*"[^"]*"', ('$$ServerBaseUrl = "' + $hostUrl + '"')
                Send-TextResponse $response $content 200 "text/plain"
            } else {
                Send-TextResponse $response "404 Not Found" 404 "text/plain"
            }
        }
        elseif ($url.StartsWith("/api/release/download-binary")) {
            $encPath = Join-Path $UploadsDir "RobloxPlayerBeta.enc"
            if (Test-Path $encPath) {
                if (Test-Path $ReleasesFile) {
                    $data = Get-Content $ReleasesFile -Raw | ConvertFrom-Json
                    $data.totalDownloads = [int]$data.totalDownloads + 1
                    $data | ConvertTo-Json -Depth 5 | Out-File $ReleasesFile -Encoding utf8 -Force
                }

                $encBytes = [System.IO.File]::ReadAllBytes($encPath)

                $DecKey = [byte[]](0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,0x23,
                                   0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
                $DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x21,0x40)

                $aes         = [System.Security.Cryptography.Aes]::Create()
                $aes.Key     = $DecKey
                $aes.IV      = $DecIV
                $aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
                $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
                $dec         = $aes.CreateDecryptor()
                $decBytes    = $dec.TransformFinalBlock($encBytes, 0, $encBytes.Length)
                $aes.Dispose()

                $response.StatusCode = 200
                $response.ContentType = "application/octet-stream"
                $response.AddHeader("Content-Disposition", "attachment; filename=`"RobloxPlayerBeta.exe`"")
                $response.ContentLength64 = $decBytes.Length
                $response.OutputStream.Write($decBytes, 0, $decBytes.Length)
                $response.OutputStream.Close()
            } else {
                Send-TextResponse $response '{"success":false,"error":"Encrypted payload not found"}' 404 "application/json"
            }
        }
        elseif ($url.StartsWith("/api/release/latest")) {
            if (Test-Path $ReleasesFile) {
                $rawJson = Get-Content $ReleasesFile -Raw
                $data = ConvertFrom-Json $rawJson
                $resData = @{
                    success = $true
                    version = $data.latestVersion
                    changelog = $data.latestChangelog
                    totalDownloads = $data.totalDownloads
                    date = $data.history[-1].date
                    md5 = $data.latestHash
                }
                Send-TextResponse $response (ConvertTo-Json $resData) 200 "application/json"
            } else {
                Send-TextResponse $response '{"success":false,"error":"releases.json not found"}' 404 "application/json"
            }
        }
        elseif ($url.StartsWith("/api/release/analytics")) {
            $reader = New-Object System.IO.StreamReader($request.InputStream)
            $body = $reader.ReadToEnd()
            
            $pin = ""
            if ($body -match '"pin"\s*:\s*"([^"]+)"') { $pin = $Matches[1] }

            if ($pin -ne "1337") {
                Send-TextResponse $response '{"success":false,"error":"Unauthorized"}' 403 "application/json"
            } else {
                if (Test-Path $ReleasesFile) {
                    $rawJson = Get-Content $ReleasesFile -Raw
                    $data = ConvertFrom-Json $rawJson
                    $resData = @{
                        success = $true
                        totalDownloads = $data.totalDownloads
                        history = $data.history
                    }
                    Send-TextResponse $response (ConvertTo-Json $resData -Depth 5) 200 "application/json"
                } else {
                    Send-TextResponse $response '{"success":false,"error":"No history"}' 404 "application/json"
                }
            }
        }
        elseif ($url.StartsWith("/api/release/publish")) {
            $contentTypeHeader = $request.ContentType
            $boundary = ""
            if ($contentTypeHeader -match 'boundary=(.*)$') {
                $boundary = $Matches[1]
            }

            if ($boundary -eq "") {
                Send-TextResponse $response '{"success":false,"error":"Missing boundary"}' 400 "application/json"
                continue
            }

            $inputStream = $request.InputStream
            $totalLength = $request.ContentLength64
            $reqBytes = New-Object byte[] $totalLength
            $readBytes = 0
            while ($readBytes -lt $totalLength) {
                $read = $inputStream.Read($reqBytes, $readBytes, $totalLength - $readBytes)
                if ($read -eq 0) { break }
                $readBytes += $read
            }

            $reqString = [System.Text.Encoding]::Default.GetString($reqBytes)
            $parts = $reqString -split "--$boundary"

            $pin = ""
            $version = ""
            $changelog = ""
            $fileDataStart = -1
            $fileDataLength = -1
            $binaryPartIndex = -1

            for ($i = 0; $i -lt $parts.Length; $i++) {
                $part = $parts[$i]
                if ($part -match 'name="pin"[\r\n]+([\r\n]+)(.*?)\r\n') {
                    $pin = $Matches[2].Trim()
                }
                elseif ($part -match 'name="version"[\r\n]+([\r\n]+)(.*?)\r\n') {
                    $version = $Matches[2].Trim()
                }
                elseif ($part -match 'name="changelog"[\r\n]+([\r\n]+)(.*?)\r\n') {
                    $changelog = $Matches[2].Trim()
                }
                elseif ($part -match 'name="binary"; filename=') {
                    $binaryPartIndex = $i
                }
            }

            if ($pin -ne "1337") {
                Send-TextResponse $response '{"success":false,"error":"Unauthorized: Invalid Admin PIN"}' 403 "application/json"
                continue
            }

            if ($version -eq "" -or $changelog -eq "") {
                Send-TextResponse $response '{"success":false,"error":"Missing version or changelog"}' 400 "application/json"
                continue
            }

            if ($binaryPartIndex -ne -1) {
                $boundaryBytes = [System.Text.Encoding]::Default.GetBytes("--$boundary")
                
                $offsets = New-Object System.Collections.Generic.List[int]
                for ($j = 0; $j -le ($reqBytes.Length - $boundaryBytes.Length); $j++) {
                    $match = $true
                    for ($k = 0; $k -lt $boundaryBytes.Length; $k++) {
                        if ($reqBytes[$j + $k] -ne $boundaryBytes[$k]) {
                            $match = $false
                            break
                        }
                    }
                    if ($match) {
                        $offsets.Add($j)
                    }
                }

                if ($offsets.Count -gt $binaryPartIndex) {
                    $partStart = $offsets[$binaryPartIndex] + $boundaryBytes.Length
                    
                    $doubleCrlf = [System.Text.Encoding]::Default.GetBytes("`r`n`r`n")
                    $headerEndOffset = -1
                    for ($j = $partStart; $j -le ($reqBytes.Length - 4); $j++) {
                        if ($reqBytes[$j] -eq 13 -and $reqBytes[$j+1] -eq 10 -and $reqBytes[$j+2] -eq 13 -and $reqBytes[$j+3] -eq 10) {
                            $headerEndOffset = $j + 4
                            break
                        }
                    }

                    if ($headerEndOffset -ne -1) {
                        $partEnd = $offsets[$binaryPartIndex + 1] - 2
                        $fileLength = $partEnd - $headerEndOffset
                        
                        $fileBytes = New-Object byte[] $fileLength
                        [System.Array]::Copy($reqBytes, $headerEndOffset, $fileBytes, 0, $fileLength)

                        $md5 = [System.Security.Cryptography.MD5]::Create()
                        $hashBytes = $md5.ComputeHash($fileBytes)
                        $hashStr = ($hashBytes | ForEach-Object { $_.ToString("x2") }) -join ""
                        $md5.Dispose()

                        $DecKey = [byte[]](0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,0x23,
                                           0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22)
                        $DecIV  = [byte[]](0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x21,0x40)

                        $aes         = [System.Security.Cryptography.Aes]::Create()
                        $aes.Key     = $DecKey
                        $aes.IV      = $DecIV
                        $aes.Mode    = [System.Security.Cryptography.CipherMode]::CBC
                        $aes.Padding = [System.Security.Cryptography.PaddingMode]::PKCS7
                        $enc         = $aes.CreateEncryptor()
                        $encBytes    = $enc.TransformFinalBlock($fileBytes, 0, $fileBytes.Length)
                        $aes.Dispose()

                        $encPath = Join-Path $UploadsDir "RobloxPlayerBeta.enc"
                        [System.IO.File]::WriteAllBytes($encPath, $encBytes)

                        $LegacyExe = Join-Path $UploadsDir "RobloxPlayerBeta.exe"
                        if (Test-Path $LegacyExe) {
                            Remove-Item $LegacyExe -Force -ErrorAction SilentlyContinue | Out-Null
                        }

                        if (Test-Path $ReleasesFile) {
                            $data = Get-Content $ReleasesFile -Raw | ConvertFrom-Json
                            
                            $newRelease = [PSCustomObject]@{
                                version = $version
                                date = (Get-Date -Format "yyyy-MM-dd")
                                changelog = $changelog
                                fileName = "RobloxPlayerBeta.enc"
                                md5 = $hashStr
                            }

                            $data.latestVersion = $version
                            $data.latestChangelog = $changelog
                            $data.latestHash = $hashStr
                            
                            $newHistory = [System.Collections.Generic.List[PSCustomObject]]::new()
                            foreach ($h in $data.history) { $newHistory.Add($h) }
                            $newHistory.Add($newRelease)
                            $data.history = $newHistory

                            $data | ConvertTo-Json -Depth 5 | Out-File $ReleasesFile -Encoding utf8 -Force
                        }
                        
                        Send-TextResponse $response '{"success":true,"message":"New release published successfully!"}' 200 "application/json"
                    } else {
                        Send-TextResponse $response '{"success":false,"error":"Failed to extract binary payload headers"}' 500 "application/json"
                    }
                } else {
                    Send-TextResponse $response '{"success":false,"error":"Failed to locate binary boundary offsets"}' 500 "application/json"
                }
            } else {
                Send-TextResponse $response '{"success":false,"error":"Binary file upload required"}' 400 "application/json"
            }
        }
        else {
            Send-TextResponse $response "404 Not Found" 404 "text/plain"
        }
    }
    catch {
        if ($null -ne $response) {
            try { $response.Close() } catch {}
        }
    }
}
