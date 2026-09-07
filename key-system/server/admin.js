#!/usr/bin/env node
// ============================================================
// TUNG-WARE KEY SYSTEM - ADMIN CLI
// Usage: node admin.js <command> [options]
//   node admin.js create --days 30 --hwids 1 --tier standard
//   node admin.js revoke --key KEY-XXXX-XXXX-XXXX
//   node admin.js list
// ============================================================
'use strict';

const https  = require('https');
const http   = require('http');
const crypto = require('crypto');

const SERVER_URL   = process.env.KEY_SERVER_URL  || 'http://127.0.0.1:3747';
const ADMIN_TOKEN  = process.env.ADMIN_TOKEN      || '';

if (!ADMIN_TOKEN) {
    console.error('[ERROR] Set the ADMIN_TOKEN environment variable before using the admin CLI.');
    process.exit(1);
}

function request(path, body) {
    return new Promise((resolve, reject) => {
        const data = JSON.stringify(body);
        const url  = new URL(path, SERVER_URL);
        const lib  = url.protocol === 'https:' ? https : http;

        const options = {
            hostname : url.hostname,
            port     : url.port || (url.protocol === 'https:' ? 443 : 80),
            path     : url.pathname,
            method   : 'POST',
            headers  : {
                'Content-Type'  : 'application/json',
                'Content-Length': Buffer.byteLength(data),
                'X-Auth-Token'  : ADMIN_TOKEN
            }
        };

        const req = lib.request(options, (res) => {
            let raw = '';
            res.on('data', (chunk) => raw += chunk);
            res.on('end', () => {
                try { resolve(JSON.parse(raw)); }
                catch { reject(new Error('Failed to parse server response.')); }
            });
        });

        req.on('error', reject);
        req.write(data);
        req.end();
    });
}

function parseArgs(argv) {
    const args = {};
    for (let i = 0; i < argv.length; i++) {
        if (argv[i].startsWith('--')) {
            const k = argv[i].slice(2);
            args[k] = argv[i + 1] && !argv[i + 1].startsWith('--') ? argv[++i] : true;
        }
    }
    return args;
}

async function main() {
    const [,, command, ...rest] = process.argv;
    const args = parseArgs(rest);

    switch (command) {
        case 'create': {
            const resp = await request('/api/v1/keys/create', {
                durationDays : Number(args.days  ?? 30),
                maxHwids     : Number(args.hwids ?? 1),
                tier         : String(args.tier  ?? 'standard')
            });
            if (resp.ok) {
                const exp = resp.expiresAt ? new Date(resp.expiresAt).toLocaleString() : 'never';
                console.log('\n✅  Key created successfully:');
                console.log(`   Key     : ${resp.key}`);
                console.log(`   Expires : ${exp}`);
                console.log(`   Tier    : ${args.tier ?? 'standard'}\n`);
            } else {
                console.error('[ERROR]', resp.code, resp.message);
            }
            break;
        }

        case 'revoke': {
            if (!args.key) { console.error('[ERROR] --key is required.'); process.exit(1); }
            const resp = await request('/api/v1/keys/revoke', { key: args.key });
            if (resp.ok) {
                console.log(`\n✅  Key revoked: ${args.key}\n`);
            } else {
                console.error('[ERROR]', resp.code, resp.message || 'Key not found.');
            }
            break;
        }

        case 'list': {
            const resp = await request('/api/v1/keys/list', {});
            if (resp.ok) {
                console.log(`\n📋  Total keys: ${resp.total}`);
                resp.keys.forEach(k => {
                    const exp = k.expiresAt ? new Date(k.expiresAt).toLocaleString() : 'lifetime';
                    const status = k.status === 'active' ? '🟢 active' : `🔴 ${k.status}`;
                    console.log(`   [${status}] ${k.rawKey}  tier=${k.tier}  hwids=${k.hwids}/${k.maxHwids}  exp=${exp}`);
                });
                console.log('');
            } else {
                console.error('[ERROR]', resp.code);
            }
            break;
        }

        default:
            console.log(`
TUNG-WARE Admin CLI
Usage:
  node admin.js create [--days 30] [--hwids 1] [--tier standard|premium|lifetime]
  node admin.js revoke --key KEY-XXXX-XXXX-XXXX
  node admin.js list
`);
    }
}

main().catch((err) => {
    console.error('[FATAL]', err.message);
    process.exit(1);
});
