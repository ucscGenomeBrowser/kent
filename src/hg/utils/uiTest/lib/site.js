// site.js -- what every Genome Browser page has in common: CGI URLs, hgLogin,
// the cached login state, reading the page's own JavaScript, and the always-on
// assertion that a navigation did not land on an error page.
//
// Site chrome only. The two selectors allowed here are hgLogin's own fields and
// the menubar's login link. A selector belonging to the BODY of one CGI belongs
// in that CGI's page object, not here.

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const { infraError, configError } = require('./env');
const wait = require('./wait');

const STATE_DIR = path.join(os.homedir(), '.cache', 'uiTest');

// Applied to every context, so no test has to remember it: the hgTracks tutorial
// pops up on a first visit and would sit on top of whatever is being looked at.
const INIT_SCRIPTS = [
    () => {
        try {
            localStorage.setItem('hgTracks_hideTutorial', '1');
        } catch (e) {
            // a context with storage disabled has no tutorial state to set
        }
    },
];

function url(env, cgi, params) {
    const qs = new URLSearchParams(params || {}).toString();
    return `${env.base}/${cgi}${qs ? '?' + qs : ''}`;
}

async function readErrorState(page) {
    // What a kent CGI actually produces when it aborts, read from
    // htmlWarnBoxSetup() and htmlVaWarn() in src/lib/htmshell.c, plus the two
    // pages Apache itself serves when the CGI never got to run.
    return page.evaluate(() => {
        const box = document.getElementById('warnBox');
        const list = document.getElementById('warnList');
        const shown = box && box.offsetWidth > 0 &&
            getComputedStyle(box).display !== 'none' &&
            getComputedStyle(box).visibility !== 'hidden';
        const body = document.body ? document.body.innerText : '';
        const html = document.body ? document.body.innerHTML : '';
        return {
            warn: shown && list ? list.innerText.trim() : '',
            title: document.title || '',
            serverError: /Request-URI Too Long|Internal Server Error|Service Temporarily Unavailable/
                .exec(body),
            tail: html.slice(-200),
        };
    });
}

async function errorOnPage(page) {
    // Returns a description of the error on this page, or null. This is the
    // detector; assertNoCgiError is the assertion built on it, and a test that
    // WANTS an error (proving the detector is alive) calls this one. A page
    // that cannot even be read is reported, not treated as clean -- a page
    // closed or navigated away mid-check must not read as "no error".
    let s;
    try {
        s = await readErrorState(page);
    } catch (e) {
        return `could not check this page for an error: ${e.message}`;
    }
    if (s.warn) {
        return s.warn.replace(/\s+/g, ' ').slice(0, 300);
    }
    if (s.serverError) {
        return s.serverError[0];
    }
    if (/Very Early Error/.test(s.title)) {
        return 'Very Early Error';
    }
    if (s.tail.includes('-- ERROR --')) {
        return 'the page ends in -- ERROR --';
    }
    return null;
}

async function assertNoCgiError(page) {
    const err = await errorOnPage(page);
    if (err) {
        throw new Error(`${page.url()} is an error page: ${err}`);
    }
}

async function goto(page, target) {
    // Every navigation goes through here, so every navigation is checked.
    await wait.navigated(page, () => page.goto(target, { waitUntil: 'domcontentloaded' }),
        assertNoCgiError);
    return page;
}

async function open(page, env, cgi, params) {
    return goto(page, url(env, cgi, params));
}

async function pageGlobal(page, expr) {
    // Read one of the page's own JavaScript globals -- hgTracks.chromName,
    // window.uppy.getFiles(). An assertion about what the page WILL SEND is
    // usually stronger than one about what it happens to have drawn.
    return page.evaluate((e) => {
        try {
            // eslint-disable-next-line no-eval
            return eval(e);
        } catch (err) {
            return undefined;
        }
    }, expr);
}

async function fetchText(page, target) {
    // Fetch through the page's own request context, so the session cookies come
    // along and udc's cache of a hub.txt is bypassed. Returns {status, body}.
    const res = await page.request.get(target);
    return { status: res.status(), body: await res.text() };
}

function stateFileFor(base, user) {
    const host = new URL(base).host.replace(/[^a-zA-Z0-9.-]/g, '_');
    const safeUser = user.replace(/[^a-zA-Z0-9.-]/g, '_');
    return path.join(STATE_DIR, `authState.${host}.${safeUser}.json`);
}

async function loggedInUser(page, env) {
    await page.goto(url(env, 'hgGateway'), { waitUntil: 'domcontentloaded' });
    return page.getAttribute('a#loginLink', 'data-username').catch(() => null);
}

async function login(page, env) {
    const account = env.account;
    if (!account) {
        throw configError('login was asked for with no account configured');
    }
    const password = env.password();
    await page.goto(url(env, 'hgLogin', { 'hgLogin.do.displayLoginPage': 1 }),
        { waitUntil: 'domcontentloaded' });
    // The warning box is in every page but hidden unless there is something to
    // warn about. On the login page it can be a stale-session notice standing
    // between us and the form.
    const ok = await page.$('#warnOK');
    if (ok && await ok.isVisible()) {
        await ok.click();
        await page.waitForLoadState('domcontentloaded');
    }
    await page.fill('#userName', account.user);
    await page.fill('#password', password);
    await Promise.all([
        page.waitForNavigation({ waitUntil: 'domcontentloaded' }),
        page.click('input[name="hgLogin.do.displayLogin"]'),
    ]);
    // The success page sets its cookies from inline JS and then redirects, so
    // the session is not usable until that has run.
    await page.waitForLoadState('load');
    await page.waitForURL(/hgSession|hgGateway|hgTracks|hgHubConnect/, { timeout: env.timeout })
        .catch(() => {});
}

async function ensureLoggedIn(ctx, page, env) {
    // Log in if the cached cookies are gone or stale, then re-save them. One
    // cached session per host and account, so switching between a sandbox and
    // hgwdev never hands over the wrong cookies.
    const account = env.account;
    if (!account) {
        throw configError('ensureLoggedIn was called with no account configured');
    }
    if (await loggedInUser(page, env) !== account.user) {
        await login(page, env);
        if (await loggedInUser(page, env) !== account.user) {
            throw infraError(`login to ${env.base} as ${account.user} did not take -- ` +
                `check the password, and whether hgLogin is showing a warning page`);
        }
    }
    const stateFile = stateFileFor(env.base, account.user);
    fs.mkdirSync(STATE_DIR, { recursive: true, mode: 0o700 });
    fs.chmodSync(STATE_DIR, 0o700);
    await ctx.storageState({ path: stateFile });
    fs.chmodSync(stateFile, 0o600);   // it holds live session cookies
    return page;
}

function storageStateFor(env) {
    // The cached cookies to hand a new context, or undefined if there are none.
    if (!env.account) {
        return undefined;
    }
    const f = stateFileFor(env.base, env.account.user);
    return fs.existsSync(f) ? f : undefined;
}

function hgsql(env, db, sql) {
    // Read-only: rows from a database on this machine, so a test can compare
    // what the page shows against what was stored. Only reachable when
    // env.canHgsql is true.
    if (!env.canHgsql) {
        throw configError(`hgsql is not available here: ${env.hgsqlReason}`);
    }
    const first = sql.trim().split(/\s+/)[0].toUpperCase();
    if (!/^(SELECT|SHOW|DESCRIBE|EXPLAIN)$/.test(first)) {
        throw configError(`hgsql is read-only in uiTest, refusing to run: ${sql}`);
    }
    if (/;\s*\S/.test(sql)) {
        throw configError(`hgsql takes one statement, refusing to run: ${sql}`);
    }
    const out = execFileSync('hgsql', [db, '-e', sql], { encoding: 'utf8' }).trim();
    const lines = out.split('\n');
    if (lines.length < 2) {
        return [];
    }
    const cols = lines[0].split('\t');
    return lines.slice(1).map(l => {
        const v = l.split('\t');
        return Object.fromEntries(cols.map((c, i) => [c, v[i]]));
    });
}

module.exports = {
    url, open, goto, assertNoCgiError, errorOnPage, pageGlobal, fetchText,
    login, ensureLoggedIn, loggedInUser, storageStateFor, hgsql,
    INIT_SCRIPTS, STATE_DIR, stateFileFor,
};
