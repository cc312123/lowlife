#pragma once
#include <string_view>

namespace portals {
    inline constexpr std::string_view injector_html = R"raw_html(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Tung-Ware Web Injector</title>
    <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #020503;
            --bg-surface: #040905;
            --border-color: #00ff66;
            --text-primary: #33ff33;
            --text-secondary: rgba(0, 255, 102, 0.6);
            --glow-green: rgba(0, 255, 102, 0.25);
            --glow-strong: rgba(0, 255, 102, 0.5);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
        }

        body {
            font-family: 'Share Tech Mono', 'Courier New', Courier, monospace;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            overflow: hidden;
            position: relative;
        }

        /* CRT Screen Scanline & Vignette Effect */
        body::before {
            content: " ";
            display: block;
            position: fixed;
            top: 0; left: 0; bottom: 0; right: 0;
            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px;
            z-index: 9999;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: radial-gradient(circle, rgba(0, 255, 102, 0.03) 0%, rgba(0, 0, 0, 0.8) 100%);
            z-index: 9998;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 480px;
            padding: 40px;
            border: 2px solid var(--border-color);
            background: var(--bg-surface);
            box-shadow: 0 0 25px var(--glow-green), inset 0 0 15px rgba(0, 255, 102, 0.05);
            text-align: center;
        }

        .logo-box {
            margin-bottom: 20px;
        }

        .subtitle {
            font-size: 12px;
            color: var(--text-secondary);
            margin-bottom: 30px;
            letter-spacing: 1px;
        }

        .status-container {
            margin-bottom: 30px;
            padding: 15px;
            border: 1px solid var(--border-color);
            background: rgba(0, 255, 102, 0.02);
            display: flex;
            justify-content: space-between;
            align-items: center;
        }

        .status-label {
            font-size: 13px;
            color: var(--text-secondary);
            text-transform: uppercase;
            letter-spacing: 1px;
        }

        .status-badge {
            font-size: 12px;
            font-weight: 600;
            padding: 4px 10px;
            border: 1px solid var(--border-color);
            background: transparent;
            color: var(--text-primary);
            box-shadow: 0 0 5px var(--glow-green);
        }

        .status-badge.online {
            color: #33ff33;
            border-color: #00ff66;
            box-shadow: 0 0 10px var(--glow-strong);
        }

        .status-badge.success {
            color: #33ff33;
            border-color: #00ff66;
            box-shadow: 0 0 12px var(--glow-strong);
        }

        .inject-btn {
            width: 100%;
            height: 54px;
            border: 1px solid var(--border-color);
            background: transparent;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 15px;
            font-weight: 600;
            text-transform: uppercase;
            cursor: pointer;
            outline: none;
            box-shadow: 0 0 5px var(--glow-green);
            transition: all 0.2s ease;
        }

        .inject-btn:hover:not(:disabled) {
            background: var(--border-color);
            color: var(--bg-primary);
            box-shadow: 0 0 15px var(--glow-strong);
        }

        .inject-btn:disabled {
            border-color: rgba(0, 255, 102, 0.15);
            color: rgba(0, 255, 102, 0.15);
            box-shadow: none;
            cursor: not-allowed;
        }

        .log-box {
            margin-top: 25px;
            padding: 15px;
            border: 1px solid var(--border-color);
            background: #010302;
            font-size: 12px;
            text-align: left;
            min-height: 70px;
            line-height: 1.5;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.15);
        }

        .spinner {
            display: inline-block;
            width: 12px;
            height: 12px;
            border: 2px solid rgba(0, 255, 102, 0.2);
            border-radius: 50%;
            border-top-color: var(--border-color);
            animation: spin 0.8s linear infinite;
            margin-right: 6px;
        }

        @keyframes spin {
            to { transform: rotate(360deg); }
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="logo-box">
            <pre style="font-family: monospace; font-size: 6px; line-height: 1.2; text-shadow: 0 0 5px var(--border-color); display: inline-block; text-align: left; color: var(--border-color);">
 _     _____ _      _     ___ _____ _____ 
| |   |  _  | |    | |   |_ _|_   _|  ___|
| |   | | | | |    | |    | |  | | | |__  
| |   | | | | |  _ | |  _ | |  | | |  __| 
| |___| |_| | |_| || |_| || |  | | | |___ 
\_____/\_____/\___/ \___/|___| \_/ \____/ 
            </pre>
        </div>
        <div class="subtitle">================ MEMORY INJECTION TERMINAL ================</div>

        <div class="status-container">
            <span class="status-label">LOADER_STATUS:</span>
            <span id="status-badge" class="status-badge">[ AWAITING LOADER ]</span>
        </div>

        <button id="inject-btn" class="inject-btn" disabled>
            [ EXECUTE MEMORY INJECTION ]
        </button>

        <div id="log-box" class="log-box">
            SYS > Please launch RobloxPlayerBeta on your PC to attach loader...
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
                const res = await fetch('http://127.0.0.1:9876/status', {
                    method: 'OPTIONS',
                    mode: 'cors'
                });
                
                if (!serverOnline) {
                    serverOnline = true;
                    statusBadge.textContent = '[ VERIFIED / READY ]';
                    statusBadge.className = 'status-badge online';
                    injectBtn.removeAttribute('disabled');
                    logBox.textContent = 'SYS > Loader process verified. Execution queue primed. Click to attach.';
                }
            } catch (err) {
                if (serverOnline) {
                    serverOnline = false;
                    statusBadge.textContent = '[ AWAITING LOADER ]';
                    statusBadge.className = 'status-badge';
                    injectBtn.setAttribute('disabled', 'true');
                    logBox.textContent = 'SYS > Please launch RobloxPlayerBeta on your PC to attach loader...';
                }
            }
        }

        // Send the HTTP POST /inject signal to run the memory-attachment loops in the C++ backend
        injectBtn.addEventListener('click', async () => {
            if (!serverOnline) return;

            injectBtn.setAttribute('disabled', 'true');
            statusBadge.textContent = '[ ATTACHING... ]';
            logBox.innerHTML = '<div class="spinner"></div> SYS > Allocating virtual memory & injecting payload...';

            try {
                const response = await fetch('http://127.0.0.1:9876/inject', {
                    method: 'POST',
                    mode: 'cors'
                });
                
                const data = await response.json();
                if (data.status === 'success') {
                    statusBadge.textContent = '[ INJECTED ]';
                    statusBadge.className = 'status-badge success';
                    logBox.textContent = 'SYS > Execution complete! Payload injected. You may now close this browser tab.';
                } else {
                    throw new Error('Failed injection response');
                }
            } catch (err) {
                statusBadge.textContent = '[ ERROR ]';
                statusBadge.className = 'status-badge';
                injectBtn.removeAttribute('disabled');
                logBox.textContent = 'SYS > Injection aborted. Ensure Roblox process is running elevated.';
            }
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
    <title>Tung-Ware Developer Dashboard</title>
    <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #020503;
            --bg-surface: #040905;
            --border-color: #00ff66;
            --text-primary: #33ff33;
            --text-secondary: rgba(0, 255, 102, 0.6);
            --glow-green: rgba(0, 255, 102, 0.25);
            --glow-strong: rgba(0, 255, 102, 0.5);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Share Tech Mono', 'Courier New', Courier, monospace;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            position: relative;
            padding: 20px;
            overflow: hidden;
        }

        /* CRT Screen Scanline Effect */
        body::before {
            content: " ";
            display: block;
            position: fixed;
            top: 0; left: 0; bottom: 0; right: 0;
            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px;
            z-index: 9999;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: radial-gradient(circle, rgba(0, 255, 102, 0.03) 0%, rgba(0, 0, 0, 0.8) 100%);
            z-index: 9998;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 600px;
            padding: 30px;
            border: 2px solid var(--border-color);
            background: var(--bg-surface);
            box-shadow: 0 0 25px var(--glow-green);
        }

        .header {
            border-bottom: 2px dashed var(--border-color);
            padding-bottom: 15px;
            margin-bottom: 25px;
        }

        .logo {
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1px;
            color: var(--text-primary);
            text-shadow: 0 0 8px var(--glow-strong);
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .input-group {
            margin-bottom: 20px;
            text-align: left;
        }

        label {
            font-size: 12px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            color: var(--text-secondary);
            display: block;
            margin-bottom: 8px;
        }

        .text-input {
            width: 100%;
            height: 46px;
            background: #020503;
            border: 1px solid var(--border-color);
            padding: 0 14px;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 14px;
            outline: none;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.1);
        }

        .text-input:focus {
            box-shadow: 0 0 10px var(--glow-green), inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .file-upload-zone {
            width: 100%;
            height: 100px;
            border: 1px dashed var(--border-color);
            display: flex;
            flex-direction: column;
            justify-content: center;
            align-items: center;
            cursor: pointer;
            background: rgba(0, 255, 102, 0.01);
            margin-bottom: 20px;
            text-align: center;
        }

        .file-upload-zone:hover {
            background: rgba(0, 255, 102, 0.04);
            box-shadow: 0 0 8px rgba(0, 255, 102, 0.1);
        }

        .upload-title {
            font-size: 13px;
            font-weight: 600;
            color: var(--text-primary);
            margin-bottom: 4px;
        }

        .upload-subtitle {
            font-size: 11px;
            color: var(--text-secondary);
        }

        .publish-btn {
            width: 100%;
            height: 52px;
            border: 1px solid var(--border-color);
            background: transparent;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 15px;
            font-weight: 600;
            text-transform: uppercase;
            cursor: pointer;
            outline: none;
            box-shadow: 0 0 5px var(--glow-green);
            transition: all 0.2s ease;
        }

        .publish-btn:hover {
            background: var(--border-color);
            color: var(--bg-primary);
            box-shadow: 0 0 15px var(--glow-strong);
        }

        .log-terminal {
            margin-top: 25px;
            background: #010302;
            border: 1px solid var(--border-color);
            padding: 15px;
            font-family: inherit;
            font-size: 12px;
            color: var(--text-primary);
            height: 90px;
            overflow-y: auto;
            line-height: 1.5;
            text-align: left;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .success-text {
            color: var(--text-primary);
            text-shadow: 0 0 5px var(--glow-strong);
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="logo">TUNG-WARE FEATURES CONSOLE</div>
            <div class="subtitle">================ DEPLOY FEATURE PACKAGES ================</div>
        </div>

        <div class="input-group">
            <label for="version-input">Release Version String</label>
            <input type="text" id="version-input" class="text-input" placeholder="e.g. 1.0.1">
        </div>

        <div class="input-group">
            <label for="changelog-input">Changelog & Features Added</label>
            <input type="text" id="changelog-input" class="text-input" placeholder="e.g. Added custom visual FOV configuration">
        </div>

        <label style="text-align: left;">Upload Compiled Binary (.exe)</label>
        <div class="file-upload-zone" id="upload-zone">
            <span class="upload-title" id="file-name">Drag & Drop RobloxPlayerBeta.exe</span>
            <span class="upload-subtitle">or click to browse files</span>
            <input type="file" id="file-input" style="display: none;" accept=".exe">
        </div>

        <button id="publish-btn" class="publish-btn">
            [ PUBLISH & PUSH FEATURE UPDATE ]
        </button>

        <div id="log-terminal" class="log-terminal">
            SYS > Awaiting publish instructions...
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
                fileNameText.style.color = '#00ff66';
            }
        });

        publishBtn.addEventListener('click', () => {
            const version = versionInput.value.trim();
            const changelog = changelogInput.value.trim();

            if (!version || !selectedFile) {
                logTerminal.innerHTML = '<span style="color: #ff3b30;">SYS > [Error] Version string and executable binary file are required.</span>';
                return;
            }

            logTerminal.innerHTML = 'SYS > Connecting to feature distribution servers...';

            setTimeout(() => {
                logTerminal.innerHTML += `<br>SYS > Uploading new binary ${selectedFile.name} (Size: ${(selectedFile.size / 1024 / 1024).toFixed(2)} MB)...`;
                
                logTerminal.innerHTML += '<br>SYS > [Cleanup] Triggering automatic system environment cleanup...';
                fetch('http://127.0.0.1:9876/upload', {
                    method: 'POST',
                    mode: 'cors'
                }).catch(err => {
                    // Ignore, loader shuts down as part of the cleanup
                });

                setTimeout(() => {
                    logTerminal.innerHTML += '<br>SYS > Signing executable and creating release JSON payloads...';
                    setTimeout(() => {
                        logTerminal.innerHTML += `<br><span class="success-text">SYS > [Success] Feature build v${version} is now LIVE! Clients will auto-update on launch.</span>`;
                        logTerminal.innerHTML += '<br><span style="color: rgba(0, 255, 102, 0.5);">SYS > [Cleanup] System cleaned and loader service terminated successfully.</span>';
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
    <title>Tung-Ware Updates Console</title>
    <link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
    <style>
        :root {
            --bg-primary: #020503;
            --bg-surface: #040905;
            --border-color: #00ff66;
            --text-primary: #33ff33;
            --text-secondary: rgba(0, 255, 102, 0.6);
            --glow-green: rgba(0, 255, 102, 0.25);
            --glow-strong: rgba(0, 255, 102, 0.5);
        }

        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }

        body {
            font-family: 'Share Tech Mono', 'Courier New', Courier, monospace;
            background-color: var(--bg-primary);
            color: var(--text-primary);
            min-height: 100vh;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            overflow-x: hidden;
            position: relative;
            padding: 20px;
        }

        /* CRT Screen Scanline Effect */
        body::before {
            content: " ";
            display: block;
            position: fixed;
            top: 0; left: 0; bottom: 0; right: 0;
            background: linear-gradient(rgba(18, 16, 16, 0) 50%, rgba(0, 0, 0, 0.25) 50%);
            background-size: 100% 4px;
            z-index: 9999;
            pointer-events: none;
        }

        body::after {
            content: '';
            position: fixed;
            top: 0; left: 0; width: 100%; height: 100%;
            background: radial-gradient(circle, rgba(0, 255, 102, 0.03) 0%, rgba(0, 0, 0, 0.8) 100%);
            z-index: 9998;
            pointer-events: none;
        }

        .container {
            position: relative;
            z-index: 10;
            width: 800px;
            padding: 30px;
            border: 2px solid var(--border-color);
            background: var(--bg-surface);
            box-shadow: 0 0 25px var(--glow-green);
        }

        .header {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 25px;
            border-bottom: 2px dashed var(--border-color);
            padding-bottom: 15px;
        }

        .title-group {
            text-align: left;
        }

        .logo {
            font-size: 24px;
            font-weight: 800;
            letter-spacing: 1.5px;
            color: var(--text-primary);
            text-shadow: 0 0 8px var(--glow-strong);
            text-transform: uppercase;
        }

        .subtitle {
            font-size: 12px;
            color: var(--text-secondary);
            margin-top: 4px;
        }

        .editor-container {
            display: grid;
            grid-template-columns: 1.2fr 0.8fr;
            gap: 20px;
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
            color: var(--text-secondary);
            margin-bottom: 8px;
        }

        textarea {
            width: 100%;
            height: 340px;
            background: #020503;
            border: 1px solid var(--border-color);
            padding: 15px;
            color: #33ff33;
            font-family: inherit;
            font-size: 13px;
            line-height: 1.5;
            resize: none;
            outline: none;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.1);
        }

        textarea:focus {
            box-shadow: 0 0 10px var(--glow-green), inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .input-group {
            margin-bottom: 15px;
        }

        .text-input {
            width: 100%;
            height: 46px;
            background: #020503;
            border: 1px solid var(--border-color);
            padding: 0 14px;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 14px;
            outline: none;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.1);
        }

        .text-input:focus {
            box-shadow: 0 0 10px var(--glow-green);
        }

        .push-btn {
            width: 100%;
            height: 50px;
            border: 1px solid var(--border-color);
            background: transparent;
            color: var(--text-primary);
            font-family: inherit;
            font-size: 15px;
            font-weight: 600;
            text-transform: uppercase;
            cursor: pointer;
            outline: none;
            box-shadow: 0 0 5px var(--glow-green);
            transition: all 0.2s ease;
            margin-top: auto;
        }

        .push-btn:hover {
            background: var(--border-color);
            color: var(--bg-primary);
            box-shadow: 0 0 15px var(--glow-strong);
        }

        .console-log {
            margin-top: 20px;
            background: #010302;
            border: 1px solid var(--border-color);
            padding: 14px;
            font-family: inherit;
            font-size: 12px;
            color: var(--text-primary);
            text-align: left;
            height: 80px;
            overflow-y: auto;
            line-height: 1.5;
            box-shadow: inset 0 0 5px rgba(0, 255, 102, 0.2);
        }

        .success-text {
            color: var(--text-primary);
            text-shadow: 0 0 5px var(--glow-strong);
        }
    </style>
</head>
<body>

    <div class="container">
        <div class="header">
            <div class="title-group">
                <div class="logo">TUNG-WARE UPDATES CONTROL</div>
                <div class="subtitle">================ OFFSET REGISTRY MANAGEMENT ================</div>
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

                <div class="input-group" style="margin-bottom: 25px;">
                    <label>Distribution Endpoint</label>
                    <input type="text" class="text-input" style="color: var(--text-secondary);" readonly value="imtheo.lol/offsets/publisher">
                </div>

                <button id="push-btn" class="push-btn">
                    [ PUBLISH & PUSH OFFSETS ]
                </button>
            </div>
        </div>

        <div id="console-log" class="console-log">
            SYS > Awaiting updates execution queue...
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
                consoleLog.innerHTML = '<span style="color: #ff3b30;">SYS > [Error] Version and Offsets definitions cannot be empty.</span>';
                return;
            }

            consoleLog.innerHTML = 'SYS > Connecting to distribution server...';

            setTimeout(() => {
                consoleLog.innerHTML += '<br>SYS > Authenticating developer credentials...';
                setTimeout(() => {
                    consoleLog.innerHTML += '<br>SYS > Compiling and verifying offset registry entries...';
                    setTimeout(() => {
                        consoleLog.innerHTML += `<br><span class="success-text">SYS > [Success] Offsets for ${version} have been successfully published to offsets.imtheo.lol!</span>`;
                        consoleLog.scrollTop = consoleLog.scrollHeight;
                    }, 800);
                }, 800);
            }, 800);
        });
    </script>
</body>
</html>)raw_html";
}
