// ============================================================
//  TUNG-WARE KEY SYSTEM - CLIENT VERIFICATION MODULE
//  This file is bundled into the client and talks to the server.
//  It is passed through the obfuscation pipeline before shipping.
// ============================================================
'use strict';

const crypto = require('crypto');
const https  = require('https');
const http   = require('http');
const os     = require('os');
const { execSync } = require('child_process');

// ── Config (replaced by build pipeline at compile time) ──────
const _C = {
    endpoint : process.env.KEY_SERVER_URL || 'http://127.0.0.1:3747',
    timeout  : 8000
};

// ── Hardware fingerprint ─────────────────────────────────────
function getHwidComponents() {
    const parts = [];

    // CPU info
    const cpus = os.cpus();
    if (cpus && cpus.length > 0) parts.push(cpus[0].model);

    // Hostname
    parts.push(os.hostname());

    // Network MAC addresses (first non-loopback)
    const ifaces = os.networkInterfaces();
    for (const name of Object.keys(ifaces)) {
        for (const iface of ifaces[name]) {
            if (!iface.internal && iface.mac && iface.mac !== '00:00:00:00:00:00') {
                parts.push(iface.mac);
                break;
            }
        }
        if (parts.length >= 3) break;
    }

    // Windows: BIOS UUID via WMIC (best unique identifier)
    if (process.platform === 'win32') {
        try {
            const uuid = execSync('wmic csproduct get uuid /value', { timeout: 3000 })
                .toString().match(/UUID=(.+)/i)?.[1]?.trim();
            if (uuid && uuid !== 'FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF') parts.push(uuid);
        } catch { /* ignore */ }
    }

    // Linux: machine-id
    if (process.platform === 'linux') {
        try {
            const id = require('fs').readFileSync('/etc/machine-id', 'utf8').trim();
            if (id) parts.push(id);
        } catch { /* ignore */ }
    }

    return parts;
}

function buildHwid() {
    const components = getHwidComponents().join('|');
    return crypto.createHash('sha256').update(components).digest('hex').slice(0, 32);
}

// ── HTTP request helper ──────────────────────────────────────
function post(path, body) {
    return new Promise((resolve, reject) => {
        const data = JSON.stringify(body);
        const url  = new URL(path, _C.endpoint);
        const lib  = url.protocol === 'https:' ? https : http;

        const timeout = setTimeout(() => {
            reject(new Error('Request timed out'));
        }, _C.timeout);

        const options = {
            hostname : url.hostname,
            port     : url.port || (url.protocol === 'https:' ? 443 : 80),
            path     : url.pathname,
            method   : 'POST',
            headers  : {
                'Content-Type'  : 'application/json',
                'Content-Length': Buffer.byteLength(data),
                'User-Agent'    : 'Mozilla/5.0 (compatible)'
            }
        };

        const req = lib.request(options, (res) => {
            clearTimeout(timeout);
            let raw = '';
            res.on('data', c => raw += c);
            res.on('end', () => {
                try { resolve(JSON.parse(raw)); }
                catch { reject(new Error('Malformed server response.')); }
            });
        });

        req.on('error', (e) => { clearTimeout(timeout); reject(e); });
        req.write(data);
        req.end();
    });
}

// ── Session cache ────────────────────────────────────────────
let _session = null;

function storeSession(token, expiresIn) {
    _session = { token, expiresAt: Date.now() + (expiresIn - 300) * 1000 };
}

function sessionValid() {
    return _session && Date.now() < _session.expiresAt;
}

// ── Public API ───────────────────────────────────────────────

/**
 * Verifies a key against the server.
 * Returns { ok, tier, sessionToken } on success.
 * Throws a descriptive Error on failure.
 */
async function verifyKey(key) {
    if (sessionValid()) {
        return { ok: true, tier: null, sessionToken: _session.token, cached: true };
    }

    const hwid = buildHwid();
    const resp = await post('/api/v1/verify', { key, hwid });

    if (!resp.ok) {
        const messages = {
            INVALID_KEY      : 'Invalid key. Please check your key and try again.',
            KEY_REVOKED      : 'This key has been revoked.',
            KEY_EXPIRED      : 'This key has expired.',
            HWID_MISMATCH    : 'This key is locked to another device.',
            RATE_LIMITED     : 'Too many attempts. Please wait and try again.',
        };
        throw new Error(messages[resp.code] || `Authentication failed (${resp.code}).`);
    }

    storeSession(resp.sessionToken, resp.expiresIn);
    return { ok: true, tier: resp.tier, sessionToken: resp.sessionToken };
}

/**
 * Re-validates a cached session token server-side.
 * Returns true if still valid.
 */
async function validateSession(sessionToken) {
    const resp = await post('/api/v1/validate-session', { sessionToken });
    return resp.ok === true;
}

module.exports = { verifyKey, validateSession, buildHwid };
