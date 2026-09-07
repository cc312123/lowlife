// ============================================================
// TUNG-WARE KEY SYSTEM - SERVER ENTRY POINT
// ============================================================
'use strict';

const express      = require('express');
const cors         = require('cors');
const helmet       = require('helmet');
const rateLimit    = require('express-rate-limit');
const crypto       = require('crypto');
const path         = require('path');
const KeyDatabase  = require('./database');

const app = express();
const db  = new KeyDatabase(path.join(__dirname, 'keys.json'));

// ── Security Hardening ───────────────────────────────────────
app.use(helmet({
    contentSecurityPolicy: false,
    crossOriginEmbedderPolicy: false
}));

app.use(cors({
    origin: process.env.ALLOWED_ORIGIN || '*',
    methods: ['POST'],
    allowedHeaders: ['Content-Type', 'X-Auth-Token']
}));

app.use(express.json({ limit: '4kb' }));

// ── Server-side secret (loaded once at startup) ───────────────
const SERVER_SECRET = process.env.KEY_SERVER_SECRET || (() => {
    // If no env var, generate a persistent secret written to disk
    const secretFile = path.join(__dirname, '.server_secret');
    const fs = require('fs');
    if (fs.existsSync(secretFile)) {
        return fs.readFileSync(secretFile, 'utf8').trim();
    }
    const s = crypto.randomBytes(48).toString('hex');
    fs.writeFileSync(secretFile, s, { mode: 0o600 });
    return s;
})();

// ── Rate Limiter (global) ────────────────────────────────────
const globalLimiter = rateLimit({
    windowMs : 60 * 1000,   // 1 min window
    max      : 20,
    message  : { ok: false, code: 'RATE_LIMITED', message: 'Too many requests.' }
});

// ── Rate Limiter (verify endpoint – stricter) ────────────────
const verifyLimiter = rateLimit({
    windowMs : 60 * 1000,
    max      : 5,
    message  : { ok: false, code: 'RATE_LIMITED', message: 'Too many validation attempts.' }
});

app.use(globalLimiter);

// ── Admin token middleware ───────────────────────────────────
function requireAdmin(req, res, next) {
    const token = req.headers['x-auth-token'] || '';
    const expected = process.env.ADMIN_TOKEN || '';
    if (!expected || !crypto.timingSafeEqual(
            Buffer.from(token.padEnd(64, '\0')),
            Buffer.from(expected.padEnd(64, '\0'))
        )) {
        return res.status(403).json({ ok: false, code: 'FORBIDDEN', message: 'Invalid admin token.' });
    }
    next();
}

// ── Utility: build HMAC signed session token ────────────────
function signPayload(payload) {
    const str = JSON.stringify(payload);
    const sig = crypto.createHmac('sha256', SERVER_SECRET)
                      .update(str)
                      .digest('hex');
    return Buffer.from(JSON.stringify({ payload, sig })).toString('base64url');
}

// ── POST /api/v1/verify ──────────────────────────────────────
// Body: { key: string, hwid: string }
app.post('/api/v1/verify', verifyLimiter, (req, res) => {
    try {
        const { key, hwid } = req.body || {};

        // Basic input sanity
        if (typeof key  !== 'string' || key.trim().length  < 8 ||
            typeof hwid !== 'string' || hwid.trim().length < 4) {
            return res.status(400).json({ ok: false, code: 'BAD_REQUEST', message: 'Missing or invalid parameters.' });
        }

        const record = db.getKeyRecord(key.trim());
        if (!record) {
            return res.status(401).json({ ok: false, code: 'INVALID_KEY', message: 'Key not recognised.' });
        }

        if (record.status !== 'active') {
            return res.status(401).json({ ok: false, code: 'KEY_' + record.status.toUpperCase(), message: `Key is ${record.status}.` });
        }

        if (record.expiresAt && Date.now() > record.expiresAt) {
            record.status = 'expired';
            db.save();
            return res.status(401).json({ ok: false, code: 'KEY_EXPIRED', message: 'Key has expired.' });
        }

        const hwidHash = crypto.createHash('sha256').update(hwid.trim()).digest('hex');
        const bindResult = db.bindHwid(record, hwidHash);
        if (!bindResult.success) {
            return res.status(401).json({ ok: false, code: 'HWID_MISMATCH', message: 'Max HWID limit reached for this key.' });
        }

        const sessionToken = signPayload({
            tier      : record.tier,
            keyHash   : record.keyHash,
            hwidHash  : hwidHash,
            issuedAt  : Date.now(),
            expiresAt : Date.now() + 6 * 60 * 60 * 1000  // 6-hour session
        });

        return res.json({
            ok           : true,
            code         : 'VALID',
            tier         : record.tier,
            sessionToken : sessionToken,
            expiresIn    : 6 * 60 * 60
        });
    } catch (err) {
        console.error('[verify]', err);
        return res.status(500).json({ ok: false, code: 'INTERNAL_ERROR', message: 'Internal server error.' });
    }
});

// ── POST /api/v1/validate-session ───────────────────────────
// Validates a session token issued by /verify
app.post('/api/v1/validate-session', (req, res) => {
    try {
        const { sessionToken } = req.body || {};
        if (typeof sessionToken !== 'string') {
            return res.status(400).json({ ok: false, code: 'BAD_REQUEST' });
        }

        let decoded;
        try {
            decoded = JSON.parse(Buffer.from(sessionToken, 'base64url').toString('utf8'));
        } catch {
            return res.status(401).json({ ok: false, code: 'INVALID_TOKEN' });
        }

        const { payload, sig } = decoded;
        const expectedSig = crypto.createHmac('sha256', SERVER_SECRET)
                                  .update(JSON.stringify(payload))
                                  .digest('hex');

        if (!crypto.timingSafeEqual(Buffer.from(sig, 'hex'), Buffer.from(expectedSig, 'hex'))) {
            return res.status(401).json({ ok: false, code: 'TAMPERED_TOKEN' });
        }

        if (Date.now() > payload.expiresAt) {
            return res.status(401).json({ ok: false, code: 'SESSION_EXPIRED' });
        }

        return res.json({ ok: true, code: 'VALID', tier: payload.tier, expiresAt: payload.expiresAt });
    } catch (err) {
        console.error('[validate-session]', err);
        return res.status(500).json({ ok: false, code: 'INTERNAL_ERROR' });
    }
});

// ── POST /api/v1/keys/create (admin) ────────────────────────
app.post('/api/v1/keys/create', requireAdmin, (req, res) => {
    try {
        const { durationDays = 30, maxHwids = 1, tier = 'standard' } = req.body || {};
        const record = db.generateKey(Number(durationDays), Number(maxHwids), String(tier));
        return res.json({ ok: true, key: record.rawKey, keyHash: record.keyHash, expiresAt: record.expiresAt });
    } catch (err) {
        console.error('[keys/create]', err);
        return res.status(500).json({ ok: false, code: 'INTERNAL_ERROR' });
    }
});

// ── POST /api/v1/keys/revoke (admin) ────────────────────────
app.post('/api/v1/keys/revoke', requireAdmin, (req, res) => {
    try {
        const { key } = req.body || {};
        if (typeof key !== 'string') {
            return res.status(400).json({ ok: false, code: 'BAD_REQUEST', message: 'key is required.' });
        }
        const result = db.revokeKey(key.trim());
        return res.json({ ok: result, code: result ? 'REVOKED' : 'NOT_FOUND' });
    } catch (err) {
        console.error('[keys/revoke]', err);
        return res.status(500).json({ ok: false, code: 'INTERNAL_ERROR' });
    }
});

// ── POST /api/v1/keys/list (admin) ──────────────────────────
app.post('/api/v1/keys/list', requireAdmin, (req, res) => {
    try {
        const all = Object.values(db.data.keys).map(r => ({
            rawKey    : r.rawKey,
            tier      : r.tier,
            status    : r.status,
            createdAt : r.createdAt,
            expiresAt : r.expiresAt,
            hwids     : r.hwids.length,
            maxHwids  : r.maxHwids
        }));
        return res.json({ ok: true, total: all.length, keys: all });
    } catch (err) {
        return res.status(500).json({ ok: false, code: 'INTERNAL_ERROR' });
    }
});

// ── Health check ─────────────────────────────────────────────
app.get('/health', (_req, res) => res.json({ ok: true, ts: Date.now() }));

// ── 404 catch-all ───────────────────────────────────────────
app.use((_req, res) => res.status(404).json({ ok: false, code: 'NOT_FOUND' }));

// ── Start ────────────────────────────────────────────────────
const PORT = process.env.PORT || 3747;
app.listen(PORT, '0.0.0.0', () => {
    console.log(`[KEY-SYSTEM] Server listening on port ${PORT}`);
});
