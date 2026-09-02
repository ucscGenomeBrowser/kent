// hgTracks: does it draw the right thing, and does the cart reach it?
//
// This is the worked example. Copy the shape of a check, not the specifics:
//
//     await t.check('what should be true', async () => {
//         ... do something, read something ...
//         return itIsWrong ? `here is what was actually there` : null;
//     });
//
// A check returns the failure message a person will read at three in the
// afternoon when the run goes red. Write that message for them.
//
// Nothing here needs an account or the database, so it runs for anyone against
// genome-test with no conf file at all.

'use strict';

const site = require('../../utils/uiTest/lib/site');
const hg = require('./pages/hgTracks');

const DB = 'hg38';
// SHH and its neighbours: a stable, gene-rich window, the same one docent's own
// tests use.
const POS = 'chr7:155799529-155812871';
const START = 155799529;
const END = 155812871;

module.exports.needs = { login: false, hgsql: false, write: false };

module.exports.main = async function (p, t) {
    const env = t.env;

    // ------------------------------------------------ the default view
    await hg.reset(p, env, { db: DB, position: POS });

    await t.check('hgTracks draws an image table', async () => {
        const n = await p.locator('#imgTbl').count();
        return n ? null : 'there is no #imgTbl on the page';
    });

    await t.check('the default track set is drawn', async () => {
        const rows = await hg.rows(p);
        const want = ['ruler', 'knownGene'];
        const missing = want.filter(w => !hg.drawn(rows, w));
        return missing.length
            ? `missing: ${missing.join(', ')}; drawn: ${rows.join(', ') || '(none)'}`
            : null;
    });

    await t.check('the image height is bounded', async () => {
        const h = await hg.height(p);
        if (!h) {
            return 'the image table has no height at all';
        }
        return h > 3000 ? `the image is ${h}px tall` : null;
    });

    // ------------------------------------------------ the error detector
    // Every navigation asserts the page is not a CGI error page, which is a
    // post-condition nobody has to write. This check is the proof that the
    // detector is alive: a malformed position MUST raise it. Same reasoning as
    // docent's expectfail.xfail.docent.yaml.
    await t.check('a malformed position produces the error banner', async () => {
        await p.goto(site.url(env, 'hgTracks',
            { db: DB, position: 'chrNotAChromosome:1-2' }),
        { waitUntil: 'domcontentloaded' });
        const err = await site.errorOnPage(p);
        return err ? null : 'a nonsense position drew a page with no error on it, ' +
            'so the always-on error detector would not catch a real one either';
    });

    await t.check('an over-long request is reported, not photographed', async () => {
        // Apache answers a long request line with 414, and a 414 renders as a
        // perfectly good page -- which is how a bulk visibility change once
        // sailed past every assertion in a run. This is the second branch of the
        // detector. The composite-expansion case that found it lives in
        // docent/tests/urllen.docent.yaml, which is the right engine for it.
        const long = site.url(env, 'hgTracks', { db: DB, position: POS }) +
            '&padding=' + 'a'.repeat(9000);
        await p.goto(long, { waitUntil: 'domcontentloaded' }).catch(() => {});
        const err = await site.errorOnPage(p);
        return /Too Long/.test(err || '') ? null
            : `a ${long.length}-character request was not reported as an error ` +
              `(detector said: ${err || 'nothing'})`;
    });

    // ------------------------------------------------ the cart reaches hgTracks
    await t.check('turning a track to pack changes its row height', async () => {
        await hg.reset(p, env, { db: DB, position: POS });
        await hg.setVis(p, env, { db: DB, position: POS, knownGene: 'dense' });
        const dense = await hg.rowHeight(p, 'knownGene');
        await hg.setVis(p, env, { db: DB, position: POS, knownGene: 'pack' });
        const pack = await hg.rowHeight(p, 'knownGene');
        if (!dense || !pack) {
            return `knownGene was not drawn (dense ${dense}px, pack ${pack}px)`;
        }
        return pack > dense ? null
            : `knownGene is ${pack}px in pack and ${dense}px in dense, so the ` +
              `cart took the visibility but hgTracks did not apply it`;
    });

    await t.check('hide all leaves only the ruler', async () => {
        await hg.reset(p, env, { db: DB, position: POS });
        await hg.hideAll(p);
        const rows = await hg.rows(p);
        if (!rows.length) {
            return 'hide all left nothing drawn at all, not even the ruler';
        }
        const extra = rows.filter(r => r !== 'ruler');
        return extra.length ? `also drew: ${extra.join(', ')}` : null;
    });

    await t.check('hiding one subtrack leaves its sibling alone', async () => {
        // The two-request composite split (#37953) in the JavaScript idiom. A
        // subtrack needs both its <track>_sel checkbox and its visibility, which
        // is the pair hui.c reads.
        const on = {
            db: DB, position: POS,
            refSeqComposite: 'pack',
            ncbiRefSeqCurated_sel: 1, ncbiRefSeqCurated: 'pack',
            ncbiRefSeqPredicted_sel: 1, ncbiRefSeqPredicted: 'pack',
        };
        await hg.setVis(p, env, on);
        let rows = await hg.rows(p);
        if (!hg.drawn(rows, 'ncbiRefSeqCurated') || !hg.drawn(rows, 'ncbiRefSeqPredicted')) {
            return `could not get both subtracks drawn to start with; drawn: ${rows.join(', ')}`;
        }
        await hg.setVis(p, env, {
            db: DB, position: POS,
            ncbiRefSeqPredicted_sel: 0, ncbiRefSeqPredicted: 'hide',
        });
        rows = await hg.rows(p);
        if (hg.drawn(rows, 'ncbiRefSeqPredicted')) {
            return 'ncbiRefSeqPredicted was hidden and came back';
        }
        return hg.drawn(rows, 'ncbiRefSeqCurated') ? null
            : 'hiding ncbiRefSeqPredicted took ncbiRefSeqCurated with it';
    });

    // ------------------------------------------------ what the page itself says
    await t.check('the position box round-trips a formatted range', async () => {
        // Read back from hgTracks' own JavaScript, not from the position box: an
        // interactive zoom stores the new window in the cart, so the box can be
        // stale while the page knows exactly where it is.
        await hg.reset(p, env, { db: DB, position: 'chr1:1-10000' });
        await hg.search(p, 'chr7:155,799,529-155,812,871');
        const at = await hg.position(p);
        if (!at || !at.chrom) {
            return 'hgTracks.chromName is not set, so the page never reached a tracks view';
        }
        if (at.chrom !== 'chr7' || at.start + 1 !== START || at.end !== END) {
            return `landed on ${at.chrom}:${at.start + 1}-${at.end}, wanted chr7:${START}-${END}`;
        }
        return null;
    });

    await t.check('an item raises its own tooltip', async () => {
        await hg.reset(p, env, { db: DB, position: POS });
        const items = await hg.tooltipItems(p);
        if (!items.length) {
            return 'nothing in the image has a tooltip, so this view cannot show one';
        }
        const item = items[0];
        // The attribute holds HTML; the tooltip renders it, so compare on text.
        const wanted = item.text.replace(/<[^>]*>/g, ' ').replace(/\s+/g, ' ').trim().slice(0, 20);
        if (!wanted) {
            return 'the first item with a tooltip has no readable text in it';
        }
        // Move off everything and wait for any leftover tooltip to go, so a
        // neighbour's is never mistaken for this one.
        await hg.moveAway(p);
        await hg.hover(p, item.x, item.y);
        try {
            await hg.waitForTip(p, wanted);
        } catch (e) {
            return `${e.message} (the tooltip up says "${await hg.tip(p)}")`;
        }
        await hg.moveAway(p);
        const still = await hg.tip(p);
        return still ? `the tooltip stayed up after the mouse left: "${still}"` : null;
    });
};
