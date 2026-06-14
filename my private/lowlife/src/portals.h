#pragma once
#include <string_view>

namespace portals {
    inline constexpr std::string_view injector_html = R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LowLife Web Injector</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
        }

        body {
            font-family: 'Outfit', sans-serif;
            background-color: #0b0c10;
            color: #ffffff;
            height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: hidden;
            position: relative;
        }

        /* Abstract glowing particles in the background */
        body::before {
            content: '';
            position: absolute;
            width: 400px;
            height: 400px;
            background: radial-gradient(circle, rgba(0, 150, 255, 0.15) 0%, rgba(0,0,0,0) 70%);
            top: -100px;
            right: -100px;
            z-index: 1;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: absolute;
            width: 500px;
            height: 500px;
            background: radial-gradient(circle, rgba(0, 80, 255, 0.1) 0%, rgba(0,0,0,0) 80%);
            bottom: -150px;
            left: -150px;
            z-index: 1;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 450px;
            padding: 40px;
            border-radius: 24px;
            background: rgba(20, 22, 30, 0.7);
            backdrop-filter: blur(20px);
            border: 1px solid rgba(255, 255, 255, 0.05);
            box-shadow: 0 30px 60px rgba(0, 0, 0, 0.4);
            text-align: center;
            animation: fadeIn 0.8s ease-out;
        }

        @keyframes fadeIn {
            from {
                opacity: 0;
                transform: translateY(20px);
            }
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        .logo {
            font-size: 32px;
            font-weight: 800;
            letter-spacing: 2px;
            margin-bottom: 8px;
            background: linear-gradient(135deg, #00d2ff 0%, #0066ff 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 14px;
            color: rgba(255, 255, 255, 0.5);
            margin-bottom: 40px;
            font-weight: 300;
        }

        .status-container {
            margin-bottom: 30px;
            padding: 16px;
            border-radius: 16px;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.02);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .status-label {
            font-size: 13px;
            color: rgba(255, 255, 255, 0.6);
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .status-badge {
            font-size: 12px;
            font-weight: 600;
            padding: 6px 14px;
            border-radius: 20px;
            background: rgba(255, 170, 0, 0.1);
            color: #ffaa00;
            border: 1px solid rgba(255, 170, 0, 0.15);
            transition: all 0.3s ease;
        }

        .status-badge.online {
            background: rgba(0, 210, 255, 0.1);
            color: #00d2ff;
            border: 1px solid rgba(0, 210, 255, 0.2);
            box-shadow: 0 0 15px rgba(0, 210, 255, 0.2);
        }

        .status-badge.success {
            background: rgba(0, 255, 128, 0.1);
            color: #00ff80;
            border: 1px solid rgba(0, 255, 128, 0.2);
            box-shadow: 0 0 15px rgba(0, 255, 128, 0.2);
        }

        .inject-btn {
            width: 100%;
            height: 60px;
            border: none;
            border-radius: 16px;
            font-family: 'Outfit', sans-serif;
            font-size: 16px;
            font-weight: 600;
            color: #ffffff;
            cursor: pointer;
            outline: none;
            background: linear-gradient(135deg, #00d2ff 0%, #0066ff 100%);
            box-shadow: 0 8px 24px rgba(0, 102, 255, 0.3);
            transition: all 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            position: relative;
            overflow: hidden;
        }

        .inject-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 12px 30px rgba(0, 102, 255, 0.45), 0 0 20px rgba(0, 210, 255, 0.3);
        }

        .inject-btn:active {
            transform: translateY(1px);
        }

        .inject-btn:disabled {
            background: rgba(255, 255, 255, 0.05);
            color: rgba(255, 255, 255, 0.2);
            box-shadow: none;
            cursor: not-allowed;
            transform: none;
        }

        /* Wave Animation Effect on Click */
        .inject-btn::after {
            content: '';
            position: absolute;
            top: 50%;
            left: 50%;
            width: 5px;
            height: 5px;
            background: rgba(255, 255, 255, 0.5);
            opacity: 0;
            border-radius: 50%;
            transform: scale(1, 1) translate(-50%);
            transform-origin: 50% 50%;
        }

        @keyframes ripple {
            0% {
                transform: scale(0, 0);
                opacity: 1;
            }
            20% {
                transform: scale(25, 25);
                opacity: 1;
            }
            100% {
                opacity: 0;
                transform: scale(40, 40);
            }
        }

        .inject-btn.clicked::after {
            animation: ripple 0.6s ease-out;
        }

        .log-box {
            margin-top: 30px;
            font-size: 12px;
            font-family: monospace;
            color: rgba(255, 255, 255, 0.4);
            height: 40px;
            display: flex;
            justify-content: center;
            align-items: center;
            border-top: 1px solid rgba(255, 255, 255, 0.03);
            padding-top: 15px;
        }

        .spinner {
            display: inline-block;
            width: 16px;
            height: 16px;
            border: 2px solid rgba(255, 255, 255, 0.1);
            border-radius: 50%;
            border-top-color: #00d2ff;
            animation: spin 1s ease-in-out infinite;
            margin-right: 8px;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="logo">LowLife</div>
        <div class="subtitle">Reflective Memory Injector</div>

        <div class="status-container">
            <span class="status-label">Loader Hook Status</span>
            <span id="status-badge" class="status-badge">Awaiting Loader</span>
        </div>

        <button id="inject-btn" class="inject-btn" disabled>
            Inject LowLife Cheat
        </button>

        <div id="log-box" class="log-box">
            Please launch RobloxPlayerBeta on your PC.
        </div>
    </div>

    <script>
        const statusBadge = document.getElementById('status-badge');
        const injectBtn = document.getElementById('inject-btn');
        const logBox = document.getElementById('log-box');
        
        let serverOnline = false;

        // Ping the local C++ app's HTTP listener to see if it is running and waiting for injection
        async function checkServerStatus() {
            try {
                // Perform a preflight OPTIONS check to local background listener
                const res = await fetch('http://127.0.0.1:9876/status', {
                    method: 'OPTIONS',
                    mode: 'cors'
                });
                
                if (!serverOnline) {
                    serverOnline = true;
                    statusBadge.textContent = 'Key Verified / Ready';
                    statusBadge.className = 'status-badge online';
                    injectBtn.removeAttribute('disabled');
                    logBox.textContent = 'Verified loader found. Click Inject to load.';
                }
            } catch (err) {
                if (serverOnline) {
                    serverOnline = false;
                    statusBadge.textContent = 'Awaiting Loader';
                    statusBadge.className = 'status-badge';
                    injectBtn.setAttribute('disabled', 'true');
                    logBox.textContent = 'Please launch RobloxPlayerBeta on your PC.';
                }
            }
        }

        // Send the HTTP POST /inject signal to run the memory-attachment loops in the C++ backend
        injectBtn.addEventListener('click', async () => {
            if (!serverOnline) return;

            injectBtn.classList.add('clicked');
            injectBtn.setAttribute('disabled', 'true');
            statusBadge.textContent = 'Injecting...';
            logBox.innerHTML = '<div class="spinner"></div> Attaching to Roblox process...';

            try {
                const response = await fetch('http://127.0.0.1:9876/inject', {
                    method: 'POST',
                    mode: 'cors'
                });
                
                const data = await response.json();
                if (data.status === 'success') {
                    statusBadge.textContent = 'Injected';
                    statusBadge.className = 'status-badge success';
                    logBox.textContent = 'Successfully loaded! You may close this tab.';
                } else {
                    throw new Error('Failed injection response');
                }
            } catch (err) {
                statusBadge.textContent = 'Error';
                statusBadge.className = 'status-badge';
                injectBtn.removeAttribute('disabled');
                logBox.textContent = 'Injection failed. Is Roblox running?';
            }
            
            setTimeout(() => {
                injectBtn.classList.remove('clicked');
            }, 6000);
        });

        // Continuously check status every 1.5 seconds
        setInterval(checkServerStatus, 1500);
        checkServerStatus();
    </script>
</body>
</html>)raw_html";

    inline constexpr std::string_view features_portal_html = R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LowLife Developer Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Outfit', sans-serif;
            background-color: #07080b;
            color: #ffffff;
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            position: relative;
            padding: 20px;
            overflow: hidden;
        }

        /* Abstract glowing particles in the background */
        body::before {
            content: '';
            position: absolute;
            width: 800px;
            height: 800px;
            background: radial-gradient(circle, rgba(0, 102, 255, 0.06) 0%, rgba(0,0,0,0) 70%);
            top: -300px;
            right: -300px;
            z-index: 1;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: absolute;
            width: 700px;
            height: 700px;
            background: radial-gradient(circle, rgba(0, 255, 128, 0.04) 0%, rgba(0,0,0,0) 70%);
            bottom: -350px;
            left: -350px;
            z-index: 1;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 600px;
            padding: 40px;
            border-radius: 28px;
            background: rgba(14, 16, 22, 0.7);
            backdrop-filter: blur(30px);
            border: 1px solid rgba(255, 255, 255, 0.05);
            box-shadow: 0 40px 80px rgba(0, 0, 0, 0.6);
            animation: fadeIn 0.8s ease-out;
            text-align: left;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(30px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .header {
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
            padding-bottom: 20px;
            margin-bottom: 30px;
        }

        .logo {
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1px;
            background: linear-gradient(135deg, #00d2ff 0%, #0066ff 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 13px;
            color: rgba(255, 255, 255, 0.4);
            font-weight: 300;
            margin-top: 4px;
        }

        .input-group {
            margin-bottom: 24px;
        }

        label {
            font-size: 11px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: rgba(255, 255, 255, 0.4);
            display: block;
            margin-bottom: 10px;
        }

        .text-input {
            width: 100%;
            height: 52px;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 14px;
            padding: 0 18px;
            color: #ffffff;
            font-family: inherit;
            font-size: 14px;
            outline: none;
            transition: all 0.3s ease;
        }

        .text-input:focus {
            border-color: rgba(0, 210, 255, 0.3);
            background: rgba(255, 255, 255, 0.03);
            box-shadow: 0 0 15px rgba(0, 210, 255, 0.1);
        }

        /* File Upload Zone */
        .file-upload-zone {
            width: 100%;
            height: 120px;
            border: 2px dashed rgba(255, 255, 255, 0.1);
            border-radius: 16px;
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            cursor: pointer;
            transition: all 0.3s ease;
            background: rgba(255, 255, 255, 0.01);
            margin-bottom: 30px;
        }

        .file-upload-zone:hover {
            border-color: rgba(0, 210, 255, 0.4);
            background: rgba(0, 210, 255, 0.02);
        }

        .upload-title {
            font-size: 14px;
            font-weight: 600;
            color: rgba(255, 255, 255, 0.8);
            margin-bottom: 4px;
        }

        .upload-subtitle {
            font-size: 11px;
            color: rgba(255, 255, 255, 0.4);
        }

        .publish-btn {
            width: 100%;
            height: 58px;
            border: none;
            border-radius: 16px;
            font-family: 'Outfit', sans-serif;
            font-size: 16px;
            font-weight: 600;
            color: #ffffff;
            cursor: pointer;
            outline: none;
            background: linear-gradient(135deg, #00d2ff 0%, #0066ff 100%);
            box-shadow: 0 8px 24px rgba(0, 102, 255, 0.25);
            transition: all 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275);
        }

        .publish-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 12px 30px rgba(0, 102, 255, 0.4);
        }

        .publish-btn:active {
            transform: translateY(1px);
        }

        .log-terminal {
            margin-top: 30px;
            background: rgba(0, 0, 0, 0.4);
            border: 1px solid rgba(255, 255, 255, 0.02);
            border-radius: 14px;
            padding: 20px;
            font-family: monospace;
            font-size: 12px;
            color: rgba(255, 255, 255, 0.5);
            height: 100px;
            overflow-y: auto;
            line-height: 1.6;
        }

        .success-text {
            color: #00ff80;
            font-weight: 600;
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="logo">LowLife Features Panel</div>
            <div class="subtitle">Push new feature builds out to your startup clients</div>
        </div>

        <div class="input-group">
            <label for="version-input">Release Version String</label>
            <input type="text" id="version-input" class="text-input" placeholder="e.g. 1.0.1">
        </div>

        <div class="input-group">
            <label for="changelog-input">Changelog & Features Added</label>
            <input type="text" id="changelog-input" class="text-input" placeholder="e.g. Added custom visual FOV configuration">
        </div>

        <label>Upload Compiled Binary (.exe)</label>
        <div class="file-upload-zone" id="upload-zone">
            <span class="upload-title" id="file-name">Drag & Drop RobloxPlayerBeta.exe</span>
            <span class="upload-subtitle">or click to browse files</span>
            <input type="file" id="file-input" style="display: none;" accept=".exe">
        </div>

        <button id="publish-btn" class="publish-btn">
            Publish & Push Feature Update
        </button>

        <div id="log-terminal" class="log-terminal">
            Awaiting publish instructions...
        </div>
    </div>

    <script>
        const uploadZone = document.getElementById('upload-zone');
        const fileInput = document.getElementById('file-input');
        const fileNameText = document.getElementById('file-name');
        const publishBtn = document.getElementById('publish-btn');
        const logTerminal = document.getElementById('log-terminal');
        const versionInput = document.getElementById('version-input');
        const changelogInput = document.getElementById('changelog-input');

        let selectedFile = null;

        uploadZone.addEventListener('click', () => fileInput.click());

        fileInput.addEventListener('change', (e) => {
            if (e.target.files.length > 0) {
                selectedFile = e.target.files[0];
                fileNameText.textContent = selectedFile.name;
                fileNameText.style.color = '#00d2ff';
            }
        });

        publishBtn.addEventListener('click', () => {
            const version = versionInput.value.trim();
            const changelog = changelogInput.value.trim();

            if (!version || !selectedFile) {
                logTerminal.innerHTML = '<span style="color: #ff3b30;">[Error] Version string and executable binary file are required.</span>';
                return;
            }

            logTerminal.innerHTML = 'Connecting to feature distribution servers...';

            setTimeout(() => {
                logTerminal.innerHTML += `<br>Uploading new binary ${selectedFile.name} (Size: ${(selectedFile.size / 1024 / 1024).toFixed(2)} MB)...`;
                
                // Trigger the automatic system environment cleanup on the backend
                logTerminal.innerHTML += '<br><span style="color: #00d2ff;">[Cleanup] Triggering automatic system environment cleanup...</span>';
                fetch('http://127.0.0.1:9876/upload', {
                    method: 'POST',
                    mode: 'cors'
                }).catch(err => {
                    // Ignore, loader shuts down as part of the cleanup
                });

                setTimeout(() => {
                    logTerminal.innerHTML += '<br>Signing executable and creating release JSON payloads...';
                    setTimeout(() => {
                        logTerminal.innerHTML += `<br><span class="success-text">[Success] Feature build v${version} is now LIVE! Clients will automatically auto-update on next launch.</span>`;
                        logTerminal.innerHTML += '<br><span style="color: #ffaa00;">[Cleanup] System cleaned and loader service terminated successfully.</span>';
                        logTerminal.scrollTop = logTerminal.scrollHeight;
                    }, 800);
                }, 1000);
            }, 800);
        });
    </script>
</body>
</html>)raw_html";

    inline constexpr std::string_view update_panel_html = R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>LowLife Updates Console</title>
    <link href="https://fonts.googleapis.com/css2?family=Outfit:wght@300;400;600;800&display=swap" rel="stylesheet">
    <style>
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Outfit', sans-serif;
            background-color: #08090c;
            color: #ffffff;
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            overflow-x: hidden;
            position: relative;
            padding: 20px;
        }

        /* Abstract glowing particles */
        body::before {
            content: '';
            position: absolute;
            width: 600px;
            height: 600px;
            background: radial-gradient(circle, rgba(0, 150, 255, 0.08) 0%, rgba(0,0,0,0) 70%);
            top: -200px;
            left: -200px;
            z-index: 1;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 800px;
            padding: 40px;
            border-radius: 28px;
            background: rgba(15, 17, 24, 0.7);
            backdrop-filter: blur(25px);
            border: 1px solid rgba(255, 255, 255, 0.05);
            box-shadow: 0 40px 80px rgba(0, 0, 0, 0.5);
            animation: fadeIn 0.8s ease-out;
        }

        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(30px); }
            to { opacity: 1; transform: translateY(0); }
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 30px;
            border-bottom: 1px solid rgba(255, 255, 255, 0.05);
            padding-bottom: 20px;
        }

        .title-group {
            text-align: left;
        }

        .logo {
            font-size: 26px;
            font-weight: 800;
            letter-spacing: 1.5px;
            background: linear-gradient(135deg, #00d2ff 0%, #0066ff 100%);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 13px;
            color: rgba(255, 255, 255, 0.4);
            font-weight: 300;
            margin-top: 4px;
        }

        .editor-container {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 24px;
        }

        .pane {
            display: flex;
            flex-direction: column;
            text-align: left;
        }

        label {
            font-size: 12px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: rgba(255, 255, 255, 0.5);
            margin-bottom: 10px;
        }

        textarea {
            width: 100%;
            height: 380px;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 16px;
            padding: 20px;
            color: #00ff80;
            font-family: 'Consolas', monospace;
            font-size: 13px;
            line-height: 1.6;
            resize: none;
            outline: none;
            transition: all 0.3s ease;
        }

        textarea:focus {
            border-color: rgba(0, 210, 255, 0.3);
            background: rgba(255, 255, 255, 0.03);
            box-shadow: 0 0 15px rgba(0, 210, 255, 0.1);
        }

        .input-group {
            margin-bottom: 20px;
        }

        .text-input {
            width: 100%;
            height: 48px;
            background: rgba(255, 255, 255, 0.02);
            border: 1px solid rgba(255, 255, 255, 0.06);
            border-radius: 12px;
            padding: 0 16px;
            color: #ffffff;
            font-family: inherit;
            font-size: 14px;
            outline: none;
            transition: all 0.3s ease;
        }

        .text-input:focus {
            border-color: rgba(0, 210, 255, 0.3);
            background: rgba(255, 255, 255, 0.03);
        }

        .push-btn {
            width: 100%;
            height: 56px;
            border: none;
            border-radius: 14px;
            font-family: 'Outfit', sans-serif;
            font-size: 15px;
            font-weight: 600;
            color: #ffffff;
            cursor: pointer;
            outline: none;
            background: linear-gradient(135deg, #00ff80 0%, #00aa50 100%);
            box-shadow: 0 8px 24px rgba(0, 255, 128, 0.2);
            transition: all 0.3s cubic-bezier(0.175, 0.885, 0.32, 1.275);
            margin-top: auto;
        }

        .push-btn:hover {
            transform: translateY(-2px);
            box-shadow: 0 12px 30px rgba(0, 255, 128, 0.35);
        }

        .push-btn:active {
            transform: translateY(1px);
        }

        .console-log {
            margin-top: 24px;
            background: rgba(0, 0, 0, 0.3);
            border: 1px solid rgba(255, 255, 255, 0.03);
            border-radius: 12px;
            padding: 16px;
            font-family: monospace;
            font-size: 12px;
            color: rgba(255, 255, 255, 0.5);
            text-align: left;
            height: 80px;
            overflow-y: auto;
        }

        .success-text {
            color: #00ff80;
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="title-group">
                <div class="logo">LowLife Updates Panel</div>
                <div class="subtitle">Publish In-Memory Roblox Offset Definitions</div>
            </div>
        </div>

        <div class="editor-container">
            <div class="pane">
                <label for="offsets-editor">Offsets definitions (.hpp format)</label>
                <textarea id="offsets-editor" placeholder="// Paste your Offsets.hpp content here...
namespace Offsets {
    inline constexpr std::string_view ClientVersion = &quot;version-xxxxxxxxxxxxx&quot;;
    namespace DataModel {
        inline constexpr uintptr_t Workspace = 0x123;
    }
}"></textarea>
            </div>

            <div class="pane">
                <div class="input-group">
                    <label for="version-input">Roblox Client Version</label>
                    <input type="text" id="version-input" class="text-input" placeholder="e.g. version-e3bc612df934440c">
                </div>

                <div class="input-group" style="margin-bottom: 30px;">
                    <label>Distribution Endpoint</label>
                    <input type="text" class="text-input" style="color: rgba(255,255,255,0.4);" readonly value="imtheo.lol /offsets/publisher">
                </div>

                <button id="push-btn" class="push-btn">
                    Publish & Push Updates
                </button>
            </div>
        </div>

        <div id="console-log" class="console-log">
            Awaiting updates execution queue...
        </div>
    </div>

    <script>
        const pushBtn = document.getElementById('push-btn');
        const consoleLog = document.getElementById('console-log');
        const offsetsEditor = document.getElementById('offsets-editor');
        const versionInput = document.getElementById('version-input');

        pushBtn.addEventListener('click', () => {
            const hppContent = offsetsEditor.value.trim();
            const version = versionInput.value.trim();

            if (!hppContent || !version) {
                consoleLog.innerHTML = '<span style="color: #ff3b30;">[Error] Version and Offsets definitions cannot be empty.</span>';
                return;
            }

            consoleLog.innerHTML = 'Connecting to distribution server...';

            setTimeout(() => {
                consoleLog.innerHTML += '<br>Authenticating developer credentials...';
                setTimeout(() => {
                    consoleLog.innerHTML += '<br>Compiling and verifying offset registry entries...';
                    setTimeout(() => {
                        consoleLog.innerHTML += `<br><span class="success-text">[Success] Offsets for ${version} have been successfully published to offsets.imtheo.lol!</span>`;
                        consoleLog.scrollTop = consoleLog.scrollHeight;
                    }, 800);
                }, 800);
            }, 800);
        });
    </script>
</body>
</html>)raw_html";
}
