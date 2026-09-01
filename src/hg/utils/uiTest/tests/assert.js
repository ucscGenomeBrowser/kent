// A twenty-line harness for the harness. These tests parse strings and stat
// files -- no browser, no network -- so they are the one tests/ directory here
// whose `make test` really runs.

'use strict';

let pass = 0;
const bad = [];

function ok(cond, what) {
    if (cond) {
        pass++;
    } else {
        bad.push(what);
        console.log(`FAIL  ${what}`);
    }
}

function is(got, want, what) {
    ok(got === want, `${what} (got ${JSON.stringify(got)}, wanted ${JSON.stringify(want)})`);
}

function throws(fn, re, what) {
    try {
        fn();
    } catch (e) {
        ok(re.test(e.message), `${what} (message was "${e.message}")`);
        return e;
    }
    ok(false, `${what} (nothing was thrown)`);
    return null;
}

function done(name) {
    console.log(`${name}: ${pass} passed, ${bad.length} failed`);
    process.exit(bad.length ? 1 : 0);
}

module.exports = { ok, is, throws, done };
