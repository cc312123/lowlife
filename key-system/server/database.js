const fs = require('fs');
const path = require('path');
const crypto = require('crypto');

class KeyDatabase {
    constructor(dbPath = path.join(__dirname, 'keys.json')) {
        this.dbPath = dbPath;
        this.data = { keys: {} };
        this.init();
    }

    init() {
        if (fs.existsSync(this.dbPath)) {
            try {
                const content = fs.readFileSync(this.dbPath, 'utf8');
                this.data = JSON.parse(content);
            } catch (err) {
                console.error('[DB] Failed to load DB, initializing new data structure:', err.message);
                this.save();
            }
        } else {
            this.save();
        }
    }

    save() {
        fs.writeFileSync(this.dbPath, JSON.stringify(this.data, null, 2), 'utf8');
    }

    generateKey(durationDays = 30, maxHwids = 1, tier = 'standard') {
        const rawKey = `KEY-${crypto.randomBytes(4).toString('hex').toUpperCase()}-${crypto.randomBytes(4).toString('hex').toUpperCase()}-${crypto.randomBytes(4).toString('hex').toUpperCase()}`;
        const keyHash = crypto.createHash('sha256').update(rawKey).digest('hex');
        
        const now = Date.now();
        const expiresAt = durationDays > 0 ? now + (durationDays * 24 * 60 * 60 * 1000) : null;

        const record = {
            rawKey: rawKey,
            keyHash: keyHash,
            createdAt: now,
            expiresAt: expiresAt,
            maxHwids: maxHwids,
            hwids: [],
            status: 'active',
            tier: tier
        };

        this.data.keys[keyHash] = record;
        this.save();
        return record;
    }

    getKeyRecord(keyString) {
        const keyHash = crypto.createHash('sha256').update(keyString.trim()).digest('hex');
        return this.data.keys[keyHash] || null;
    }

    bindHwid(keyRecord, hwid) {
        if (!keyRecord.hwids.includes(hwid)) {
            if (keyRecord.hwids.length >= keyRecord.maxHwids) {
                return { success: false, reason: 'HWID_LIMIT_REACHED' };
            }
            keyRecord.hwids.push(hwid);
            this.save();
        }
        return { success: true };
    }

    revokeKey(keyString) {
        const record = this.getKeyRecord(keyString);
        if (record) {
            record.status = 'revoked';
            this.save();
            return true;
        }
        return false;
    }
}

module.exports = KeyDatabase;
