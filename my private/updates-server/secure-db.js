const fs = require('fs');
const path = require('path');
const crypto = require('crypto');


const DB_MASTER_KEY = crypto.createHash('sha256').update('TUNG-WARE-DB-SUPER-SECRET-KEY-2026').digest();
const DB_FILE = path.join(__dirname, 'user_data.enc');


function encryptData(dataObject) {
    const rawText = JSON.stringify(dataObject);
    const iv = crypto.randomBytes(12); 
    const cipher = crypto.createCipheriv('aes-256-gcm', DB_MASTER_KEY, iv);
    
    let encrypted = cipher.update(rawText, 'utf8', 'hex');
    encrypted += cipher.final('hex');
    const authTag = cipher.getAuthTag().toString('hex');
    
    
    return JSON.stringify({
        iv: iv.toString('hex'),
        authTag: authTag,
        content: encrypted
    });
}


function decryptData(encryptedJson) {
    try {
        const payload = JSON.parse(encryptedJson);
        const iv = Buffer.from(payload.iv, 'hex');
        const authTag = Buffer.from(payload.authTag, 'hex');
        const decipher = crypto.createDecipheriv('aes-256-gcm', DB_MASTER_KEY, iv);
        decipher.setAuthTag(authTag);
        
        let decrypted = decipher.update(payload.content, 'hex', 'utf8');
        decrypted += decipher.final('utf8');
        return JSON.parse(decrypted);
    } catch (err) {
        
        console.error("[SECURE-DB] Decryption failed or database file is corrupted.", err.message);
        return { users: {}, licenseKeys: {} };
    }
}


function loadDatabase() {
    if (!fs.existsSync(DB_FILE)) {
        return { users: {}, licenseKeys: {} };
    }
    const encryptedContent = fs.readFileSync(DB_FILE, 'utf8');
    return decryptData(encryptedContent);
}


function saveDatabase(db) {
    const encryptedContent = encryptData(db);
    fs.writeFileSync(DB_FILE, encryptedContent, 'utf8');
}


function hashPassword(password) {
    const salt = crypto.randomBytes(16).toString('hex');
    const derivedKey = crypto.scryptSync(password, salt, 64);
    return `${salt}:${derivedKey.toString('hex')}`;
}


function verifyPassword(password, storedHash) {
    const [salt, hash] = storedHash.split(':');
    const derivedKey = crypto.scryptSync(password, salt, 64);
    return crypto.timingSafeEqual(Buffer.from(hash, 'hex'), derivedKey);
}





const SecureDB = {
    
    createUser(username, password, licenseKey) {
        const db = loadDatabase();
        const normalizedUser = username.toLowerCase().trim();

        if (db.users[normalizedUser]) {
            return { success: false, error: 'User already exists.' };
        }

        if (!licenseKey) {
            return { success: false, error: 'A valid license key is required to register.' };
        }

        const keyData = db.licenseKeys[licenseKey];
        if (!keyData) {
            return { success: false, error: 'Invalid license key.' };
        }
        if (keyData.usedBy) {
            return { success: false, error: 'License key has already been used.' };
        }

        
        let newExpiry;
        if (keyData.durationHours === -1) {
            newExpiry = new Date('2099-12-31T23:59:59.000Z');
        } else {
            newExpiry = new Date(Date.now() + keyData.durationHours * 60 * 60 * 1000);
        }

        db.users[normalizedUser] = {
            username: username,
            passwordHash: hashPassword(password),
            licenseExpiry: newExpiry.toISOString(),
            hwid: null,
            createdAt: new Date().toISOString()
        };

        keyData.usedBy = normalizedUser;
        keyData.activatedAt = new Date().toISOString();

        saveDatabase(db);
        return { success: true, message: 'Account created and key activated successfully.' };
    },

    
    authenticateUser(username, password, hwid = null) {
        const db = loadDatabase();
        const normalizedUser = username.toLowerCase().trim();
        const user = db.users[normalizedUser];

        if (!user) {
            return { success: false, error: 'Invalid username or password.' };
        }

        if (!verifyPassword(password, user.passwordHash)) {
            return { success: false, error: 'Invalid username or password.' };
        }

        
        if (!user.licenseExpiry) {
            return { success: false, error: 'No active license. Please purchase or activate a key.' };
        }

        const now = Date.now();
        const expiry = new Date(user.licenseExpiry).getTime();
        if (now > expiry) {
            return { success: false, error: 'License key has expired.' };
        }

        
        if (hwid) {
            if (!user.hwid) {
                user.hwid = hwid;
                saveDatabase(db);
            } else if (user.hwid !== hwid) {
                return { success: false, error: 'HWID mismatch. Reset required.' };
            }
        }

        const timeLeftSeconds = Math.max(0, Math.floor((expiry - now) / 1000));
        return {
            success: true,
            username: user.username,
            timeLeftSeconds: timeLeftSeconds,
            expiryDate: user.licenseExpiry
        };
    },

    
    generateLicenseKey(durationHours = 24) {
        const db = loadDatabase();
        
        const segments = [];
        for (let i = 0; i < 3; i++) {
            segments.push(crypto.randomBytes(2).toString('hex').toUpperCase());
        }
        const key = `TUNG-${segments.join('-')}`;

        db.licenseKeys[key] = {
            durationHours: durationHours,
            usedBy: null,
            activatedAt: null,
            createdAt: new Date().toISOString()
        };

        saveDatabase(db);
        return { success: true, key: key, durationHours: durationHours };
    },

    
    activateLicense(username, licenseKey) {
        const db = loadDatabase();
        const normalizedUser = username.toLowerCase().trim();
        const user = db.users[normalizedUser];
        const keyData = db.licenseKeys[licenseKey];

        if (!user) {
            return { success: false, error: 'User does not exist.' };
        }
        if (!keyData) {
            return { success: false, error: 'Invalid license key.' };
        }
        if (keyData.usedBy) {
            return { success: false, error: 'License key has already been activated.' };
        }

        
        let currentExpiry = user.licenseExpiry ? new Date(user.licenseExpiry).getTime() : Date.now();
        if (currentExpiry < Date.now()) {
            currentExpiry = Date.now(); 
        }

        let newExpiry;
        if (keyData.durationHours === -1) {
            
            newExpiry = new Date('2099-12-31T23:59:59.000Z');
        } else {
            newExpiry = new Date(currentExpiry + keyData.durationHours * 60 * 60 * 1000);
        }

        user.licenseExpiry = newExpiry.toISOString();
        keyData.usedBy = normalizedUser;
        keyData.activatedAt = new Date().toISOString();

        saveDatabase(db);
        return {
            success: true,
            expiryDate: user.licenseExpiry,
            message: `Key activated successfully. Expiration: ${user.licenseExpiry}`
        };
    },

    
    resetHwid(username) {
        const db = loadDatabase();
        const normalizedUser = username.toLowerCase().trim();
        const user = db.users[normalizedUser];

        if (!user) {
            return { success: false, error: 'User does not exist.' };
        }

        user.hwid = null;
        saveDatabase(db);
        return { success: true, message: 'HWID reset successfully.' };
    },

    
    getRawDatabase() {
        return loadDatabase();
    },

    saveRawDatabase(db) {
        saveDatabase(db);
    }
};

module.exports = SecureDB;
