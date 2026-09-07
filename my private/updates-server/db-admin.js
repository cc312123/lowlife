const secureDb = require('./secure-db');

const args = process.argv.slice(2);
const command = args[0] ? args[0].toLowerCase() : 'help';

function showHelp() {
    console.log(`
============================================================
Tung-Ware Encrypted Database Administrator CLI Utility
============================================================
Usage:
  node db-admin.js <command> [arguments]

Commands:
  list-users                          Lists all registered user accounts
  reset-hwid <username>               Resets the HWID device lock for a user
  add-user <username> <password>      Directly registers a new account
  delete-user <username>              Deletes a user account from the database
  extend-license <username> <hours>   Directly adds hours to a user's subscription (-1 for lifetime)
  gen-key [hours]                     Generates a new expiring license key (default: 24h)
  list-keys                           Lists all generated license keys and status
  help                                Shows this help dialog
============================================================
`);
}

switch (command) {
    case 'list-users': {
        const db = secureDb.getRawDatabase();
        const users = Object.values(db.users);
        if (users.length === 0) {
            console.log("No registered users found.");
            break;
        }

        console.log("\n--- REGISTERED USERS ---");
        users.forEach(u => {
            const expiryStr = u.licenseExpiry ? new Date(u.licenseExpiry).toLocaleString() : "No Active License";
            console.log(`Username:  ${u.username}`);
            console.log(`Created:   ${new Date(u.createdAt).toLocaleString()}`);
            console.log(`License:   ${expiryStr}`);
            console.log(`HWID Lock: ${u.hwid || "Not bound (Awaiting first login)"}`);
            console.log("------------------------");
        });
        break;
    }

    case 'reset-hwid': {
        const username = args[1];
        if (!username) {
            console.log("Error: Please provide a username.");
            console.log("Usage: node db-admin.js reset-hwid <username>");
            break;
        }
        const result = secureDb.resetHwid(username);
        if (result.success) {
            console.log(`Success: HWID lock cleared for user '${username}'.`);
        } else {
            console.log(`Error: ${result.error}`);
        }
        break;
    }

    case 'add-user': {
        const username = args[1];
        const password = args[2];
        if (!username || !password) {
            console.log("Error: Please provide both username and password.");
            console.log("Usage: node db-admin.js add-user <username> <password>");
            break;
        }
        const result = secureDb.createUser(username, password);
        if (result.success) {
            console.log(`Success: Created account '${username}'.`);
        } else {
            console.log(`Error: ${result.error}`);
        }
        break;
    }

    case 'delete-user': {
        const username = args[1];
        if (!username) {
            console.log("Error: Please provide a username.");
            console.log("Usage: node db-admin.js delete-user <username>");
            break;
        }
        const db = secureDb.getRawDatabase();
        const normalized = username.toLowerCase().trim();
        if (!db.users[normalized]) {
            console.log(`Error: User '${username}' does not exist.`);
            break;
        }
        delete db.users[normalized];
        secureDb.saveRawDatabase(db);
        console.log(`Success: Deleted account '${username}' from database.`);
        break;
    }

    case 'extend-license': {
        const username = args[1];
        const hoursStr = args[2];
        if (!username || !hoursStr) {
            console.log("Error: Please provide a username and duration in hours.");
            console.log("Usage: node db-admin.js extend-license <username> <hours>");
            break;
        }
        const hours = parseInt(hoursStr);
        if (isNaN(hours)) {
            console.log("Error: Hours must be a valid number (-1 for lifetime).");
            break;
        }

        const db = secureDb.getRawDatabase();
        const normalized = username.toLowerCase().trim();
        const user = db.users[normalized];
        if (!user) {
            console.log(`Error: User '${username}' does not exist.`);
            break;
        }

        let currentExpiry = user.licenseExpiry ? new Date(user.licenseExpiry).getTime() : Date.now();
        if (currentExpiry < Date.now()) {
            currentExpiry = Date.now();
        }

        let newExpiry;
        if (hours === -1) {
            newExpiry = new Date('2099-12-31T23:59:59.000Z');
        } else {
            newExpiry = new Date(currentExpiry + hours * 60 * 60 * 1000);
        }

        user.licenseExpiry = newExpiry.toISOString();
        secureDb.saveRawDatabase(db);
        console.log(`Success: Subscription extended for '${username}'. New expiry: ${newExpiry.toLocaleString()}`);
        break;
    }

    case 'gen-key': {
        const hoursStr = args[1] || '24';
        const hours = parseInt(hoursStr);
        if (isNaN(hours)) {
            console.log("Error: Hours must be a valid number.");
            break;
        }
        const result = secureDb.generateLicenseKey(hours);
        if (result.success) {
            console.log(`Success: Generated key (${hours === -1 ? 'Lifetime' : hours + 'h'}):`);
            console.log(`Key: \x1b[32m${result.key}\x1b[0m`);
        } else {
            console.log("Error generating key.");
        }
        break;
    }

    case 'list-keys': {
        const db = secureDb.getRawDatabase();
        const keys = Object.entries(db.licenseKeys);
        if (keys.length === 0) {
            console.log("No generated keys found.");
            break;
        }

        console.log("\n--- GENERATED LICENSE KEYS ---");
        keys.forEach(([key, data]) => {
            const hoursStr = data.durationHours === -1 ? "Lifetime" : `${data.durationHours} Hours`;
            const status = data.usedBy ? `REDEEMED by ${data.usedBy}` : "ACTIVE/UNCLAIMED";
            console.log(`Key:      ${key}`);
            console.log(`Duration: ${hoursStr}`);
            console.log(`Status:   ${status}`);
            console.log("------------------------------");
        });
        break;
    }

    case 'help':
    default:
        showHelp();
        break;
}
