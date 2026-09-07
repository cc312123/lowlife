// ============================================================
//  TUNG-WARE KEY SYSTEM - WEB CLIENT INTEGRATION
//  Drop this <script> block into index.html to wire the 
//  authentication gate into your existing UI.
//  The XOR obfuscation layer is applied at the end so that
//  the server URL and logic are not plaintext in the HTML.
// ============================================================

(function () {
    'use strict';

    // ── Obfuscated config (XOR encoded at build time) ─────────
    // To regenerate: node scripts/encode-config.js
    const _OBF_KEY = 'TW_SECURE_RUNTIME_2026';
    const _OBF_CFG = 'PLACEHOLDER_ENCODED_CONFIG'; // replaced by build pipeline

    function xorDecode(encoded, key) {
        const b = atob(encoded);
        let r = '';
        for (let i = 0; i < b.length; i++) {
            r += String.fromCharCode(b.charCodeAt(i) ^ key.charCodeAt(i % key.length));
        }
        return r;
    }

    // Runtime config decoded from obfuscated string
    let _cfg;
    try {
        _cfg = JSON.parse(xorDecode(_OBF_CFG, _OBF_KEY));
    } catch {
        // Fallback for development mode
        _cfg = { endpoint: 'http://127.0.0.1:3747', timeout: 8000 };
    }

    // ── Hardware fingerprint (browser) ───────────────────────
    function buildBrowserHwid() {
        const parts = [
            navigator.userAgent,
            screen.width + 'x' + screen.height,
            navigator.language,
            navigator.hardwareConcurrency || '?',
            Intl.DateTimeFormat().resolvedOptions().timeZone
        ];
        let hash = 0;
        const str = parts.join('|');
        for (let i = 0; i < str.length; i++) {
            const ch = str.charCodeAt(i);
            hash = ((hash << 5) - hash) + ch;
            hash |= 0;
        }
        return Math.abs(hash).toString(16).padStart(8, '0') + '-' + str.length.toString(16);
    }

    // ── API call helper ──────────────────────────────────────
    async function apiPost(path, body) {
        const controller = new AbortController();
        const tid = setTimeout(() => controller.abort(), _cfg.timeout);
        try {
            const resp = await fetch(_cfg.endpoint + path, {
                method  : 'POST',
                headers : { 'Content-Type': 'application/json' },
                body    : JSON.stringify(body),
                signal  : controller.signal
            });
            return await resp.json();
        } finally {
            clearTimeout(tid);
        }
    }

    // ── Session cache (sessionStorage) ───────────────────────
    const SESSION_KEY = '_tw_sess';

    function saveSession(token, expiresIn) {
        sessionStorage.setItem(SESSION_KEY, JSON.stringify({
            token,
            expiresAt: Date.now() + (expiresIn - 300) * 1000
        }));
    }

    function getSession() {
        try {
            const d = JSON.parse(sessionStorage.getItem(SESSION_KEY));
            if (d && Date.now() < d.expiresAt) return d;
        } catch { /**/ }
        return null;
    }

    // ── Core verify function ─────────────────────────────────
    async function verifyKey(key) {
        const existing = getSession();
        if (existing) return { ok: true, cached: true, sessionToken: existing.token };

        const hwid = buildBrowserHwid();
        const resp = await apiPost('/api/v1/verify', { key, hwid });

        if (resp.ok) {
            saveSession(resp.sessionToken, resp.expiresIn);
            return { ok: true, tier: resp.tier, sessionToken: resp.sessionToken };
        }

        const msg = {
            INVALID_KEY   : 'Invalid key.',
            KEY_REVOKED   : 'Key has been revoked.',
            KEY_EXPIRED   : 'Key has expired.',
            HWID_MISMATCH : 'Key is locked to another device.',
            RATE_LIMITED  : 'Too many attempts. Please wait.',
        }[resp.code] || 'Authentication failed.';

        throw new Error(msg);
    }

    // ── UI Integration ───────────────────────────────────────
    // Hooks into the existing auth-gate-card UI already in index.html
    function showError(msg) {
        let el = document.getElementById('key-error-msg');
        if (!el) {
            el = document.createElement('div');
            el.id = 'key-error-msg';
            el.style.cssText = 'color:#ff4d6d;font-size:12px;margin-top:8px;text-align:center;font-family:monospace;';
            const gate = document.getElementById('auth-gate-card');
            if (gate) gate.appendChild(el);
        }
        el.textContent = '⚠ ' + msg;
    }

    function clearError() {
        const el = document.getElementById('key-error-msg');
        if (el) el.textContent = '';
    }

    function setLoading(btn, state) {
        if (!btn) return;
        btn.disabled  = state;
        btn.textContent = state ? 'VERIFYING...' : 'VERIFY KEY';
    }

    // Called when user submits key in the UI
    async function handleKeySubmit() {
        const input = document.getElementById('gate-key-input') || document.getElementById('gate-login-user');
        if (!input) return;
        const key = input.value.trim();
        if (!key) { showError('Please enter your key.'); return; }

        const btn = document.getElementById('btn-key-verify') || document.getElementById('btn-login');
        clearError();
        setLoading(btn, true);

        try {
            const result = await verifyKey(key);
            if (result.ok) {
                // Dispatch custom event so the rest of the app knows auth passed
                document.dispatchEvent(new CustomEvent('tw:auth:success', {
                    detail: { tier: result.tier, token: result.sessionToken }
                }));
                // Hide the gate
                const gate = document.getElementById('auth-gate-card');
                if (gate) gate.style.display = 'none';
            }
        } catch (err) {
            showError(err.message);
        } finally {
            setLoading(btn, false);
        }
    }

    // ── Auto-wire on DOM ready ───────────────────────────────
    function wireUI() {
        const btn = document.getElementById('btn-key-verify') || document.getElementById('btn-login');
        if (btn) {
            btn.addEventListener('click', handleKeySubmit);
        }

        // Allow Enter key inside input to submit
        const input = document.getElementById('gate-key-input') || document.getElementById('gate-login-user');
        if (input) {
            input.addEventListener('keydown', (e) => {
                if (e.key === 'Enter') handleKeySubmit();
            });
        }
    }

    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', wireUI);
    } else {
        wireUI();
    }

    // Expose for advanced integration
    window.__twKeyClient = { verifyKey, buildBrowserHwid, getSession };
})();
