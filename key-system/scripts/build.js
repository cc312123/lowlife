// ============================================================
//  TUNG-WARE KEY SYSTEM - OBFUSCATION BUILD PIPELINE
//  Run: node scripts/build.js
//  Produces:  dist/keyClient.web.min.js  (obfuscated)
//             dist/keyClient.web.obf.js  (deeply obfuscated)
// ============================================================
'use strict';

const fs     = require('fs');
const path   = require('path');
const crypto = require('crypto');

// ── Config that will be XOR-encoded into the client ──────────
const CLIENT_CONFIG = {
    endpoint : process.env.KEY_SERVER_URL || 'http://127.0.0.1:3747',
    timeout  : 8000
};

const OBF_KEY = 'TW_SECURE_RUNTIME_2026';

// ── XOR encode ───────────────────────────────────────────────
function xorEncode(input, key) {
    let r = '';
    for (let i = 0; i < input.length; i++) {
        r += String.fromCharCode(input.charCodeAt(i) ^ key.charCodeAt(i % key.length));
    }
    return Buffer.from(r, 'binary').toString('base64');
}

// ── String array obfuscation (poor-man's AST-free) ───────────
function obfuscateStrings(code) {
    // Collect all quoted strings, encode them into an array,
    // and replace references inline. Strings shorter than 4 chars are skipped.
    const strings = [];
    const encoded = code.replace(/'([^'\\]{4,})'/g, (match, s) => {
        if (s.startsWith('use strict') || s.startsWith('utf8')) return match;
        const idx = strings.length;
        strings.push(Buffer.from(s).toString('base64'));
        return `_0x${idx.toString(16).padStart(4,'0')}()`;
    });

    if (strings.length === 0) return code;

    const arrName = '_sa' + crypto.randomBytes(3).toString('hex');
    const fnName  = '_gs' + crypto.randomBytes(3).toString('hex');

    const header = `
const ${arrName}=${JSON.stringify(strings)};
function ${fnName}(i){return atob(${arrName}[i]);}
` + strings.map((_,i) => `function _0x${i.toString(16).padStart(4,'0')}(){return ${fnName}(${i});}`).join('\n');

    return header + '\n' + encoded;
}

// ── Identifier mangling (rename local vars) ──────────────────
function mangleIdentifiers(code) {
    const reserved = new Set([
        'function','const','let','var','return','if','else','for','while',
        'break','continue','new','this','null','undefined','true','false',
        'typeof','instanceof','in','of','class','try','catch','finally',
        'throw','async','await','import','export','default','switch','case',
        'document','window','fetch','Promise','Error','Date','Math',
        'JSON','String','Number','Boolean','Array','Object','Symbol',
        'console','setTimeout','clearTimeout','navigator','screen','Intl',
        'sessionStorage','CustomEvent','AbortController','parseInt','parseFloat',
        'Buffer','require','module','exports','process','crypto','atob','btoa'
    ]);

    const localVars = new Map();
    let counter = 0;

    function nextName() {
        const chars = 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ';
        let name = '_';
        let n = counter++;
        do {
            name += chars[n % chars.length];
            n = Math.floor(n / chars.length);
        } while (n > 0);
        return name;
    }

    // Replace simple const/let declarations (non-regex-safe, basic)
    return code.replace(/\b(const|let|var)\s+([a-zA-Z_$][a-zA-Z0-9_$]*)\b/g, (match, decl, name) => {
        if (reserved.has(name) || name.startsWith('_0x') || name.startsWith('_sa') || name.startsWith('_gs')) {
            return match;
        }
        if (!localVars.has(name)) {
            localVars.set(name, nextName());
        }
        return `${decl} ${localVars.get(name)}`;
    }).replace(/\b([a-zA-Z_$][a-zA-Z0-9_$]*)\b/g, (match) => {
        if (localVars.has(match) && !reserved.has(match)) {
            return localVars.get(match);
        }
        return match;
    });
}

// ── Strip comments & minify ──────────────────────────────────
function minify(code) {
    return code
        .replace(/\/\/[^\n]*/g, '')           // single-line comments
        .replace(/\/\*[\s\S]*?\*\//g, '')     // block comments
        .replace(/\n\s*\n/g, '\n')            // blank lines
        .replace(/^\s+/gm, '')                // leading whitespace
        .trim();
}

// ── Self-defense: anti-devtools trap ────────────────────────
const ANTI_DEVTOOLS = `
(function(){
    let _d=false;
    const _t=function(){
        const _start=new Date();
        debugger;
        if(new Date()-_start>100){
            _d=true;
            document.body.innerHTML='';
            window.location.href='about:blank';
        }
    };
    setInterval(_t,800);
})();
`;

// ── Main build ───────────────────────────────────────────────
async function build() {
    const distDir    = path.join(__dirname, '..', 'dist');
    const srcFile    = path.join(__dirname, '..', 'client', 'keyClient.web.js');

    if (!fs.existsSync(distDir)) fs.mkdirSync(distDir, { recursive: true });

    let src = fs.readFileSync(srcFile, 'utf8');

    // 1. Inject encoded config
    const encodedCfg = xorEncode(JSON.stringify(CLIENT_CONFIG), OBF_KEY);
    src = src.replace("'PLACEHOLDER_ENCODED_CONFIG'", `'${encodedCfg}'`);

    // 2. Minify
    let out = minify(src);

    // 3. Obfuscate strings
    out = obfuscateStrings(out);

    // 4. Mangle identifiers
    out = mangleIdentifiers(out);

    // 5. Prepend anti-devtools
    out = ANTI_DEVTOOLS + '\n' + out;

    // 6. Wrap in self-executing function to prevent globals leaking
    out = `!function(){${out}}();`;

    // 7. Write outputs
    const outPath = path.join(distDir, 'keyClient.web.obf.js');
    fs.writeFileSync(outPath, out, 'utf8');
    console.log(`[BUILD] ✅ Written: ${outPath} (${(out.length/1024).toFixed(1)} KB)`);

    // Also write a readable minified version for reference
    let min = fs.readFileSync(srcFile, 'utf8');
    min = min.replace("'PLACEHOLDER_ENCODED_CONFIG'", `'${encodedCfg}'`);
    min = minify(min);
    const minPath = path.join(distDir, 'keyClient.web.min.js');
    fs.writeFileSync(minPath, min, 'utf8');
    console.log(`[BUILD] ✅ Written: ${minPath} (${(min.length/1024).toFixed(1)} KB)`);

    console.log('\n[BUILD] Done. Use dist/keyClient.web.obf.js in production.');
}

build().catch(e => { console.error('[BUILD ERROR]', e.message); process.exit(1); });
