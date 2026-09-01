// The hgTracks page object: every selector these tests need, in one place.
//
// Verbs do a thing and wait for their own completion condition, and return
// nothing. Readers return data and never assert. Keeping the assertions out of
// here is what lets a check say what it wanted in its own words -- and if a test
// ever needs a raw selector, this file is missing a reader.
//
// The row readers are the same four lines docent's expectState() uses. They are
// re-typed rather than shared on purpose: making hg/utils/docent require a file
// under hg/hgTracks/tests/ would invert the dependency and leave docent
// unrunnable the day that file moved.

'use strict';

const site = require('../../../utils/uiTest/lib/site');
const wait = require('../../../utils/uiTest/lib/wait');

// ---------------------------------------------------------------- verbs

async function waitForImgTbl(p) {
    // #imgTbl is the track image table. Every verb that lands on hgTracks
    // waits for it, so a render that never finishes fails right where it
    // happened instead of as an empty result somewhere downstream.
    try {
        await p.waitForSelector('#imgTbl');
    } catch (e) {
        throw new Error(`hgTracks never drew #imgTbl: ${e.message.split('\n')[0]}`);
    }
}

async function open(p, env, params) {
    // Every navigation goes through site.goto, which asserts the page is not an
    // error page. So no test has to check for a CGI error; they all get it.
    await site.open(p, env, 'hgTracks', params);
    await waitForImgTbl(p);
    return p;
}

async function reset(p, env, params) {
    // hgt.reset drops the cart back to the assembly's default track set, so a
    // run does not inherit whatever the last one left behind.
    return open(p, env, { 'hgt.reset': 1, ...params });
}

async function hideAll(p) {
    // The Hide all button, hButtonWithMsg("hgt.hideAll", ...) in hgTracks.c.
    await wait.navigated(p, async () => {
        await Promise.all([
            p.waitForNavigation({ waitUntil: 'domcontentloaded' }),
            p.click('#hgt\\.hideAll'),
        ]);
    }, site.assertNoCgiError);
    await waitForImgTbl(p);
}

async function setVis(p, env, vis, extra) {
    // Visibility through the cart, the way the browser's own links do it. A
    // subtrack needs its <track>_sel checkbox as well as its visibility, which
    // is the pair hui.c reads.
    return open(p, env, { ...vis, ...(extra || {}) });
}

async function search(p, term) {
    await p.fill('#positionInput', term);
    await wait.navigated(p, async () => {
        await Promise.all([
            p.waitForNavigation({ waitUntil: 'domcontentloaded' }),
            p.press('#positionInput', 'Enter'),
        ]);
    }, site.assertNoCgiError);
    await waitForImgTbl(p);
}

async function hover(p, x, y) {
    await p.mouse.move(x, y);
}

async function moveAway(p) {
    // Park the mouse somewhere with nothing under it, and wait for any tooltip
    // to actually go away. Without this a neighbour's leftover tooltip reads as
    // the one this check was about to raise.
    await p.mouse.move(2, 2);
    await wait.until(p, () => {
        const c = document.getElementById('mouseoverContainer');
        return !c || getComputedStyle(c).visibility === 'hidden';
    }, 'a tooltip was still up after moving the mouse off everything',
    { timeout: 5000 });
}

// ---------------------------------------------------------------- readers

async function rows(p) {
    // The track rows hgTracks drew, scoped to the image table. Document-wide
    // would also sweep up page chrome: the ideogram IMG carries id="chrom".
    return p.evaluate(() =>
        [...document.querySelectorAll('#imgTbl [id^="img_data_"]')]
            .map(e => e.id.replace('img_data_', '')));
}

function drawn(rowList, name) {
    // A hub track's row id carries a per-run hub_<n>_ prefix, so match a plain
    // name by suffix, the way docent resolves a track.
    return rowList.some(r => r === name || r.endsWith('_' + name));
}

async function height(p) {
    return p.evaluate(() => {
        const im = document.getElementById('imgTbl');
        return im ? Math.round(im.getBoundingClientRect().height) : 0;
    });
}

async function rowHeight(p, track) {
    // How tall one track's row is. A visibility that the cart accepted but
    // hgTracks did not apply shows up here and nowhere else.
    return p.evaluate((t) => {
        const el = [...document.querySelectorAll('#imgTbl [id^="img_data_"]')]
            .find(e => e.id === 'img_data_' + t || e.id.endsWith('_' + t));
        return el ? Math.round(el.getBoundingClientRect().height) : 0;
    }, track);
}

async function position(p) {
    // Straight from the page's own JavaScript rather than the position box: an
    // interactive zoom stores the new window in the cart, so the box can be
    // stale while hgTracks knows exactly where it is.
    return site.pageGlobal(p,
        '({chrom: hgTracks.chromName, start: hgTracks.winStart, end: hgTracks.winEnd})');
}

async function tooltipItems(p) {
    // Every item in the image that has a tooltip, with the point to hover to
    // raise it. convertTitleTagsToMouseovers() in utils.js copies each area's
    // title into a mouseoverText attribute on load, so this is what the page
    // will actually show.
    return p.evaluate(() => {
        const out = [];
        for (const a of document.querySelectorAll('#imgTbl area[mouseoverText]')) {
            const text = a.getAttribute('mouseoverText');
            const map = a.closest('map');
            if (!text || !map || !map.name) {
                continue;
            }
            const img = document.querySelector(`img[usemap="#${map.name}"]`);
            if (!img) {
                continue;
            }
            const c = (a.getAttribute('coords') || '').split(',').map(Number);
            if (c.length < 4) {
                continue;
            }
            const box = img.getBoundingClientRect();
            out.push({
                text: text,
                x: Math.round(box.left + (c[0] + c[2]) / 2),
                y: Math.round(box.top + (c[1] + c[3]) / 2),
            });
        }
        return out;
    });
}

async function tip(p) {
    return p.evaluate(() => {
        const c = document.getElementById('mouseoverContainer');
        if (!c) {
            return '';
        }
        const up = c.offsetWidth > 0 && getComputedStyle(c).visibility !== 'hidden' &&
            getComputedStyle(c).display !== 'none';
        return up ? c.innerText.trim() : '';
    });
}

async function waitForTip(p, wanted) {
    // The tooltip appears 500ms after mouseenter (addMouseover in utils.js), so
    // this waits for the text rather than for the clock.
    await wait.until(p, (want) => {
        const c = document.getElementById('mouseoverContainer');
        if (!c || getComputedStyle(c).visibility === 'hidden') {
            return false;
        }
        return c.innerText.trim().includes(want);
    }, `no tooltip saying "${wanted}" came up`, { arg: wanted, timeout: 8000 });
}

module.exports = {
    open, reset, hideAll, setVis, search, hover, moveAway,
    rows, drawn, height, rowHeight, position, tooltipItems, tip, waitForTip,
};
