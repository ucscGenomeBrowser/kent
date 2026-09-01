// wait.js -- the anti-flake helpers.
//
// The policy these exist to serve: page.waitForTimeout is banned in lib/ and in
// pages/, and in a test it must carry a "// flake:" comment naming what it waits
// for and why no condition exists. "uiTest lint" enforces that.
//
// A sleep is a guess about how long something takes on a machine you are not
// sitting at. Every helper here waits for a CONDITION instead, and says what it
// was waiting for when the condition never arrives -- which is the whole
// difference between a useful failure and "Timeout 30000ms exceeded".

'use strict';

async function until(page, fn, msg, opts) {
    // waitForFunction with an authored failure message. The workhorse: almost
    // every sleep in a page object is really "wait until this row exists". With
    // no opts.timeout this uses the context's own default timeout instead of a
    // number of its own.
    const o = opts || {};
    const options = o.timeout ? { timeout: o.timeout } : {};
    try {
        await page.waitForFunction(fn, o.arg, options);
    } catch (e) {
        throw new Error(`${msg}: ${e.message.split('\n')[0]}`);
    }
}

async function eventually(fn, msg, opts) {
    // The Node-side version: poll a check()-shaped function (returns a failure
    // string, or null when it holds) until it holds. Replaces "sleep, then
    // assert". Returns the last failure string, so it drops straight into a
    // check() body. There is no page here, so no context default timeout to
    // fall back on -- 30000 below is this function's own default, not
    // timeout.default.
    const o = opts || {};
    const timeout = o.timeout || 30000;
    const every = o.every || 250;
    const deadline = Date.now() + timeout;
    let last = msg || 'condition never held';
    for (;;) {
        try {
            const out = await fn();
            if (!out) {
                return null;
            }
            last = out;
        } catch (e) {
            last = `threw: ${e.message.split('\n')[0]}`;
        }
        if (Date.now() >= deadline) {
            return `${last} (still true after ${timeout}ms)`;
        }
        await new Promise(r => setTimeout(r, every));
    }
}

async function noPending(page) {
    // Wait for jQuery to have no request in flight. hgTracks and myData both
    // drive their updates through jQuery ajax, so this is a real, cheap, correct
    // condition and it is the right default wait after a click. A page with no
    // jQuery has nothing pending by definition.
    try {
        await page.waitForFunction(() => {
            const jq = window.jQuery || window.$;
            return !jq || jq.active === 0;
        });
    } catch (e) {
        throw new Error(`jQuery still had a request in flight: ${e.message.split('\n')[0]}`);
    }
}

async function navigated(page, fn, postCondition) {
    // Run fn, wait for the page to settle, then apply the caller's
    // post-condition. site.js passes the always-on CGI error assertion here, so
    // every navigation in every CGI's tests gets checked without anyone writing
    // a check for it. The post-condition is a parameter rather than a require,
    // to keep this file free of anything site-specific.
    await fn();
    await page.waitForLoadState('domcontentloaded');
    await noPending(page);
    if (postCondition) {
        await postCondition(page);
    }
}

async function stable(page, sel, opts) {
    // Bounding box unchanged across two animation frames. For jQuery dialogs and
    // anything else that slides into place: "it exists" is true well before "it
    // has stopped moving". With no opts.timeout this uses the context's own
    // default timeout instead of a number of its own.
    const o = opts || {};
    const options = { polling: 'raf' };
    if (o.timeout) {
        options.timeout = o.timeout;
    }
    await page.waitForFunction((s) => {
        const el = document.querySelector(s);
        if (!el) {
            return false;
        }
        const key = '__uiTestBox';
        const now = el.getBoundingClientRect();
        const was = el[key];
        el[key] = { x: now.x, y: now.y, w: now.width, h: now.height };
        return !!was && was.x === now.x && was.y === now.y &&
            was.w === now.width && was.h === now.height;
    }, sel, options)
        .catch((e) => {
            throw new Error(`"${sel}" never stopped moving: ${e.message.split('\n')[0]}`);
        });
}

module.exports = { until, eventually, noPending, navigated, stable };
