// browser.js -- launching a browser, making a context, tracing, screenshots.
//
// This layer must stay code that could drive any website. It knows nothing about
// the Genome Browser: no CGI names, no selectors, no cart, no login. Anything
// site-specific belongs in site.js one layer up.
//
// Every determinism knob is applied here, once, so no test has to remember it: a
// fixed viewport, an sRGB colour profile, a pinned locale and timezone. Two runs
// on two machines then see the same page.

'use strict';

const fs = require('fs');
const path = require('path');
const pw = require('./pw');

// Fixed and generous: wide enough that the button bars do not wrap, tall enough
// that a normal page needs no scrolling to be photographed.
const VIEWPORT = { width: 1280, height: 1024 };
const LOCALE = 'en-US';
const TIMEZONE = 'America/Los_Angeles';

async function launch(env) {
    const chromium = pw.chromium(env);
    return chromium.launch({
        headless: !env.headed,
        slowMo: env.slowMo || 0,
        args: ['--force-color-profile=srgb'],
    });
}

async function context(browser, env, opts) {
    const o = opts || {};
    const ctx = await browser.newContext({
        viewport: VIEWPORT,
        locale: LOCALE,
        timezoneId: TIMEZONE,
        acceptDownloads: true,
        ...(o.storageState ? { storageState: o.storageState } : {}),
    });
    ctx.setDefaultTimeout(env.timeout);
    ctx.setDefaultNavigationTimeout(env.timeout);
    for (const script of (o.initScripts || [])) {
        await ctx.addInitScript(script);
    }
    // sources: false so a trace never carries a copy of the kent tree's
    // JavaScript around with it.
    if (o.trace !== false) {
        await ctx.tracing.start({ screenshots: true, snapshots: true, sources: false });
    }
    return ctx;
}

async function stopTracing(ctx, file) {
    // With a path the trace is written; with none it is thrown away. So tracing
    // is free on a run where everything passed.
    try {
        if (file) {
            fs.mkdirSync(path.dirname(file), { recursive: true });
            await ctx.tracing.stop({ path: file });
        } else {
            await ctx.tracing.stop();
        }
    } catch (e) {
        // A context that already closed has nothing to stop, and there are
        // other ways this can fail -- an artifacts directory that is full or
        // unwritable, say. Report the reason so a missing trace, the most
        // useful thing in the artifacts directory, does not go missing with no
        // sign why. Never let cleanup turn a passing run into a failing one.
        console.error(`uiTest: could not write trace: ${e.message}`);
    }
}

async function shot(page, file) {
    try {
        fs.mkdirSync(path.dirname(file), { recursive: true });
        await page.screenshot({ path: file, fullPage: true });
        return file;
    } catch (e) {
        console.error(`uiTest: could not save screenshot ${file}: ${e.message}`);
        return null;
    }
}

module.exports = { launch, context, stopTracing, shot, VIEWPORT, LOCALE, TIMEZONE };
