// pw.js -- find the shared Playwright install and load it.
//
// Resolution order, first hit wins:
//
//   1. UITEST_PW_PREFIX
//   2. pw.prefix from the conf file
//   3. /hive/groups/browser/uiTest/pw     the shared install, owned by braney
//   4. $HOME/.uiTestPw                    a laptop or a non-hgwdev machine
//   5. bare require('playwright')         whatever node can already see
//
// The shared install is flat: node_modules/ and browsers/ sit directly under the
// prefix. Its README.md carries the pin and the recipe for moving it.
//
// Two things this module does on purpose:
//
// Playwright is loaded by ABSOLUTE PATH, not by setting NODE_PATH. NODE_PATH
// loses to any node_modules directory in an ancestor of the working directory,
// and hgwdev has a stale /tmp/node_modules/playwright owned by someone else, so
// anything run from under /tmp with NODE_PATH set loads that instead and dies.
// An absolute-path require does not consult ancestor node_modules at all, so a
// uiTest run works from any directory.
//
// PLAYWRIGHT_BROWSERS_PATH is set before the require, because the browsers are a
// sibling of node_modules rather than inside it.

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { infraError } = require('./env');

const SHARED = '/hive/groups/browser/uiTest/pw';
const PRIVATE = path.join(os.homedir(), '.uiTestPw');

let cached = null;

function usable(prefix) {
    if (!prefix) {
        return false;
    }
    return fs.existsSync(path.join(prefix, 'node_modules', 'playwright'));
}

function readPin(prefix) {
    try {
        return fs.readFileSync(path.join(prefix, 'pw.pin'), 'utf8').trim();
    } catch (e) {
        return null;
    }
}

function versionAt(modDir) {
    try {
        return JSON.parse(fs.readFileSync(path.join(modDir, 'package.json'), 'utf8')).version;
    } catch (e) {
        return null;
    }
}

function resolve(env) {
    // Returns {prefix, source, playwright, version, pin, browsersPath, warning}.
    // The prefix is null when we fell through to a bare require.
    if (cached) {
        return cached;
    }
    const tried = [];
    const order = [
        [env && env.pwPrefix, 'UITEST_PW_PREFIX or pw.prefix'],
        [SHARED, 'the shared install'],
        [PRIVATE, 'a private install'],
    ];
    for (const [prefix, source] of order) {
        if (!prefix) {
            continue;
        }
        tried.push(prefix);
        if (!usable(prefix)) {
            continue;
        }
        const browsers = path.join(prefix, 'browsers');
        if (fs.existsSync(browsers)) {
            process.env.PLAYWRIGHT_BROWSERS_PATH = browsers;
        }
        const modDir = path.join(prefix, 'node_modules', 'playwright');
        const version = versionAt(modDir);
        const pin = readPin(prefix);
        let warning = null;
        if (pin && version && pin !== version) {
            warning = `playwright ${version} does not match the pin ${pin} in ` +
                `${prefix}/pw.pin -- proceeding, but say so if something looks odd`;
        }
        cached = {
            prefix: prefix, source: source, playwright: require(modDir),
            version: version, pin: pin,
            browsersPath: fs.existsSync(browsers) ? browsers : null,
            warning: warning,
        };
        return cached;
    }
    // Last resort: whatever this node process can already see.
    try {
        const pw = require('playwright');
        cached = {
            prefix: null, source: 'a bare require', playwright: pw,
            version: null, pin: null, browsersPath: null, warning: null,
        };
        return cached;
    } catch (e) {
        throw infraError(
            `cannot find playwright. Looked in:\n  ${tried.join('\n  ')}\n` +
            `The shared install is ${SHARED} -- read its README.md for how it is ` +
            `built and who owns it. Point somewhere else with UITEST_PW_PREFIX or ` +
            `a pw.prefix line in your conf file.`);
    }
}

function chromium(env) {
    return resolve(env).playwright.chromium;
}

function subprocessEnv(env) {
    // The environment a docent subprocess needs. Docent uses a bare require, so
    // it does need NODE_PATH -- run it with an explicit cwd that is not under
    // /tmp and the stale copy there cannot reach it.
    const r = resolve(env);
    if (!r.prefix) {
        return {};
    }
    return {
        PW_DIR: r.prefix,
        NODE_PATH: path.join(r.prefix, 'node_modules'),
        ...(r.browsersPath ? { PLAYWRIGHT_BROWSERS_PATH: r.browsersPath } : {}),
    };
}

module.exports = { resolve, chromium, subprocessEnv, SHARED, PRIVATE, usable, readPin };
