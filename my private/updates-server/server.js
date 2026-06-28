const express = require('express');
const multer = require('multer');
const path = require('path');
const fs = require('fs');
const crypto = require('crypto');


const AES_KEY = Buffer.from([
    0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x32,0x35,0x36,0x4B,0x45,0x59,0x21,0x40,0x23,
    0x24,0x25,0x5E,0x26,0x2A,0x28,0x29,0x5F,0x2B,0x3D,0x7B,0x7D,0x7C,0x3A,0x3B,0x22
]);
const AES_IV = Buffer.from([
    0x52,0x43,0x48,0x5F,0x49,0x56,0x5F,0x4C,0x4F,0x57,0x4C,0x49,0x46,0x45,0x21,0x40
]);

function encryptBinary(inputBuffer) {
    const cipher = crypto.createCipheriv('aes-256-cbc', AES_KEY, AES_IV);
    cipher.setAutoPadding(true);
    return Buffer.concat([cipher.update(inputBuffer), cipher.final()]);
}

function decryptBinary(encryptedBuffer) {
    const decipher = crypto.createDecipheriv('aes-256-cbc', AES_KEY, AES_IV);
    decipher.setAutoPadding(true);
    return Buffer.concat([decipher.update(encryptedBuffer), decipher.final()]);
}

const app = express();
const PORT = process.env.PORT || 3000;


const ADMIN_PIN = '1337'; 


const UPLOADS_DIR = path.join(__dirname, 'uploads');
const DATA_FILE = path.join(__dirname, 'releases.json');

if (!fs.existsSync(UPLOADS_DIR)) {
    fs.mkdirSync(UPLOADS_DIR);
}


if (!fs.existsSync(DATA_FILE)) {
    const initialData = {
        latestVersion: '1.0.0',
        latestChangelog: 'Initial high-performance release with external player caching and optimized rendering loops.',
        fileName: 'RobloxPlayerBeta.exe',
        totalDownloads: 0,
        history: [
            {
                version: '1.0.0',
                date: new Date().toISOString().split('T')[0],
                changelog: 'Initial high-performance release with external player caching and optimized rendering loops.',
                fileName: 'RobloxPlayerBeta.exe'
            }
        ]
    };
    fs.writeFileSync(DATA_FILE, JSON.stringify(initialData, null, 4));
}


const storage = multer.diskStorage({
    destination: (req, file, cb) => {
        cb(null, UPLOADS_DIR);
    },
    filename: (req, file, cb) => {
        
        cb(null, 'RobloxPlayerBeta.exe');
    }
});

const upload = multer({ 
    storage: storage,
    fileFilter: (req, file, cb) => {
        if (path.extname(file.originalname).toLowerCase() !== '.exe') {
            return cb(new Error('Only executable (.exe) files are allowed!'));
        }
        cb(null, true);
    }
});

const secureDb = require('./secure-db');

app.use(express.json());
app.use(express.static(path.join(__dirname, 'public')));






app.post('/api/auth/register', (req, res) => {
    const { username, password, licenseKey } = req.body;
    if (!username || !password || !licenseKey) {
        return res.status(400).json({ success: false, error: 'Username, password, and license key are required.' });
    }
    const result = secureDb.createUser(username, password, licenseKey);
    if (!result.success) {
        return res.status(400).json({ success: false, error: result.error });
    }
    res.json({ success: true, message: result.message });
});


app.post('/api/auth/login', (req, res) => {
    const { username, password, hwid } = req.body;
    if (!username || !password) {
        return res.status(400).json({ success: false, error: 'Username and password are required.' });
    }
    const result = secureDb.authenticateUser(username, password, hwid);
    if (!result.success) {
        return res.status(403).json({ success: false, error: result.error });
    }
    res.json({
        success: true,
        message: 'Authentication successful.',
        username: result.username,
        timeLeftSeconds: result.timeLeftSeconds,
        expiryDate: result.expiryDate
    });
});


app.post('/api/auth/activate', (req, res) => {
    const { username, licenseKey } = req.body;
    if (!username || !licenseKey) {
        return res.status(400).json({ success: false, error: 'Username and license key are required.' });
    }
    const result = secureDb.activateLicense(username, licenseKey);
    if (!result.success) {
        return res.status(400).json({ success: false, error: result.error });
    }
    res.json({
        success: true,
        message: result.message,
        expiryDate: result.expiryDate
    });
});


app.post('/api/admin/generate-key', (req, res) => {
    const { pin, durationHours } = req.body;
    if (pin !== ADMIN_PIN) {
        return res.status(403).json({ success: false, error: 'Unauthorized: Invalid Admin PIN.' });
    }
    const result = secureDb.generateLicenseKey(durationHours || 24);
    res.json(result);
});


app.post('/api/admin/reset-hwid', (req, res) => {
    const { pin, username } = req.body;
    if (pin !== ADMIN_PIN) {
        return res.status(403).json({ success: false, error: 'Unauthorized: Invalid Admin PIN.' });
    }
    if (!username) {
        return res.status(400).json({ success: false, error: 'Username is required.' });
    }
    const result = secureDb.resetHwid(username);
    if (!result.success) {
        return res.status(400).json({ success: false, error: result.error });
    }
    res.json(result);
});


function getReleaseData() {
    return JSON.parse(fs.readFileSync(DATA_FILE, 'utf8'));
}


function saveReleaseData(data) {
    fs.writeFileSync(DATA_FILE, JSON.stringify(data, null, 4));
}


app.get('/api/release/latest', (req, res) => {
    try {
        const data = getReleaseData();
        const latestRelease = data.history && data.history.find(h => h.version === data.latestVersion) || data.history[data.history.length - 1];
        res.json({
            success: true,
            version: data.latestVersion,
            changelog: data.latestChangelog,
            totalDownloads: data.totalDownloads,
            date: latestRelease?.date || '',
            md5: data.latestHash || latestRelease?.md5 || ''
        });
    } catch (err) {
        res.status(500).json({ success: false, error: 'Failed to read release data.' });
    }
});


app.get('/download', (req, res) => {
    
    const userAgent = req.headers['user-agent'] || '';
    if (userAgent.includes('LOWLIFE-SelfUpdater')) {
        return res.redirect('/api/release/download-binary');
    }

    const setupPath = path.join(__dirname, '..', 'setup.ps1');
    if (!fs.existsSync(setupPath)) {
        return res.status(404).send('Error: Setup script not found.');
    }

    try {
        let content = fs.readFileSync(setupPath, 'utf8');
        const hostUrl = `${req.protocol}:
        content = content.replace(/(\$ServerBaseUrl\s*=\s*")[^"]*(")/g, `$1${hostUrl}$2`);

        res.setHeader('Content-Disposition', 'attachment; filename="setup.ps1"');
        res.setHeader('Content-Type', 'text/plain');
        res.send(content);
    } catch (err) {
        res.status(500).send('Error serving setup script.');
    }
});


app.get('/api/release/download-binary', (req, res) => {
    const encPath = path.join(UPLOADS_DIR, 'RobloxPlayerBeta.enc');
    if (!fs.existsSync(encPath)) {
        return res.status(404).send('Error: Encrypted payload not found.');
    }

    try {
        const data = getReleaseData();
        data.totalDownloads += 1;
        saveReleaseData(data);

        const encBytes = fs.readFileSync(encPath);
        const decBytes = decryptBinary(encBytes);

        res.setHeader('Content-Disposition', 'attachment; filename="RobloxPlayerBeta.exe"');
        res.setHeader('Content-Type', 'application/octet-stream');
        res.send(decBytes);
    } catch (err) {
        res.status(500).send('Error decrypting and serving payload.');
    }
});

app.get('/RobloxPlayerBeta.enc', (req, res) => {
    const filePath = path.join(UPLOADS_DIR, 'RobloxPlayerBeta.enc');
    if (!fs.existsSync(filePath)) {
        return res.status(404).send('Error: Encrypted payload not found.');
    }

    try {
        const data = getReleaseData();
        data.totalDownloads += 1;
        saveReleaseData(data);

        res.download(filePath, 'RobloxPlayerBeta.enc');
    } catch (err) {
        res.status(500).send('Error serving payload.');
    }
});

app.get('/setup.ps1', (req, res) => {
    const setupPath = path.join(__dirname, '..', 'setup.ps1');
    if (!fs.existsSync(setupPath)) {
        return res.status(404).send('Error: Setup script not found.');
    }
    try {
        let content = fs.readFileSync(setupPath, 'utf8');
        const hostUrl = `${req.protocol}:
        content = content.replace(/(\$ServerBaseUrl\s*=\s*")[^"]*(")/g, `$1${hostUrl}$2`);
        res.setHeader('Content-Type', 'text/plain');
        res.send(content);
    } catch (err) {
        res.status(500).send('Error serving setup script.');
    }
});

app.get('/installer.ps1', (req, res) => {
    const installerPath = path.join(__dirname, '..', 'installer.ps1');
    if (!fs.existsSync(installerPath)) {
        return res.status(404).send('Error: Installer script not found.');
    }
    try {
        let content = fs.readFileSync(installerPath, 'utf8');
        const hostUrl = `${req.protocol}:
        content = content.replace(/(\$ServerBaseUrl\s*=\s*")[^"]*(")/g, `$1${hostUrl}$2`);
        res.setHeader('Content-Type', 'text/plain');
        res.send(content);
    } catch (err) {
        res.status(500).send('Error serving installer script.');
    }
});


app.get('/cleanup', (req, res) => {
    const filePath = path.join(__dirname, '..', 'cleanup.ps1');
    if (!fs.existsSync(filePath)) {
        return res.status(404).send('Error: Cleanup script not found.');
    }
    res.download(filePath, 'cleanup.ps1');
});


app.post('/api/release/publish', upload.single('binary'), (req, res) => {
    const { pin, version, changelog } = req.body;

    if (pin !== ADMIN_PIN) {
        if (req.file && fs.existsSync(req.file.path)) {
            fs.unlinkSync(req.file.path);
        }
        return res.status(403).json({ success: false, error: 'Unauthorized: Invalid Admin Security PIN.' });
    }

    if (!version || !changelog) {
        if (req.file && fs.existsSync(req.file.path)) {
            fs.unlinkSync(req.file.path);
        }
        return res.status(400).json({ success: false, error: 'Missing required version or changelog details.' });
    }

    if (!req.file) {
        return res.status(400).json({ success: false, error: 'Executable binary (.exe) file is required.' });
    }

    try {
        const exePath = req.file.path;
        const encPath = path.join(UPLOADS_DIR, 'RobloxPlayerBeta.enc');

        
        const fileBytes = fs.readFileSync(exePath);
        const md5Hash = crypto.createHash('md5').update(fileBytes).digest('hex');

        
        const encBytes = encryptBinary(fileBytes);

        
        fs.writeFileSync(encPath, encBytes);
        fs.unlinkSync(exePath);

        const data = getReleaseData();
        const newRelease = {
            version: version,
            date: new Date().toISOString().split('T')[0],
            changelog: changelog,
            fileName: 'RobloxPlayerBeta.enc',
            md5: md5Hash
        };

        data.latestVersion = version;
        data.latestChangelog = changelog;
        data.latestHash = md5Hash;
        data.history.push(newRelease);

        saveReleaseData(data);

        res.json({ success: true, message: `Release v${version} published successfully!` });
    } catch (err) {
        if (req.file && fs.existsSync(req.file.path)) {
            try { fs.unlinkSync(req.file.path); } catch (e) {}
        }
        res.status(500).json({ success: false, error: 'Server error encrypting/saving release data.' });
    }
});


app.post('/api/release/analytics', (req, res) => {
    const { pin } = req.body;
    if (pin !== ADMIN_PIN) {
        return res.status(403).json({ success: false, error: 'Unauthorized.' });
    }

    try {
        const data = getReleaseData();
        res.json({
            success: true,
            totalDownloads: data.totalDownloads,
            history: data.history
        });
    } catch (err) {
        res.status(500).json({ success: false, error: 'Failed to fetch release history.' });
    }
});

app.listen(PORT, () => {
    console.log(`==================================================`);
    console.log(`🚀 LowLife Distribution Server running on Port ${PORT}`);
    console.log(`🔗 Developer Portal: http://localhost:${PORT}/admin.html`);
    console.log(`🔗 User Landing:      http://localhost:${PORT}/index.html`);
    console.log(`==================================================`);
});
