// run.js -- checks, skips, xfail, and the results a run leaves behind.
//
// A check returns a FAILURE STRING, or anything falsy when it holds:
//
//     await t.check('hide all leaves only the ruler', async () => {
//         await hgTracks.hideAll(p);
//         const extra = (await hgTracks.rows(p)).filter(r => r !== 'ruler');
//         return extra.length ? `also drew: ${extra.join(', ')}` : null;
//     });
//
// The message is authored by whoever wrote the check, which is why this is
// better here than assert: "also drew: clinvarCnv" tells you what went wrong,
// where "expected 0 to equal 1" does not. A check that throws is a failure too,
// so a stale selector shows up as a failed check rather than killing the run.
//
// Skipped never folds into passed. A suite that quietly skipped everything must
// not read as green, so every status is counted separately and printed.
//
// This file must not name a CGI.

'use strict';

const fs = require('fs');
const path = require('path');
const browser = require('./browser');
const site = require('./site');
const { EXIT, configError } = require('./env');

const SCHEMA = 1;

function slug(s) {
    return s.replace(/[^a-z0-9]+/gi, '-').replace(/^-|-$/g, '').slice(0, 60).toLowerCase();
}

function safeUrl(u) {
    // The URL that produced a check, fit to be written down. The hgsid goes,
    // because a results.json often lands under public_html and an hgsid is a
    // live session anyone who reads it could then load. Long ones are cut,
    // because a check can deliberately send a 9000-character request line.
    if (!u) {
        return '';
    }
    let out = u;
    try {
        const parsed = new URL(u);
        parsed.searchParams.delete('hgsid');
        out = parsed.toString();
    } catch (e) {
        out = u.replace(/([?&])hgsid=[^&]*/g, '$1');
    }
    return out.length > 300 ? `${out.slice(0, 300)}... [${out.length} characters]` : out;
}

function resolveNeeds(needs, env) {
    // Decided before any browser launches. Returns null to run, or a reason to
    // skip. Throws a config error for the one case that must never be skippable.
    const n = needs || {};
    if (n.write && env.isRr) {
        throw configError(
            `this test declares needs.write and the target is ${env.target}, which is ` +
            `production. Refusing to run. Point --target somewhere else.`);
    }
    if (n.write && !env.canWrite) {
        return `can.write is not set in ${env.confFile}`;
    }
    if (n.login && !env.account) {
        return env.accountReason;
    }
    if (n.hgsql && !env.canHgsql) {
        return env.hgsqlReason;
    }
    return null;
}

function createRun(env, dir) {
    fs.mkdirSync(dir, { recursive: true });
    const logFile = path.join(dir, 'run.log');

    const run = {
        env: env,
        dir: dir,
        checks: [],
        grep: null,
        strict: false,
        filtered: 0,
        page: null,        // the page a failing check is photographed from
        failedHere: false, // did the test file being run right now fail?
    };

    run.log = function (msg) {
        const line = msg == null ? '' : String(msg);
        console.log(line);
        fs.appendFileSync(logFile, line + '\n');
    };

    run.heading = function (s) {
        run.log(`\n--- ${s} ---`);
    };

    run.record = function (entry) {
        run.checks.push(entry);
        if (entry.status === 'fail' || entry.status === 'xpass') {
            run.failedHere = true;
        }
        return entry;
    };

    async function one(name, fn, expectFail) {
        if (run.grep && !name.toLowerCase().includes(run.grep.toLowerCase())) {
            run.filtered++;
            return true;
        }
        const t0 = Date.now();
        let out;
        try {
            out = await fn();
        } catch (e) {
            out = `threw: ${String(e.message || e).split('\n')[0]}`;
        }
        const held = !out;
        let status;
        if (expectFail) {
            status = held ? 'xpass' : 'xfail';
        } else {
            status = held ? 'pass' : 'fail';
        }
        const bad = status === 'fail' || status === 'xpass';
        const entry = {
            name: name,
            status: status,
            ms: Date.now() - t0,
            detail: out ? String(out) : '',
            url: run.page ? safeUrl(run.page.url()) : '',
            shot: null,
        };
        if (status === 'xpass') {
            entry.detail = 'this was supposed to fail, and it passed';
        }
        if (bad && run.page) {
            const f = path.join(dir, 'shots',
                `${String(run.checks.length + 1).padStart(2, '0')}-${slug(name)}.png`);
            entry.shot = await browser.shot(run.page, f);
        }
        const label = { pass: 'PASS', fail: 'FAIL', xfail: 'XFAIL', xpass: 'XPASS' }[status];
        run.log(`${label}  ${name}${entry.detail ? '\n        ' + entry.detail : ''}`);
        if (entry.shot) {
            run.log(`        screenshot: ${entry.shot}`);
        }
        run.record(entry);
        return !bad;
    }

    run.check = (name, fn) => one(name, fn, false);
    run.xcheck = (name, fn) => one(name, fn, true);

    run.skipped = function (name, reason) {
        run.log(`SKIP  ${name}\n        ${reason}`);
        return run.record({ name, status: 'skip', ms: 0, detail: reason, url: '', shot: null });
    };

    run.runFile = async function (file) {
        // Load the test, decide whether it may run, then give it a page and get
        // out of the way.
        const name = path.basename(file).replace(/\.js$/, '');
        run.heading(name);
        let mod;
        try {
            mod = require(path.resolve(file));
        } catch (e) {
            const detail = `cannot load ${file}: ${e.message}`.split('\n')[0];
            run.record({
                name: `${name} aborted`, status: 'fail', ms: 0,
                detail: detail, url: '', shot: null,
            });
            run.log(`\nRUN ABORTED: ${detail}`);
            return;
        }
        if (typeof mod.main !== 'function') {
            const detail = `${file} exports no main function`;
            run.record({
                name: `${name} aborted`, status: 'fail', ms: 0,
                detail: detail, url: '', shot: null,
            });
            run.log(`\nRUN ABORTED: ${detail}`);
            return;
        }
        const skip = resolveNeeds(mod.needs, env);
        if (skip) {
            run.skipped(name, skip);
            return;
        }

        run.failedHere = false;
        const b = await browser.launch(env);
        try {
            const ctx = await browser.context(b, env, {
                initScripts: site.INIT_SCRIPTS,
                storageState: (mod.needs && mod.needs.login) ? site.storageStateFor(env) : undefined,
            });
            const p = await ctx.newPage();
            p.on('pageerror', e => run.log(`        [page error] ${e.message}`));
            run.page = p;
            let traceFile = null;
            try {
                if (mod.needs && mod.needs.login) {
                    await site.ensureLoggedIn(ctx, p, env);
                }
                await mod.main(p, run);
            } catch (e) {
                if (e.exitCode) {
                    throw e;              // config or infrastructure, not a failed check
                }
                run.record({
                    name: `${name} aborted`, status: 'fail', ms: 0,
                    detail: String(e.message || e).split('\n')[0],
                    url: safeUrl(p.url()),
                    shot: await browser.shot(p, path.join(dir, 'shots', `${slug(name)}-abort.png`)),
                });
                run.log(`\nRUN ABORTED: ${e.message}`);
            } finally {
                // A trace is written only when this file recorded a failure, so
                // tracing costs nothing on a green run.
                if (run.failedHere) {
                    traceFile = path.join(dir, 'trace', `${name}.zip`);
                }
                await browser.stopTracing(ctx, traceFile);
                run.page = null;
            }
            if (traceFile) {
                run.log(`        trace: npx playwright show-trace ${traceFile}`);
            }
        } finally {
            // The browser is closed on every path out of this block, launched
            // or not, so a failure while setting up the context or the page
            // never leaves a headless Chromium process behind.
            await b.close().catch(() => {});
        }
    };

    run.totals = function () {
        const t = { pass: 0, fail: 0, skip: 0, xfail: 0, xpass: 0 };
        run.checks.forEach(c => { t[c.status]++; });
        return t;
    };

    run.summary = function () {
        const t = run.totals();
        const parts = [`${t.pass} passed`, `${t.fail} failed`, `${t.skip} skipped`];
        if (t.xfail) {
            parts.push(`${t.xfail} xfail`);
        }
        if (t.xpass) {
            parts.push(`${t.xpass} xpass`);
        }
        run.log(`\n${parts.join(', ')}`);
        if (run.filtered) {
            run.log(`(${run.filtered} check(s) not run, filtered by --grep)`);
        }
        const bad = run.checks.filter(c => c.status === 'fail' || c.status === 'xpass');
        if (bad.length) {
            run.log('failed:');
            bad.forEach(c => run.log(`  - ${c.name}: ${c.detail}`));
        }
        run.log(`\nresults: ${path.join(dir, 'results.json')}`);
        return t;
    };

    run.write = function (meta) {
        const out = {
            schema: SCHEMA,
            ...meta,
            target: env.target,
            base: env.base,
            account: env.account ? env.account.user : null,
            totals: run.totals(),
            checks: run.checks,
        };
        fs.writeFileSync(path.join(dir, 'results.json'), JSON.stringify(out, null, 2) + '\n');
        return out;
    };

    run.exitCode = function () {
        const t = run.totals();
        if (t.fail || t.xpass) {
            return EXIT.FAIL;
        }
        if (run.strict && t.pass === 0 && t.xfail === 0 && (t.skip > 0 || run.filtered > 0)) {
            return EXIT.SKIPPED;
        }
        return EXIT.OK;
    };

    return run;
}

module.exports = { createRun, resolveNeeds, slug, safeUrl, SCHEMA };
