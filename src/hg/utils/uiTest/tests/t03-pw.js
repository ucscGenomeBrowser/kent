// Finding the shared Playwright install.
//
// The part worth pinning down is that we load playwright by ABSOLUTE PATH rather
// than by setting NODE_PATH. NODE_PATH loses to any node_modules directory in an
// ancestor of the working directory, and hgwdev has a stale
// /tmp/node_modules/playwright owned by someone else, so a NODE_PATH-based
// resolver breaks for anything run from under /tmp. This test does its work with
// its own working directory moved into a temp tree that has exactly that trap in
// it, so the day someone "simplifies" pw.js back to NODE_PATH, this fails.

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const t = require('./assert');
const pw = require('../lib/pw');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'uiTestPw-'));

// --- usable() wants node_modules/playwright under the prefix ---
t.ok(!pw.usable(null), 'no prefix is not usable');
t.ok(!pw.usable(path.join(tmp, 'nothing')), 'an empty directory is not usable');
const fake = path.join(tmp, 'fake');
fs.mkdirSync(path.join(fake, 'node_modules', 'playwright'), { recursive: true });
t.ok(pw.usable(fake), 'a prefix with node_modules/playwright is usable');

// --- pw.pin is read from the prefix, not from the kent tree ---
t.is(pw.readPin(fake), null, 'no pw.pin reads as null');
fs.writeFileSync(path.join(fake, 'pw.pin'), '1.62.1\n');
t.is(pw.readPin(fake), '1.62.1', 'pw.pin is read and trimmed');

// --- the shared install, where the real answer lives ---
if (pw.usable(pw.SHARED)) {
    const pin = pw.readPin(pw.SHARED);
    t.ok(!!pin, `the shared install at ${pw.SHARED} has a pw.pin (${pin})`);
    t.ok(fs.existsSync(path.join(pw.SHARED, 'browsers')),
        'the shared install has browsers/ beside node_modules');

    // The trap: work from a directory whose ancestor holds a decoy
    // node_modules/playwright, and confirm we still load the shared one.
    const trap = path.join(tmp, 'trap');
    fs.mkdirSync(path.join(trap, 'node_modules', 'playwright'), { recursive: true });
    fs.writeFileSync(path.join(trap, 'node_modules', 'playwright', 'package.json'),
        '{"name":"playwright","version":"0.0.0-decoy","main":"index.js"}');
    fs.writeFileSync(path.join(trap, 'node_modules', 'playwright', 'index.js'),
        'throw new Error("the decoy was loaded");');
    const here = process.cwd();
    const work = path.join(trap, 'work');
    fs.mkdirSync(work);
    process.chdir(work);
    process.env.NODE_PATH = path.join(trap, 'node_modules');
    let r = null;
    try {
        r = pw.resolve({ pwPrefix: null });
    } catch (e) {
        t.ok(false, `resolving from under a decoy node_modules threw: ${e.message}`);
    } finally {
        process.chdir(here);
        delete process.env.NODE_PATH;
    }
    if (r) {
        t.is(r.prefix, pw.SHARED, 'resolved to the shared install, not the decoy beside us');
        t.ok(r.version !== '0.0.0-decoy', 'the decoy playwright was not loaded');
        t.is(process.env.PLAYWRIGHT_BROWSERS_PATH, path.join(pw.SHARED, 'browsers'),
            'PLAYWRIGHT_BROWSERS_PATH was set before the require');
        const sub = pw.subprocessEnv({});
        t.is(sub.PW_DIR, pw.SHARED, 'a docent subprocess is pointed at the same prefix');
        t.is(sub.NODE_PATH, path.join(pw.SHARED, 'node_modules'),
            'and given the NODE_PATH docent needs, since docent uses a bare require');
    }
} else {
    console.log(`SKIP  the shared install is not at ${pw.SHARED} on this machine`);
}

fs.rmSync(tmp, { recursive: true, force: true });
t.done('t03-pw');
