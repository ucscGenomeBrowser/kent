#!/usr/bin/env node
/* docent.js SCRIPT.docent.yaml [OUT.mp4]
 *
 * Docent -- a language for authoring guided tours of the UCSC Genome Browser.
 *
 * Render a hand-authored Docent script into a silent mp4 PLUS a named still PNG at
 * every `shot:` marker (the figures). One source script -> both outputs, so a figure
 * is literally a frame of the tour. Reuses the shared Playwright/Chromium in ~/pwrec.
 *
 *   PLAYWRIGHT_BROWSERS_PATH=~/pwrec/browsers NODE_PATH=~/pwrec/node_modules \
 *     node docent.js AP1.docent.yaml
 *
 * The high-level verbs bake in the quickLift/Convert mechanics (dbSNP composite
 * params, the hideDefaults-reverts-on-assembly-change bug, target lookup) so the
 * author writes intent, not selectors. See README.md for the language.
 *
 * The surface syntax is YAML so ordinary editors highlight it; the language is the
 * verb vocabulary layered on top, not the serialization. Scripts are named
 * <base>.docent.yaml (a bare <base>.docent also works).
 */
const { chromium } = require('playwright');
const yaml = require('js-yaml');
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

// ---------- parse script + config ----------
const SCRIPT = process.argv[2];
if (!SCRIPT) { console.error('usage: node docent.js SCRIPT.docent.yaml [OUT.mp4]'); process.exit(2); }
const doc = yaml.load(fs.readFileSync(SCRIPT, 'utf8')) || {};

// Lint: in a YAML flow map a colon needs a trailing space, so `{item:name5568747}`
// parses as ONE key "item:name5568747" (value null) and the intended `item:` arg is
// silently dropped -- the verb then quietly falls back to a default. Catch that here
// (before the long browser run) by flagging any arg key that contains a ':'.
(function lintSteps(steps) {
  let n = 0;
  const scan = (obj, where) => {
    if (!obj || typeof obj !== 'object') return;
    for (const k of Object.keys(obj)) {
      if (typeof k === 'string' && k.includes(':')) {
        n++;
        console.warn(`WARNING ${where}: key "${k}" contains ':' -- a YAML flow map needs a `
          + `space after the colon. Did you mean "${k.replace(/:(?=\S)/, ': ')}"? `
          + `(this key is being IGNORED, so the verb may fall back to a default)`);
      }
      scan(obj[k], where);
    }
  };
  (steps || []).forEach((s, i) => {
    if (s && typeof s === 'object') { const v = Object.keys(s)[0]; scan(s[v], `step ${i + 1} (${v})`); }
  });
  if (n) console.warn(`(${n} suspicious key${n > 1 ? 's' : ''} above -- fix the missing space, or the arg is dropped)`);
})(doc.steps);
const HERE = path.dirname(path.resolve(SCRIPT));
const base = path.basename(SCRIPT).replace(/\.(docent\.)?ya?ml$/i, '').replace(/\.docent$/i, '');
const FIGDIR = path.resolve(HERE, '..');                       // figures dir beside the scripts
const OUTMP4 = process.argv[3] || doc.mp4 || path.join(FIGDIR, base + '.mp4');
// Stills go to stills/<base>/. DOCENT_STILLS names a different PARENT ("stills.hires"),
// which is how a high-resolution run keeps its figures beside the screen-resolution ones
// instead of overwriting them.
const STILLPARENT = process.env.DOCENT_STILLS;
const STILLDIR = STILLPARENT ? path.resolve(HERE, STILLPARENT, base)
  : doc.stills ? path.resolve(HERE, doc.stills) : path.join(HERE, 'stills', base);
// Saved sessions go to sessions/<base>/, beside stills/. `sessions:` and DOCENT_SESSIONS
// name the PARENT (not the per-scenario directory), so `sessionUrlBase:` below always maps
// onto it as <base>/<name>.txt, and a print run keeps its files out of the screen run's way.
const SESSDIR = path.join(
  path.resolve(HERE, process.env.DOCENT_SESSIONS || doc.sessions || 'sessions'), base);

// `target:` takes a shorthand from this table, a bare `hgwdev-<user>` sandbox name
// (expanded below), or a full https://.../cgi-bin URL. Default is genome-test, so a
// script that forgets to say where it runs does not silently hit someone's sandbox.
const SERVERS = {
  'rr': 'https://genome.ucsc.edu/cgi-bin',
  'genome-test': 'https://genome-test.gi.ucsc.edu/cgi-bin',
  'hgwdev': 'https://hgwdev.gi.ucsc.edu/cgi-bin',
  'hgwbeta': 'https://hgwbeta.soe.ucsc.edu/cgi-bin',
};
const resolveTarget = t => {
  if (!t) return SERVERS['genome-test'];
  if (SERVERS[t]) return SERVERS[t];
  if (/^hgwdev-[a-z0-9._-]+$/i.test(t)) return `https://${t}.gi.ucsc.edu/cgi-bin`;  // personal sandbox
  return t;                                                    // full URL
};
const SERVER = resolveTarget(doc.target).replace(/\/$/, '');
// SCALE: the same tour rendered at k times the resolution, for figures that have to print.
// Nothing is upscaled -- a still only ever has the pixels it was drawn with -- so each layer
// is asked to draw k times as many while the layout is left alone:
//
//   * deviceScaleFactor: k. The viewport keeps its 1x CSS size, so the page lays out exactly
//     as at 1x -- same line breaks, same jQuery-dialog width, same tooltip placement -- and
//     every bit of it is rasterized with k times the pixels. The retina case, natively.
//   * `pix` x k, so the server draws the browser image k times as wide, with `textSize`
//     stepped up to match so hgTracks makes the SAME layout decisions in that bigger image:
//     same tick spacing, same room for labels, same packing of features into rows. Without
//     the font, a wider image is a different picture rather than a bigger one.
//   * `zoom: 1/k` on the image table (SCALE_INIT below), handing that k-times-wider image
//     back the 1x amount of layout space. One image pixel then falls on exactly one device
//     pixel: native resolution, no resampling anywhere in the path.
//
// So the still comes out k times the 1x still in each dimension, showing the same figure --
// not a variation of it rendered in a bigger window.
const SCALE = Math.max(1, Number(process.env.DOCENT_SCALE || doc.scale || 1));
const [VW, VH] = doc.size || [1000, 760];
const PIX = Math.round((doc.pix || 850) * SCALE);
// hgTracks offers a fixed ladder of track font sizes (hgTracks/config.c); step to the one
// closest to scaling its 8px default, so rows and labels grow with the image instead of
// staying 8px tall in a 3x-wide picture.
const TEXTSIZE = [6, 8, 10, 12, 14, 18, 24, 34].reduce((a, b) =>
  Math.abs(b - 8 * SCALE) < Math.abs(a - 8 * SCALE) ? b : a);
// What every hgTracks nav carries: the image width, plus the font to draw it with at scale.
const IMGVARS = `pix=${PIX}` + (SCALE > 1 ? `&textSize=${TEXTSIZE}` : '');
// The tooltip's font-size, forced back to what a 1x run gives it (see SCALE_INIT). hgTracks
// takes it from the browser text size, which is TEXTSIZE on a scaled run, and then the device
// pixel ratio scales it a second time.
const SCALE_ARGS = { k: SCALE, tipPx: Math.round(TEXTSIZE / SCALE) };
// `pix` makes the image k times WIDER; nothing makes a fixed-height track taller. A bigLolly
// or wiggle row is a pixel count (`DEFAULT_HEIGHT_PER` = 128 in hg/inc/wiggle.h), read from
// trackDb/the cart and untouched by `pix` or `textSize` -- so a 128px row that was 15% of an
// 850px image is 5% of a 2550px one, which is how the ClinVar lollipop row came out a sliver
// with unreadable y-axis labels. A print run therefore asks for k times the height of every
// track a `track:` step turns on. It is harmless where it means nothing (a bigBed never reads
// heightPer) and each track's own `maxHeightPixels` still clamps it, so a track that should
// stay short does -- raise that ceiling in trackDb for one that should not.
// The k*128 is the DEFAULT height scaled, not each track's own: a row configured at 50px or
// 300px gets k*128 too, which is proportional only for the tracks that took the default. That
// covers every track a tour has used so far, and going further would mean reading each row's
// heightPer out of the cart before asking for k times it. If a tour ever wants a figure of a
// deliberately short or tall row, that is the fix -- the symptom is a row that comes back the
// wrong size in a scaled still and the right size at 1x.
const HEIGHTPER = SCALE > 1 ? Math.round(128 * SCALE) : 0;
// FAST: iterate on the FIGURES. Everything that exists only for the video is dropped --
// the dwells, the cursor animation, the dropdown theatrics, the screen recording and the
// mp4 transcode. The stills are byte-for-byte what a full run produces, and a run costs
// roughly a third as long. `fast: true` in the script, DOCENT_FAST=1, or `make FAST=1 BP1`.
// A scaled run is a figure run: at 3x the video would be a 3000px-wide recording of a tour
// nobody watches at that size, so the mp4 is skipped and only the stills are produced. Build
// the video from an unscaled run of the same script.
const FAST = !!(doc.fast || process.env.DOCENT_FAST || SCALE > 1);
const PACE = FAST ? 0 : Math.round((doc.pace ?? 1.2) * 1000);   // dwell after each step
const SHOTHOLD = FAST ? 0 : Math.round((doc.shotHold ?? 2.2) * 1000);  // extra pause at a shot

// ---------- trackDb ----------
// Docent carries NO table of per-track cart variables. Such a table encodes one snapshot
// of trackDb and then quietly lies when trackDb changes (this file used to pin
// `clinvarMain=dense` for every `clinvar:` step, for instance). Instead ask the server for
// the trackDb it is driving -- hubApi /list/tracks -- and derive what a step needs:
//
//   * the containers above a subtrack (composite, view, superTrack) that have to be
//     turned on with it, and what each of those takes (a superTrack wants show/hide),
//   * whether trackDb leaves that subtrack UNSELECTED (`parent <c> off`), in which case
//     the subtrack checkbox `<name>_sel` has to come along,
//   * which leaf actually draws the pixels for a container name.
//
// Everything else -- which dropdown to open, which row to hover -- is read off the live
// page. Tracks the listing doesn't know (attached hubs, custom tracks, quickLift's own
// tracks on the target) fall back to a literal `name=mode`, which is all Docent could
// honestly do for them anyway.
const TDB_TTL = 24 * 3600 * 1000;                              // re-fetch a cached listing daily
const TDB_CACHE = path.join(os.tmpdir(), `docent-tdb-${SERVER.replace(/[^\w.-]+/g, '_')}`);
let tdbPending = null;                                         // db -> Promise<Map|null>, once each
function tdbParse(genome) {
  const idx = new Map();
  const add = (name, o) => {
    const p = String(o.parent || '').trim().split(/\s+/);
    idx.set(name, {
      name,
      parent: p[0] || null,
      parentState: (p[1] || '').toLowerCase(),                 // on | off | a visibility | ''
      vis: o.visibility || null,
      view: o.view || null,
      superChild: !!o.superTrack,                              // member of a superTrack
      superTrack: false,                                       // set below for containers
      children: [],
    });
    for (const [k, v] of Object.entries(o))
      if (v && typeof v === 'object' && !Array.isArray(v)) add(k, v);
  };
  for (const [k, v] of Object.entries(genome || {}))
    if (v && typeof v === 'object' && !Array.isArray(v)) add(k, v);
  // hubApi flattens superTrack members to the top level and never lists the superTrack
  // itself, so synthesize the container from the `parent` field it points at.
  for (const n of [...idx.values()]) {
    if (!n.parent) continue;
    if (!idx.has(n.parent))
      idx.set(n.parent, { name: n.parent, parent: null, parentState: '', vis: null, view: null,
                          superChild: false, superTrack: true, children: [] });
    idx.get(n.parent).children.push(n.name);
  }
  return idx;
}
// The cache holds the DERIVED index (a few hundred kB), not hubApi's reply (~30 MB for
// hg38): same information for our purposes, ~20x less to read and parse on every run.
function tdbFlatten(idx) {
  return { docentIndex: 1,
           rows: [...idx.values()].map(n => [n.name, n.parent, n.parentState, n.vis, n.view,
                                             n.superChild ? 1 : 0, n.superTrack ? 1 : 0]) };
}
function tdbInflate(o) {
  const idx = new Map();
  for (const [name, parent, parentState, vis, view, superChild, superTrack] of o.rows)
    idx.set(name, { name, parent, parentState, vis, view,
                    superChild: !!superChild, superTrack: !!superTrack, children: [] });
  for (const n of idx.values()) if (n.parent && idx.has(n.parent)) idx.get(n.parent).children.push(n.name);
  return idx;
}
async function tdbIndex(db) {
  if (!tdbPending) tdbPending = new Map();
  if (tdbPending.has(db)) return tdbPending.get(db);
  const p = (async () => {
    const cache = `${TDB_CACHE}-${db}.json`;
    try {
      const st = fs.statSync(cache);
      if (Date.now() - st.mtimeMs < TDB_TTL) {
        const o = JSON.parse(fs.readFileSync(cache, 'utf8'));
        return o && o.docentIndex ? tdbInflate(o) : tdbParse(o);
      }
    } catch (e) {}
    const url = `${SERVER}/hubApi/list/tracks?genome=${enc(db)}&trackLeavesOnly=0`;
    try {
      const r = await fetch(url);
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      const j = await r.json();
      const genome = j[db];
      // A hub-supplied genome (an assembly hub, or quickLift's own generated target hub)
      // isn't in the server's trackDb listing at all -- expected, not a failure.
      if (!genome) {
        console.log(`trackDb: ${db} is not a server assembly (hub genome), `
          + `so track steps there are sent as literal name=mode`);
        return null;
      }
      const idx = tdbParse(genome);
      // Write via a unique temp file + rename so parallel builds (make -j) can't read a
      // half-written cache.
      try {
        const tmp = `${cache}.${process.pid}.tmp`;
        fs.writeFileSync(tmp, JSON.stringify(tdbFlatten(idx)));
        fs.renameSync(tmp, cache);
      } catch (e) {}
      console.log(`trackDb: ${idx.size} tracks for ${db} from ${SERVER}/hubApi`);
      return idx;
    } catch (e) {
      console.warn(`trackDb: could not read ${url} (${e.message}) -- `
        + `falling back to literal name=mode for every track step`);
      return null;
    }
  })();
  tdbPending.set(db, p);
  return p;
}

const sleep = ms => new Promise(r => setTimeout(r, ms));
// A pause that exists only so a viewer can follow the video: skipped entirely in FAST.
const dwell = ms => (FAST ? Promise.resolve() : sleep(ms));
// Typing is shown on screen for the video; in FAST just put the text in the box (fill()
// still fires the input events the autocompletes listen for).
async function typeIn(pg, sel, text) {
  if (FAST) await pg.fill(sel, String(text));
  else await pg.type(sel, String(text), { delay: 45 });
}
const enc = s => encodeURIComponent(String(s));
const state = { db: doc.db || 'hg38', position: doc.position || '', hgsid: '' };

// ---------- SCALE: give the k-times-wider browser image the 1x amount of layout space ----------
// hgTracks sizes the image table in the HTML it writes, so shrinking the <img> elements alone
// would leave the table 3x wider than its own contents. Zoom the table: the zoom reaches the
// images inside it, and its box goes back to the width the 1x page gives it -- 854 CSS px for
// a pix=850 run, whatever k is -- so the rest of the page is laid out exactly as at 1x while
// the image keeps all k times its pixels (one per device pixel at deviceScaleFactor: k).
//
// Installed for the whole run rather than at the shutter: the tour's own geometry -- a drag
// across the image, a mouseover on a feature -- then works in the same coordinates a 1x run
// works in, and needs no scale arithmetic of its own.
//
// The tooltip needs the opposite correction. It is a DOM element, so deviceScaleFactor
// already draws it k times bigger -- and hgTracks sets its font-size from the BROWSER TEXT
// SIZE (`window.browserTextSize` -> hg/js/utils.js addMouseover, hgTracks.js #mouseOverText),
// which a print run has just multiplied by k for the image. Both scalings land on the same
// text, so a 3x still gets a tooltip 3x too big -- the popups swamp the figure and the last
// one pinned falls off the crop. Pin the font-size back to the 1x value (TEXTSIZE / k); it
// is written as an inline style, so the rule has to be !important to win. `.__pinnedTip` is
// a recorded tooltip re-injected by pinShot() (which strips the id, keeps the class).
const SCALE_INIT = ({ k, tipPx }) => {
  const add = () => {
    if (document.getElementById('__scale')) return;
    const s = document.createElement('style');
    s.id = '__scale';
    s.textContent = `#imgTbl, #chromIdeoImg, img[src*="hgtIdeo"] { zoom: ${1 / k} !important; }`
      + `\n#mouseoverContainer, #mouseOverText, .tooltip, .__pinnedTip { font-size: ${tipPx}px !important; }`;
    (document.head || document.documentElement).appendChild(s);
  };
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', add);
  else add();
};

// ---------- animated cursor (same technique as the walkthrough-video skill's record.js) ----------
// The glyph and its box are shared with pinShot(), which draws a STATIC copy at every
// pinned mouseover so a combined figure shows where each tooltip was raised from. Keep
// them one definition: a pinned cursor that did not match the animated one would read as
// a different pointer rather than as the same tour paused.
const CURSOR_BOX = 'position:fixed;left:0;top:0;z-index:2147483647;pointer-events:none;'
  + 'width:24px;height:24px;margin-left:-3px;margin-top:-2px;filter:drop-shadow(0 1px 1px rgba(0,0,0,.4));';
const CURSOR_SVG = '<svg width="24" height="24" viewBox="0 0 24 24"><path d="M3 2 L3 19 L7.5 14.5 L10.5 21.5 L13.5 20.2 L10.6 13.5 L17 13.5 Z" fill="#111" stroke="#fff" stroke-width="1.3"/></svg>';
const CURSOR_INIT = ({ box, svg }) => {
  const add = () => {
    if (document.getElementById('__cur')) return;
    const c = document.createElement('div');
    c.id = '__cur';
    c.style.cssText = box;
    c.innerHTML = svg;
    document.documentElement.appendChild(c);
    const place = (x, y) => { c.style.transform = `translate(${x}px,${y}px)`; };
    place(120, 120);
    document.addEventListener('mousemove', e => place(e.clientX, e.clientY), true);
    document.addEventListener('mousedown', e => {
      const r = document.createElement('div');
      r.className = '__ripple';
      r.style.cssText = `position:fixed;left:${e.clientX}px;top:${e.clientY}px;z-index:2147483645;pointer-events:none;width:6px;height:6px;margin:-3px 0 0 -3px;border:3px solid rgba(225,30,30,.95);border-radius:50%;`;
      document.documentElement.appendChild(r);
      r.animate([{ transform: 'scale(1)', opacity: 1 }, { transform: 'scale(6)', opacity: 0 }], { duration: 520, easing: 'ease-out' });
      setTimeout(() => r.remove(), 540);
    }, true);
  };
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', add);
  else add();
};

function absurl(u) {
  if (/^https?:/.test(u)) return u;
  if (u.startsWith('/cgi-bin/')) return SERVER.replace(/\/cgi-bin$/, '') + u;
  if (u.startsWith('/')) return SERVER.replace(/\/cgi-bin$/, '') + u;
  return SERVER + '/' + u;
}

const T_START = Date.now();
(async () => {
  fs.mkdirSync(STILLDIR, { recursive: true });
  const browser = await chromium.launch({ headless: true, args: ['--force-color-profile=srgb'] });
  const ctx = await browser.newContext({
    viewport: { width: VW, height: VH }, deviceScaleFactor: SCALE,
    ...(FAST ? {} : { recordVideo: { dir: path.join(HERE, '.vid_' + base), size: { width: VW, height: VH } } }),
  });
  if (SCALE > 1) await ctx.addInitScript(SCALE_INIT, SCALE_ARGS);
  await ctx.addInitScript(CURSOR_INIT, { box: CURSOR_BOX, svg: CURSOR_SVG });
  await ctx.addInitScript(() => { try { localStorage.setItem('hgTracks_hideTutorial', '1'); } catch (e) {} });
  const page = await ctx.newPage();
  const cur = { x: 120, y: 120 };
  const pinnedTips = [];   // recorded mouseover tooltips for the next pinShot (per view)

  async function captureState() {
    try {
      const u = new URL(page.url());
      const h = u.searchParams.get('hgsid'); if (h) state.hgsid = h;
      const db = u.searchParams.get('db'); if (db) state.db = db;
      const p = u.searchParams.get('position'); if (p) state.position = p;
    } catch (e) {}
    // An interactive zoom / drag-select reload stores the new window in the CART, not the
    // URL, so read the live position straight from hgTracks when we're on a tracks page --
    // otherwise a later position-based nav (e.g. turning on a track) reverts the zoom.
    try {
      const pos = await page.evaluate(() => {
        try { if (typeof hgTracks !== 'undefined' && hgTracks.chromName)
          return hgTracks.chromName + ':' + (hgTracks.winStart + 1) + '-' + hgTracks.winEnd; } catch (_) {}
        return null;
      });
      if (pos) state.position = pos;
    } catch (e) {}
    // Authoring aid: DOCENT_ROWS=1 logs the rows hgTracks actually drew, so "why is that
    // subtrack still there / why is my mouseover track not shown" is one run, not a guess.
    if (process.env.DOCENT_ROWS) {
      const rows = await page.evaluate(() =>
        [...document.querySelectorAll('[id^="img_data_"]')].map(e => e.id.replace('img_data_', ''))).catch(() => null);
      if (rows) console.log('  rows:', rows.join(', ') || '(none)');
    }
    await scaleHeights();   // print runs only; see below
  }
  // A print run makes the image k times wider, and a track with a FIXED PIXEL height does not
  // follow: a bigLolly or wiggle row is a pixel count read from trackDb/the cart, untouched by
  // `pix` and `textSize`, so a 128px row that was 15% of an 850px image is 5% of a 2550px one.
  // That is how the ClinVar lollipop row came out a sliver with unreadable y-axis labels next to
  // a bed track that DID grow with the font. So after each view change, ask for k times the
  // height of every row hgTracks just drew. It is asked of the rows the page actually has --
  // which is the only way to reach the LIFTED view, whose tracks are hub tracks under names
  // trackDb never saw. Harmless where it means nothing (a bigBed never reads heightPer), and
  // each track's own `maxHeightPixels` still clamps it, so a track that should stay short does;
  // raise that ceiling in trackDb for one that should not (clinvarSubLolly does this).
  const heightsSent = new Set();
  let inHeightNav = false;
  async function scaleHeights() {
    if (!HEIGHTPER || inHeightNav) return;
    const drawn = await page.evaluate(() =>
      [...document.querySelectorAll('#imgTbl [id^="img_data_"]')].map(e => e.id.replace('img_data_', ''))
    ).catch(() => []);
    const fresh = (drawn || []).filter(n => n && !heightsSent.has(n));
    if (!fresh.length) return;                      // same rows as last time: no second load
    fresh.forEach(n => heightsSent.add(n));
    inHeightNav = true;                             // the nav below must not recurse
    try { await nav(`/cgi-bin/hgTracks?${fresh.map(n => `${n}.heightPer=${HEIGHTPER}`).join('&')}&${IMGVARS}`); }
    finally { inHeightNav = false; }
  }
  // Apache's LimitRequestLine defaults to 8190 bytes for the whole request line, and a
  // step that derives a lot of cart variables can sail past it. The server then answers 414
  // and the page LOADS -- so nothing throws, captureState finds no image, and the next
  // shot: quietly photographs "Request-URI Too Long". Say so, since only an eyeball on the
  // still would otherwise catch it.
  const URL_WARN = 7800;
  async function nav(u) {
    const full = absurl(u);
    if (full.length > URL_WARN)
      console.warn(`nav: URL is ${full.length} chars, over Apache's usual ${8190} limit `
        + `-- expect a 414 "Request-URI Too Long" page instead of the view`);
    pinnedTips.length = 0;
    await page.goto(full, { waitUntil: 'load' });
    await captureState();
    await page.mouse.move(cur.x, cur.y);
  }
  async function glide(x, y) {
    if (FAST) { await page.mouse.move(x, y); cur.x = x; cur.y = y; return; }
    const steps = Math.max(10, Math.round(Math.hypot(x - cur.x, y - cur.y) / 9));
    for (let i = 1; i <= steps; i++) { await page.mouse.move(cur.x + (x - cur.x) * i / steps, cur.y + (y - cur.y) * i / steps); await sleep(15); }
    cur.x = x; cur.y = y;
  }
  async function glideTo(sel) {
    const b = await page.locator(sel).first().boundingBox({ timeout: 8000 }).catch(() => null);
    if (b) await glide(b.x + b.width / 2, b.y + b.height / 2);
  }
  async function clickGlide(sel) { await glideTo(sel); await sleep(160); await page.click(sel); }
  async function checkGlide(sel, want) {
    await glideTo(sel); await sleep(140);
    if (want) await page.check(sel).catch(() => {}); else await page.uncheck(sel).catch(() => {});
  }
  // visibly open a native <select>, highlight the target, (optionally) commit, collapse.
  // commit=false leaves the real value untouched (just shows the pick) -- used for the
  // track-controls gesture, where the actual state is applied by a follow-up nav().
  async function openSelectVisible(sel, val, rows = 12, commit = true) {
    const loc = page.locator(sel).first();
    if (FAST) {   // no one is watching: just set it (the open/highlight is video-only)
      if (commit) { await loc.selectOption(val).catch(() => {}); await sleep(120); }
      return;
    }
    await loc.evaluate(el => { try { el.scrollIntoView({ block: 'center' }); } catch (e) { el.scrollIntoView(); } });
    await sleep(350); await glideTo(sel); await sleep(300);
    await loc.evaluate((el, r) => {
      const b = el.getBoundingClientRect();
      el.dataset._sz = el.size || 1; el.dataset._cs = el.style.cssText;
      el.style.position = 'fixed'; el.style.left = b.left + 'px'; el.style.top = b.top + 'px';
      el.style.width = 'auto'; el.style.minWidth = b.width + 'px'; el.style.zIndex = '2147483646';
      el.style.background = '#fff'; el.style.border = '1px solid #888'; el.style.boxShadow = '0 4px 12px rgba(0,0,0,.35)';
      el.size = Math.min(r, el.options.length);
    }, rows);
    await sleep(650);
    await loc.evaluate((el, v) => { const i = [...el.options].findIndex(o => o.value === v); if (i >= 0) { el.selectedIndex = i; try { el.options[i].scrollIntoView({ block: 'center' }); } catch (e) {} } }, val);
    await sleep(1000);
    if (commit) {
      await loc.evaluate((el, v) => { el.value = v; el.dispatchEvent(new Event('change', { bubbles: true })); }, val);
      await sleep(250);
    }
    await loc.evaluate(el => { el.size = parseInt(el.dataset._sz) || 1; el.style.cssText = el.dataset._cs || ''; }).catch(() => {});
  }
  // ---------- position-box suggestions (the gene-name path of goShow) ----------
  // Rows of the open suggest menu, minus the category headings (and the trailing
  // "Unable to find a genome?" div, which is not an <li>).
  const SUGGEST_ROW = 'ul.ui-autocomplete li:not(.ui-autocomplete-category):visible';
  async function suggestRows() {
    return await page.evaluate(() => {
      const rows = [...document.querySelectorAll('ul.ui-autocomplete li')]
        .filter(li => !li.classList.contains('ui-autocomplete-category') && li.offsetWidth > 0);
      return rows.map(li => {
        let d = null;
        try { if (window.jQuery) d = jQuery(li).data('ui-autocomplete-item') || null; } catch (e) {}
        return {
          text: (li.innerText || li.textContent || '').trim(),
          // The item's own identifier, preferred over its display label: a gene suggestion
          // carries geneSymbol, a genome suggestion (the Convert page's target search)
          // carries db/genome. `value` comes LAST because jQuery UI copies the label into
          // it when the item has none, which would make every row "match" its own text.
          sym: d ? String(d.geneSymbol || d.db || d.genome || d.value || '') : '',
          recent: !!(d && d.displayCategory === 'Recent'),       // a previously-visited position
        };
      });
    });
  }
  // Choose the suggestion row for `term`, waiting out the hgSuggest ajax. Two traps this
  // handles: the "Recent" positions render INSTANTLY, so a plain "menu is up" wait picks a
  // recent position instead of the gene; and when the real suggestions arrive the menu is
  // re-rendered, so a row chosen too early is gone by the time we click it. So: poll until
  // a row actually matches AND the menu has stopped changing. `want` (goShow's `pick:`)
  // overrides the match, for a term with several sensible hits. Returns {index, text} into
  // the SUGGEST_ROW set, or null if nothing ever matched.
  async function pickSuggest(term, want, ms = 8000) {
    const t0 = Date.now();
    const low = s => String(s).toLowerCase();
    const demote = (s, r) => (s ? s - (r.recent ? 0.5 : 0) : 0);  // a real hit beats a recent one
    // A term that appears as a whole token in the row text counts as a real match -- an
    // accession sits in parentheses ("human (HG02148.mat 2021) (GCA_018471535.1)"), so
    // neither an id compare nor startsWith would catch it.
    const tokenRe = new RegExp(`(^|[^A-Za-z0-9_.])${String(term).replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}([^A-Za-z0-9_.]|$)`, 'i');
    const scoreTerm = r => demote(
      low(r.sym) === low(term) ? 3 :                              // the item's own id
      low(r.text).startsWith(low(term)) ? 2 :                     // label starts with the term
      tokenRe.test(r.text) ? 2 :                                  // term appears as a token
      low(r.text).includes(low(term)) ? 1 : 0, r);
    const scoreWant = r => demote(want && low(r.text).includes(low(want)) ? 3 : 0, r);
    const pickBest = (rows, fn) => {
      let bi = -1, bs = 0;
      rows.forEach((r, i) => { const s = fn(r); if (s > bs) { bs = s; bi = i; } });
      return bi < 0 ? null : { index: bi, text: rows[bi].text, score: bs };
    };
    const deadline = Date.now() + ms;
    let prev = null, byTerm = null, byWant = null;
    while (Date.now() < deadline) {
      const rows = await suggestRows();
      const sig = JSON.stringify(rows.map(r => r.text));
      // Same authoring aid as the drawn-row dump: shows what the menu offered and what
      // identifier each row carries, which is how you tell why a pick went elsewhere.
      if (process.env.DOCENT_ROWS && sig !== prev) console.log('  suggest:', JSON.stringify(rows));
      byTerm = pickBest(rows, scoreTerm) || byTerm;
      byWant = want ? (pickBest(rows, scoreWant) || byWant) : null;
      const b = want ? byWant : byTerm;
      // matched, and the menu settled
      if (b && b.score >= 2 && sig === prev) return { ...b, ms: Date.now() - t0 };
      prev = sig;
      await sleep(400);
    }
    // `pick:` that matches nothing shouldn't throw away the gene: fall back to the plain
    // term match (and say so) rather than skipping the menu and submitting raw text.
    if (want && !byWant && byTerm)
      console.warn(`goShow ${term}: pick "${want}" matched no suggestion, using "${byTerm.text}"`);
    const out = want ? (byWant || byTerm) : byTerm;
    return out ? { ...out, ms: Date.now() - t0, timedOut: true } : null;
  }
  // ---------- what a `track:` step has to send, per trackDb ----------
  // Cart variables that put `name` in `mode`, as [key, value] pairs: the track itself, its
  // subtrack checkbox if trackDb leaves it unselected, and every container above it -- a
  // composite or view takes the mode, a superTrack takes show. Nothing is pushed DOWNWARD:
  // a container's visibility reaches its selected children on its own (`clinvar=pack` draws
  // clinvarMain/clinvarCnv/clinvarSubLolly, `dbSnp155Composite=pack` draws dbSnp155Common),
  // which is why the author only has to name deviations from trackDb. A name trackDb doesn't
  // know (hub, custom, quickLift target) gets a literal `name=mode`.
  async function visVars(name, mode) {
    const idx = await tdbIndex(state.db);
    const n = idx && idx.get(name);
    if (!n) return [[name, mode]];
    if (n.superTrack && !['show', 'hide'].includes(mode)) {
      console.warn(`track ${name}: superTrack container takes show/hide, not "${mode}" -- using show`);
      mode = 'show';
    }
    const out = [[name, mode]];
    // A subtrack of a composite is governed by its CHECKBOX, not its visibility: when the
    // container's own vis var is in the same request, hgTracks reshapes the composite and a
    // bare `clinvarCnv=hide` is dropped (the cart keeps clinvarCnv_sel=1 and the row still
    // draws). So state the selection explicitly in both directions. Views are containers,
    // not selectable rows, so they are left out of this.
    const parent = n.parent ? idx.get(n.parent) : null;
    if (parent && !parent.superTrack && !n.children.length)
      out.push([`${name}_sel`, mode === 'hide' ? '0' : '1']);
    // Hiding a subtrack says NOTHING about its container: propagating `hide` upward would
    // turn the whole composite off (`{clinvar: pack, clinvarCnv: hide}` would end in
    // clinvar=hide and no ClinVar at all).
    if (mode === 'hide') return out;
    for (let p = n.parent; p; ) {
      const pn = idx.get(p);
      out.push([p, pn && pn.superTrack ? 'show' : mode]);
      p = pn ? pn.parent : null;
    }
    return out;
  }
  // Leaf descendants of a container, in trackDb order (a container draws no pixels of its
  // own, so a `mouseover:`/`click:` naming one has to be resolved to a row that does).
  async function tdbLeaves(name) {
    const idx = await tdbIndex(state.db);
    const n = idx && idx.get(name);
    if (!n || !n.children.length) return [];
    const out = [];
    const walk = k => {
      const c = idx.get(k);
      if (!c || !c.children.length) out.push(k);
      else c.children.forEach(walk);
    };
    n.children.forEach(walk);
    return out;
  }
  // What `hideKids` actually has to send. NOT the same thing as tdbLeaves(): hiding a
  // COMPOSITE already reaches its subtracks, so the walk stops at the first container that
  // propagates its own visibility and only keeps descending through superTracks, which do
  // not. hubApi never lists a superTrack container -- tdbParse synthesizes it and flags
  // superTrack -- so anything else holding children came from hubApi's own nesting and is a
  // composite or a view.
  //
  // Descending all the way to leaves here is how `{cCREs: hideKids}` on hg38 came to send
  // 1701 cart variables in a 42,020-character GET: the walk went straight through the
  // ENCODE4 Core Collection composite and enumerated all 850 of its ENCFF subtracks. Apache
  // answered 414 and the next shot: photographed the error page, with nothing failing the
  // build. Stopping at the composite makes that same step three names.
  async function tdbHideTargets(name) {
    const idx = await tdbIndex(state.db);
    const n = idx && idx.get(name);
    if (!n || !n.children.length) return [];
    const out = [];
    const walk = k => {
      const c = idx.get(k);
      if (!c || !c.children.length) { out.push(k); return; }   // a real leaf
      if (!c.superTrack) { out.push(k); return; }              // composite/view: hide as a unit
      c.children.forEach(walk);                                // superTrack: does not propagate
    };
    n.children.forEach(walk);
    return out;
  }
  // The track-controls dropdown to open for the visible gesture. Composite children have no
  // dropdown of their own (the container carries it), so walk up until the DOM has one.
  async function ctrlSelect(name) {
    const idx = await tdbIndex(state.db);
    for (let k = name; k; ) {
      const sel = `select[name="${k}"]`;
      if (await page.locator(sel).count()) return sel;
      const n = idx && idx.get(k);
      k = n ? n.parent : null;
    }
    return null;
  }
  async function shot(name) {
    const p = path.join(STILLDIR, name + '.png');
    // Drop any click ripple still fading: it reads as a red blob over whatever was just
    // clicked. It belongs to the video, not to a figure.
    await page.evaluate(() => document.querySelectorAll('.__ripple').forEach(e => e.remove())).catch(() => {});
    // If a mouseover tooltip is currently up, capture the image + tooltip together
    // (the tooltip is appended to <body>, so an #imgTbl element shot would clip it).
    const clip = await page.evaluate(() => {
      const im = document.getElementById('imgTbl');
      if (!im) return null;
      // Floating overlays to capture together with the image: the mouseover tooltip,
      // and any visible jQuery-UI dialog (e.g. the drag-select "Zoom In / Highlight" box).
      const overlays = [];
      const tip = document.getElementById('mouseoverContainer');
      if (tip && tip.offsetWidth > 0 &&
          getComputedStyle(tip).display !== 'none' && getComputedStyle(tip).visibility !== 'hidden') overlays.push(tip);
      for (const d of document.querySelectorAll('.ui-dialog')) if (d.offsetWidth > 0) overlays.push(d);
      if (!overlays.length) return null;
      const a = im.getBoundingClientRect();
      let x = a.left, y = a.top, x2 = a.right, y2 = a.bottom;
      for (const o of overlays) { const r = o.getBoundingClientRect(); x = Math.min(x, r.left); y = Math.min(y, r.top); x2 = Math.max(x2, r.right); y2 = Math.max(y2, r.bottom); }
      return { x: Math.max(0, x - 4), y: Math.max(0, y - 4), width: (x2 - x) + 8, height: (y2 - y) + 8 };
    });
    if (clip) {
      await page.screenshot({ path: p, clip });
    } else if (await page.locator('#imgTbl').count()) {
      await page.locator('#imgTbl').screenshot({ path: p });
    } else {
      // Not a tracks page (hgc detail page, an external page a link led to, ...). An element
      // shot of <main> would run the whole scrolling page -- thousands of pixels tall and
      // unusable as a figure. Capture the viewport only, i.e. the top of the page.
      await page.screenshot({ path: p });
    }
    console.log('SHOT', p);
    await sleep(SHOTHOLD);
  }
  // Resolve a track name to its DOM key + data-image and row bounding boxes. On a
  // quickLift/Convert target the tracks come from a hub, so ids gain a dynamic
  // `hub_<n>_` prefix -- match by suffix so the YAML can just say `track: quickLiftChain`.
  // A container draws nothing itself, so if the name given is one, fall through to the
  // leaves trackDb lists under it and take the first that is actually on the page.
  async function trackBox(t) {
    const cands = [t, ...await tdbLeaves(t)];
    let key = null;
    for (const c of cands) {
      key = await page.evaluate(k => {
        if (document.getElementById('img_data_' + k)) return k;
        const el = [...document.querySelectorAll('[id^="img_data_"]')]
          .find(e => e.id === 'img_data_' + k || e.id.endsWith('_' + k));
        return el ? el.id.replace('img_data_', '') : null;
      }, c);
      if (key) { if (c !== t) console.log(`track ${t}: drawn by "${c}"`); break; }
    }
    if (!key) throw new Error(`track "${t}" not shown (no #img_data_ for ${cands.join(', ')})`);
    const img = await page.locator(`#img_data_${key}`).first().boundingBox({ timeout: 8000 }).catch(() => null);
    const row = await page.locator(`#imgTbl tr#tr_${key}`).first().boundingBox({ timeout: 8000 }).catch(() => null);
    if (!img || !row) throw new Error(`track "${t}" not shown (need #img_data_${key} + #tr_${key})`);
    // Everything hgTracks reports about the image -- map-box coords, mouseOver spans,
    // insideX -- is in the pixels of the image the SERVER drew, which is not the size the
    // page shows it at when the image is scaled (SCALE). imgPx is that ratio, so those
    // numbers can be turned into page coordinates: 1 normally, 1/SCALE for a print render.
    const imgPx = await page.evaluate(k => {
      const im = document.getElementById('img_data_' + k);
      return im && im.naturalWidth ? im.getBoundingClientRect().width / im.naturalWidth : 1;
    }, key);
    return { key, img, row, imgPx: imgPx || 1 };
  }
  // Resolve a NAMED item to a point {x,y} + its map-box HREF (the item's hgc link). We
  // pick the map <area> whose href(&i=<name>)/title carries the name AND whose box sits in
  // this track's own ROW band (so stacked items on other rows don't win); fall back to the
  // JSON mouseOver spans (wig/dense tracks, no per-item href).
  async function itemXY(t, want, titleOnly) {
    const { key, img, row, imgPx } = await trackBox(t);
    const band = { top: row.y, bot: row.y + row.height };
    const area = await page.evaluate(({ want, titleOnly, band, imgBox }) => {
      const areas = [...document.querySelectorAll('map[name^="map_"] area')];
      const cands = [];
      for (const a of areas) {
        const href = a.getAttribute('href') || '';
        const title = a.getAttribute('title') || a.getAttribute('data-tooltip')
                   || a.getAttribute('mouseoverText') || '';
        const hay = titleOnly ? title : (href + ' ' + title);
        if (!hay.includes(want)) continue;
        const c = (a.getAttribute('coords') || '').split(',').map(Number);
        if (c.length < 4) continue;
        // origin = the image this map is attached to (fall back to the data image box)
        const m = a.closest('map'), nm = m && m.getAttribute('name');
        const im = nm && document.querySelector(`img[usemap="#${nm}"]`);
        const r = im ? im.getBoundingClientRect() : null;
        const ox = r ? r.left : imgBox.x, oy = r ? r.top : imgBox.y;
        // coords are in the drawn image's own pixels; s converts them to page pixels
        const s = (r && im.naturalWidth) ? r.width / im.naturalWidth : 1;
        const cx = ox + s * (c[0] + c[2]) / 2, cy = oy + s * (c[1] + c[3]) / 2;
        // The tooltip's own text, so the hover can wait for THIS item's tooltip rather than
        // for any tooltip at all (see mouseover()). Rendered exactly the way the tooltip
        // renders it -- innerHTML then textContent -- because the attribute holds markup AND
        // undecoded entities (`<b>`, `&#9733;`) that getAttribute hands back literally.
        const tmp = document.createElement('div');
        tmp.innerHTML = title;
        const tip = (tmp.textContent || '').replace(/\s+/g, ' ').trim();
        cands.push({ cx, cy, href, tip, inBand: cy >= band.top - 1 && cy <= band.bot + 1 });
      }
      const inBand = cands.filter(h => h.inBand);
      const pick = (inBand[0] || cands[0]) || null;
      return pick && { ...pick, n: cands.length, nBand: inBand.length,
                       all: cands.map(c => Math.round(c.cx)) };
    }, { want: String(want), titleOnly: !!titleOnly, band, imgBox: img });
    if (area) {
      if (process.env.DOCENT_ROWS)
        console.log(`  item "${want}" in ${t}: ${area.n} map box(es) match (${area.nBand} in row), `
                  + `centers x=[${area.all}] -> hovering (${Math.round(area.cx)},${Math.round(area.cy)})`
                  + (area.tip ? `, expecting tip "${area.tip.slice(0, 40)}"` : ''));
      return { x: area.cx, y: area.cy, href: area.href, tip: area.tip };
    }
    const span = await page.evaluate(({ keys, want }) => {
      const md = window.mapData; if (!md || !md.spans) return null;
      for (const k of keys) {
        const arr = md.spans[k]; if (!arr) continue;
        const s = arr.find(r => String(r.value || '').includes(want));
        if (s) return { x1: s.x1, x2: s.x2 };
      }
      return null;
    }, { keys: [key, t], want: String(want) });
    if (!span) {
      // Say WHAT is there instead. An item name that has gone missing is usually a track
      // whose items depend on the pixel width -- a print render (SCALE) draws a wider image,
      // so features hgTracks merged into one box at screen width come apart into several
      // with names of their own -- and the fix is to pick from the names that do exist.
      const near = await page.evaluate(({ band }) => {
        const out = [];
        for (const a of document.querySelectorAll('map[name^="map_"] area')) {
          const c = (a.getAttribute('coords') || '').split(',').map(Number);
          if (c.length < 4) continue;
          const m = a.closest('map'), nm = m && m.getAttribute('name');
          const im = nm && document.querySelector(`img[usemap="#${nm}"]`);
          if (!im) continue;
          const r = im.getBoundingClientRect();
          const s = im.naturalWidth ? r.width / im.naturalWidth : 1;
          const cy = r.top + s * (c[1] + c[3]) / 2;
          if (cy < band.top - 1 || cy > band.bot + 1) continue;
          const i = (a.getAttribute('href') || '').match(/[?&]i=([^&]+)/);
          if (i) out.push(decodeURIComponent(i[1]));
        }
        return out;
      }, { band }).catch(() => []);
      const win = await page.evaluate(() => {
        try { return `${hgTracks.chromName}:${hgTracks.winStart}-${hgTracks.winEnd}`; }
        catch (_) { return '?'; }
      }).catch(() => '?');
      const show = process.env.DOCENT_ROWS ? near : near.slice(0, 12);
      throw new Error(`item "${want}" not found in track "${t}" (searched map-box areas + `
        + `mouseOver spans). Window ${win}.${near.length ? ` In that row: ${show.join(', ')}`
          + `${show.length < near.length ? `, ... (${near.length} total; DOCENT_ROWS=1 for all)` : ''}`
          : ''}`);
    }
    return { x: img.x + imgPx * (span.x1 + span.x2) / 2, y: row.y + row.height / 2, href: null };
  }
  // POSITIONAL point: at:/frac:/x: -> x, y forced to the track row's middle. The grey
  // side-label strip (insideX) is baked into the image's left, so a fraction/coord maps
  // across [img.x+insideX, img.x+img.width], not the whole image width.
  async function posXY(t, o) {
    const { img, row, imgPx } = await trackBox(t);
    const insideX = imgPx * await page.evaluate(() => { try { return hgTracks.insideX || 0; } catch (_) { return 0; } });
    let x;
    if (o.x != null) x = img.x + insideX + imgPx * Number(o.x);
    else {
      const frac = (o.frac != null) ? Number(o.frac)
        : (o.at != null) ? await page.evaluate(at => {
            try { const s = hgTracks.winStart, e = hgTracks.winEnd;
              const c = +String(at).replace(/.*:/, '').replace(/,/g, '');
              return Math.max(0, Math.min(1, (c - s) / (e - s))); } catch (_) { return 0.5; }
          }, o.at)
        : 0.5;
      x = img.x + insideX + frac * (img.width - insideX);
    }
    return { x, y: row.y + row.height / 2 };
  }
  // Hover an item to raise its mouseover tooltip (real mousemove -> the browser's own
  // tooltip). Two ways to place the cursor:
  //   IDENTITY  `item:` / `title:` / `value:` -> name the item (lands on the right ROW).
  //   POSITION  `at:` (genomic coord) / `frac:` (0..1) / `x:` (raw px) -> a point.
  async function mouseover(o) {
    if (typeof o === 'string') o = { track: o };
    o = o || {};
    const t = o.track; if (!t) throw new Error('mouseover: needs a track');
    const want = o.item ?? o.title ?? o.value;         // identity mode if any is set
    const spot = (want != null)
      ? await itemXY(t, want, o.title != null && o.item == null && o.value == null)
      : await posXY(t, o);
    const { x, y } = spot;
    // Raise a FRESH tooltip for THIS item, and be sure it IS this item's. The browser shows
    // a tooltip on MOUSEENTER after a 500ms delay and hides it 500ms after mouseleave
    // (hg/js/utils.js addMouseover), so while the cursor glides in it crosses other items
    // and any of THEIR tooltips can still be on screen when we arrive -- an Alignment
    // Differences item recorded as "identical" (the neighbouring aligned block, lingering in
    // its grace period) when the item under the cursor reads "mismatch C->T". Waiting for
    // "some tooltip is visible" is therefore not enough: when we know the item's own text
    // (from its map box) we wait for exactly that.
    const tipHtml = () => page.evaluate(() => {
      const c = document.getElementById('mouseoverContainer');
      if (!c || !c.offsetWidth) return null;
      const st = getComputedStyle(c);
      return (st.display === 'none' || st.visibility === 'hidden') ? null : c.innerHTML;
    });
    const prevTip = await tipHtml();
    await page.mouse.move(2, y); cur.x = 2; cur.y = y;
    if (prevTip) await page.waitForFunction(() => {
      const c = document.getElementById('mouseoverContainer');
      if (!c || !c.offsetWidth) return true;
      const st = getComputedStyle(c);
      return st.display === 'none' || st.visibility === 'hidden';
    }, null, { timeout: 2000 }).catch(() => {});
    await sleep(60);
    await glide(x, y);
    // small jiggle so the mousemove handler definitely fires and positions the tooltip
    await page.mouse.move(x + 1, y); await sleep(60); await page.mouse.move(x, y);
    const shown = () => page.evaluate(() => {
      const c = document.getElementById('mouseoverContainer');
      if (!c || !c.offsetWidth) return null;
      const st = getComputedStyle(c);
      if (st.display === 'none' || st.visibility === 'hidden') return null;
      return (c.textContent || '').replace(/\s+/g, ' ').trim();
    });
    const wantTip = spot.tip || null;
    if (wantTip) {
      // The item's own tooltip, or nothing. A neighbour's tooltip lingering from the glide
      // fails this test, so we keep waiting until the 500ms show timer fires for OUR item.
      // Compared with ALL whitespace removed: the title's markup (`<b>rsID</b>: ...`, `<br>`)
      // leaves no whitespace at all in textContent, so any tag-to-space normalisation would
      // never match and the wait would just burn its timeout on a tooltip that was right.
      await page.waitForFunction(w => {
        const c = document.getElementById('mouseoverContainer');
        if (!c || !c.offsetWidth) return false;
        const st = getComputedStyle(c);
        if (st.display === 'none' || st.visibility === 'hidden') return false;
        const flat = z => z.replace(/\s+/g, '');
        // A distinctive PREFIX, not the whole string: the head of a mouseOver carries the
        // item's identity (its name/HGVS), while the tail can render differently from the
        // title it came from (entities, stars, a max-width span). Short tips match whole.
        return flat(c.textContent || '').includes(flat(w).slice(0, 60));
      }, wantTip, { timeout: 4000 }).catch(() => {});
      const flat = z => (z || '').replace(/\s+/g, '');
      if (process.env.DOCENT_ROWS && !flat(await shown()).includes(flat(wantTip).slice(0, 60)))
        console.warn(`  WARNING: mouseover ${t} "${want}": tooltip never showed its own text\n`
                   + `      want: ${JSON.stringify(flat(wantTip).slice(0, 70))}\n`
                   + `      got : ${JSON.stringify(flat(await shown()).slice(0, 70))}`);
    } else {
      await page.waitForFunction(prev => {
        const c = document.getElementById('mouseoverContainer');
        if (!c || !c.offsetWidth) return false;
        const st = getComputedStyle(c);
        if (st.display === 'none' || st.visibility === 'hidden') return false;
        return prev == null || c.innerHTML !== prev;
      }, prevTip, { timeout: 3000 }).catch(() => {});
    }
    // A POSITIONAL hover has no expected text to wait for, so the best it can do is let the
    // tooltip settle: the content stops changing once the cursor is parked, so sample until
    // two reads agree. (A pinned positional hover therefore still records whatever is under
    // the point -- `frac: 0.5` can land between two features and report the block they sit
    // in. Name the item when the figure depends on which tooltip it is.)
    if (!wantTip) {
      let settled = await tipHtml();
      for (let i = 0; i < 15; i++) {
        await sleep(80);
        const now = await tipHtml();
        if (now && now === settled) break;
        settled = now;
      }
    }
    if (process.env.DOCENT_ROWS)
      console.log(`  tip at (${Math.round(x)},${Math.round(y)}): `
        + JSON.stringify(((await shown()) || '').slice(0, 90)));
    // Optionally RECORD this tooltip so a later `pinShot:` can show several mouseovers
    // open together in one figure. We only record (position + the tooltip's own HTML)
    // here -- nothing is injected into the recorded page, so the mp4 still shows just the
    // transient native tooltip. `pin:` on the step overrides the document-level
    // `pinMouseovers:` default. Records accumulate within a view and are cleared on nav.
    const pin = (o.pin != null) ? o.pin : (doc.pinMouseovers === true);
    if (pin) await recordTip(x, y);
    if (!FAST) await sleep(o.hold != null ? Number(o.hold) * 1000 : SHOTHOLD);
    if (o.shot) await shot(o.shot);
  }
  // Grab the live mouseover tooltip's HTML and anchor it at the ITEM's coordinate (x,y
  // that mouseover just hovered), expressed RELATIVE TO the track image (#imgTbl). The
  // browser parks its own tooltip at a near-fixed spot, so two tips would stack; anchoring
  // to the item keeps each pinned tooltip on its own feature (and robust to the throwaway
  // page's image sitting at a different offset).
  // cx/cy is the HOVER POINT itself (also image-relative), kept alongside the tooltip's
  // own offset so pinShot() can draw a cursor exactly where the tip was raised from.
  async function recordTip(x, y) {
    const t = await page.evaluate(({ x, y }) => {
      const c = document.getElementById('mouseoverContainer');
      if (!c || !c.offsetWidth) return null;
      const im = document.getElementById('imgTbl');
      const ir = im ? im.getBoundingClientRect() : { left: 0, top: 0 };
      return { cx: x - ir.left, cy: y - ir.top,
               dx: x - ir.left + 8, dy: y - ir.top + 8, html: c.outerHTML };
    }, { x, y });
    if (t) pinnedTips.push(t);
  }
  // Render every recorded tooltip open at once in a still, WITHOUT touching the recorded
  // page (so the mp4 is unaffected): reload the current view on a throwaway page that
  // shares the session cookie (cart), inject the recorded tooltips, screenshot, discard.
  // Bare string is the still's name; the map form adds `cursors:` (default true) to draw
  // a static pointer at every pinned hover point, so a combined figure says which feature
  // each tooltip came off rather than leaving the reader to infer it from the anchor.
  async function pinShot(arg) {
    const o = (arg && typeof arg === 'object') ? arg : { name: arg };
    const name = o.name ?? o.shot;
    const cursors = (o.cursors != null) ? o.cursors !== false : true;
    if (!pinnedTips.length) { console.warn(`pinShot ${name}: no pinned mouseovers recorded (set pin: true / pinMouseovers: true)`); return; }
    const url = page.url();
    const ctx2 = await browser.newContext({ viewport: { width: VW, height: VH }, deviceScaleFactor: SCALE });
    if (SCALE > 1) await ctx2.addInitScript(SCALE_INIT, SCALE_ARGS);
    await ctx2.addCookies(await ctx.cookies());
    const pg2 = await ctx2.newPage();
    await pg2.goto(url, { waitUntil: 'load' });
    await pg2.waitForSelector('#imgTbl', { timeout: 8000 }).catch(() => {});
    await pg2.evaluate(({ tips, cursors, box, svg }) => {
      window.scrollTo(0, 0);
      const im = document.getElementById('imgTbl');
      const ir = im ? im.getBoundingClientRect() : { left: 0, top: 0 };
      for (const t of tips) {
        const wrap = document.createElement('div');
        wrap.innerHTML = t.html;
        const el = wrap.firstElementChild; if (!el) continue;
        el.removeAttribute('id');
        el.classList.add('__pinnedTip');
        el.style.position = 'fixed';
        el.style.left = (ir.left + t.dx) + 'px'; el.style.top = (ir.top + t.dy) + 'px';
        el.style.opacity = '1'; el.style.visibility = 'visible';
        el.style.display = 'inline-block'; el.style.pointerEvents = 'none';
        document.documentElement.appendChild(el);
        // A pointer at the hover point, drawn the same way the live overlay draws it.
        // It lands on the tooltip's top-left corner, which is exactly where a real
        // screenshot of that hover would put it.
        if (cursors && t.cx != null) {
          const c = document.createElement('div');
          c.className = '__pinnedCursor';
          c.style.cssText = box;
          c.innerHTML = svg;
          c.style.transform = `translate(${ir.left + t.cx}px,${ir.top + t.cy}px)`;
          document.documentElement.appendChild(c);
        }
      }
    }, { tips: pinnedTips, cursors, box: CURSOR_BOX, svg: CURSOR_SVG });
    const p = path.join(STILLDIR, name + '.png');
    const clip = await pg2.evaluate(() => {
      const im = document.getElementById('imgTbl'); if (!im) return null;
      const els = [im, ...document.querySelectorAll('.__pinnedTip, .__pinnedCursor')];
      let x = Infinity, y = Infinity, x2 = -Infinity, y2 = -Infinity;
      for (const o of els) { const r = o.getBoundingClientRect(); x = Math.min(x, r.left); y = Math.min(y, r.top); x2 = Math.max(x2, r.right); y2 = Math.max(y2, r.bottom); }
      return { x: Math.max(0, x - 4), y: Math.max(0, y - 4), width: (x2 - x) + 8, height: (y2 - y) + 8 };
    });
    if (clip) await pg2.screenshot({ path: p, clip });
    else await pg2.locator('#imgTbl').screenshot({ path: p });
    await ctx2.close();
    console.log('SHOT', p, `(pinned: ${pinnedTips.length})`);
    pinnedTips.length = 0;   // consume the set
  }
  // Compose stills already written this run into ONE multi-panel PNG, which is what a
  // journal wants for a figure with parts (A), (B), ... Doing it here rather than in a
  // project script keeps the composite a product of the same tour: rename a shot and the
  // montage follows, instead of silently dropping a panel at submission time.
  //
  //   montage: {name: figure1, shots: [source_hg38, lifted_hs1]}
  //   montage: {name: fig2, shots: [a, b], direction: horizontal, labels: [Before, After]}
  //
  // Composed in a browser page at deviceScaleFactor 1 with every panel at its NATURAL
  // pixel size, so the composite is pixel-for-pixel the panels -- a `make hires` montage
  // is print resolution because its inputs were, not because anything was upscaled.
  // Panels narrower than the widest are left-aligned and padded, never stretched.
  async function montage(arg) {
    const o = (arg && typeof arg === 'object') ? arg : { name: arg };
    const name = o.name ?? o.shot;
    const shots = o.shots || o.panels || [];
    if (!name || !shots.length) { console.warn(`montage: needs {name:, shots: [...]}`); return; }
    const dir = (o.direction || 'vertical').startsWith('h') ? 'row' : 'column';
    const gap = o.gap != null ? Number(o.gap) : 14;
    const auto = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
    const panels = [];
    for (let i = 0; i < shots.length; i++) {
      const src = path.join(STILLDIR, shots[i] + '.png');
      if (!fs.existsSync(src)) { console.warn(`montage ${name}: no still "${shots[i]}.png" -- panel skipped`); continue; }
      panels.push({ data: 'data:image/png;base64,' + fs.readFileSync(src).toString('base64'),
                    label: (o.labels && o.labels[i] != null) ? String(o.labels[i])
                         : (o.labels === false ? '' : auto[i] || String(i + 1)) });
    }
    if (!panels.length) { console.warn(`montage ${name}: no panels to compose`); return; }
    const ctx3 = await browser.newContext({ viewport: { width: 1200, height: 900 }, deviceScaleFactor: 1 });
    const pg3 = await ctx3.newPage();
    await pg3.setContent('<!doctype html><body style="margin:0;background:#fff"><div id="__fig"></div></body>');
    const labelSize = await pg3.evaluate(({ panels, dir, gap, labelSize }) => {
      const fig = document.getElementById('__fig');
      fig.style.cssText = `display:inline-flex;flex-direction:${dir};align-items:flex-start;`
        + `gap:${gap}px;background:#fff;padding:${gap}px;font-family:Helvetica,Arial,sans-serif;`;
      const rows = panels.map(p => {
        const row = document.createElement('div');
        row.style.cssText = 'display:flex;align-items:flex-start;';
        const lab = document.createElement('div');
        lab.className = '__figLabel';
        lab.textContent = p.label;
        const img = document.createElement('img');
        img.src = p.data; img.style.display = 'block';
        row.appendChild(lab); row.appendChild(img);
        fig.appendChild(row);
        return { lab, img };
      });
      return Promise.all(rows.map(r => r.img.decode().catch(() => {}))).then(() => {
        // Label size follows the panels' own resolution, so a 3x montage gets 3x lettering
        // rather than a caption that shrinks to nothing next to a 2500px panel.
        const maxW = Math.max(...rows.map(r => r.img.naturalWidth));
        const fs = labelSize != null ? labelSize : Math.max(11, Math.round(maxW / 55));
        for (const r of rows) {
          r.lab.style.cssText = `flex:0 0 ${Math.round(fs * 1.5)}px;font-weight:bold;`
            + `font-size:${fs}px;line-height:1;color:#111;`;
          r.img.style.width = r.img.naturalWidth + 'px';   // natural size, never stretched
        }
        return fs;
      });
    }, { panels, dir, gap, labelSize: o.labelSize != null ? Number(o.labelSize) : null });
    const p = path.join(STILLDIR, name + '.png');
    await pg3.locator('#__fig').screenshot({ path: p });
    await ctx3.close();
    console.log('SHOT', p, `(montage: ${panels.length} panels, label ${labelSize}px)`);
  }
  // `shot:` writes a picture of the view; `session:` writes the view ITSELF, as a settings
  // file anyone can load into their own browser. hgSession's save-to-a-local-file path
  // (doSaveLocal, hg/hgSession/hgSession.c) needs no wiki login, so a plain GET returns the
  // whole cart -- every track's visibility, the attached hubs, the custom tracks, the window.
  //
  // A FILE rather than the live hgsid URL, because the hgsid cart goes on changing as the
  // tour runs: a link handed out at step 5 would open on whatever step 20 left behind. The
  // file is a snapshot of this step, which is what `shot:` already means.
  //
  // Fetched through ctx.request, which shares the context's cookies -- so it reaches the same
  // cart -- but never touches the page, so nothing about it lands in the recorded video.
  // hgSession drops its own hgS_* variables before it checks the cart out
  // (cleanHgSessionFromCart), so the tour carries on from an unchanged state.
  async function session(arg) {
    const o = (arg && typeof arg === 'object') ? arg : { name: arg };
    const name = String(o.name || base);
    // Every nav so far may have gone by cookie alone, in which case state.hgsid is still
    // empty; the page itself always carries one, in a form field or in its own links.
    if (!state.hgsid) {
      const h = await page.evaluate(() => {
        const i = document.querySelector('input[name="hgsid"]');
        if (i && i.value) return i.value;
        for (const a of document.querySelectorAll('a[href*="hgsid="]')) {
          const m = /[?&]hgsid=([\w.]+)/.exec(a.getAttribute('href') || '');
          if (m) return m[1];
        }
        return null;
      }).catch(() => null);
      if (h) state.hgsid = h;
    }
    const res = await ctx.request.get(`${SERVER}/hgSession?`
      + (state.hgsid ? `hgsid=${enc(state.hgsid)}&` : '')
      + `hgS_doSaveLocal=submit&hgS_saveLocalFileName=${enc(name + '.txt')}`
      + `&hgS_saveLocalFileCompress=none`);
    const body = await res.text();
    // An hgSession error is still a 200 with an HTML page in it. Written out unchecked, that
    // "settings file" would only be found to be an error page by whoever tried to load it.
    const settings = body.split('\n').filter(l => /^\S+ /.test(l)).length;
    if (!res.ok() || /^\s*</.test(body) || settings < 5) {
      console.warn(`session ${name}: hgSession answered ${res.status()} with no settings `
        + `("${body.slice(0, 120).replace(/\s+/g, ' ')}") -- nothing written`);
      return;
    }
    fs.mkdirSync(SESSDIR, { recursive: true });
    const file = path.join(SESSDIR, name + '.txt');
    fs.writeFileSync(file, body);
    console.log('SESSION', file, `(${settings} settings)`);
    // hgTracks reads the file over http, so a URL can only be printed once the script says
    // where that directory is published.
    if (doc.sessionUrlBase)
      console.log(`  ${SERVER}/hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=`
        + enc(`${String(doc.sessionUrlBase).replace(/\/+$/, '')}/${base}/${name}.txt`));
    else
      console.log('  (no sessionUrlBase: at the top of the script, so no load URL -- '
        + 'hgTracks reads the file over http, so the sessions directory has to be published)');
  }
  // The inverse of `session:`: start the tour FROM a saved state instead of from a clean
  // cart. A script can then begin where another ended, and a bug report that arrives as a
  // session link becomes a starting position rather than something to rebuild by hand.
  //
  //   loadSession: https://example.org/settings.txt          a settings file, by URL
  //   loadSession: https://genome.ucsc.edu/s/braney/myView   a share link
  //   loadSession: {user: braney, name: myView}              a named session
  //   loadSession: {file: source}                            sessions/<base>/source.txt
  //
  // Quick and silent, like `hub:` -- this is setup, not something the tour demonstrates.
  //
  // Whatever form it takes, the load is issued against `target:`. A share link naming
  // another host is turned into a named-session load HERE rather than followed there,
  // because every later step navigates to `target:` by absolute URL: following the link
  // would leave the tour on the other server's cart and the next step would silently
  // abandon it. Named sessions are per-server (hgcentral vs hgcentraltest), so a link
  // copied off the RR only works if that session also exists on the machine being driven.
  async function loadSession(arg) {
    const o = (arg && typeof arg === 'object') ? arg : { from: arg };
    const from = o.from ?? o.url ?? o.file ?? o.name;
    if (from == null) throw new Error('loadSession: nothing to load');
    if (o.user) {
      await nav(`/cgi-bin/hgTracks?hgS_doOtherUser=submit&hgS_otherUserName=${enc(o.user)}`
        + `&hgS_otherUserSessionName=${enc(o.name ?? from)}`);
    } else if (o.file || !/^https?:/i.test(String(from))) {
      // A file on disk goes up through hgSession's own upload form: hgTracks can only read a
      // session over http, and a project's sessions/ directory need not be published at all.
      const n = String(o.file ?? from);
      const file = path.isAbsolute(n) ? n : path.join(SESSDIR, /\.\w+$/.test(n) ? n : n + '.txt');
      if (!fs.existsSync(file)) throw new Error(`loadSession: no such file: ${file}`);
      await nav('/cgi-bin/hgSession');
      await page.setInputFiles('input[name="hgS_loadLocalFileName"]', file);
      await page.click('input[name="hgS_doLoadLocal"]');
      await page.waitForLoadState('load').catch(() => {});
      await nav('/cgi-bin/hgTracks');
    } else {
      const u = new URL(String(from));
      const share = /^\/s\/([^/]+)\/(.+)$/.exec(u.pathname);
      const hgs = [...u.searchParams.keys()].some(k => k.startsWith('hgS_'));
      // Only a link that names a SESSION cares which host it came from: the session lives in
      // that machine's hgcentral. A settings file is just a file, and being on another host
      // is the normal case -- hgTracks fetches it over http.
      if ((share || hgs) && u.host !== new URL(SERVER).host)
        console.warn(`loadSession: the link names ${u.host} but the tour drives `
          + `${new URL(SERVER).host}, so it is loaded there instead `
          + `-- a named session has to exist on the machine being driven`);
      if (share)
        await nav(`/cgi-bin/hgTracks?hgS_doOtherUser=submit`
          + `&hgS_otherUserName=${enc(decodeURIComponent(share[1]))}`
          + `&hgS_otherUserSessionName=${enc(decodeURIComponent(share[2]))}`);
      else if (hgs)
        await nav(`/cgi-bin/hgTracks?${u.search.slice(1)}`);
      else
        await nav(`/cgi-bin/hgTracks?hgS_doLoadUrl=submit&hgS_loadUrlName=${enc(String(from))}`);
    }
    await page.waitForSelector('#imgTbl').catch(() => {});
    await captureState();
    if (o.shot) await shot(o.shot);
  }
  // The only verb that can fail a run. Every other verb renders happily whatever it is
  // handed: a superTrack that came up whole and made an image 7,581 px tall, a subtrack
  // that never hid, a pinned tooltip that grabbed the neighbouring item, an Apache 414
  // page where the view should be. All of those shipped once and all were caught by eye.
  // Stating the expectation instead stops the run, non-zero, at the step that broke it --
  // `make` then fails rather than writing a wrong figure over a right one.
  //
  //   expect: {rows: [ruler, mane]}         these rows were drawn
  //   expect: {rows: [ruler, mane], exact: true}   ... and nothing else
  //   expect: {noRows: [clinvarCnv]}        this row was not
  //   expect: {height: 2000}                the still is no taller than this ("<1200" etc.)
  //   expect: {tip: "mismatch A->C"}        the tooltip now up says this
  //   expect: {text: "...", noText: "..."}  the page does / does not contain this
  //
  // `warn: true` downgrades a failure to a warning, for a check worth logging but not worth
  // stopping a build over.
  async function expectState(arg) {
    const o = (arg && typeof arg === 'object') ? arg : { text: arg };
    const seen = await page.evaluate(() => {
      const im = document.getElementById('imgTbl');
      const tip = document.getElementById('mouseoverContainer');
      const up = tip && tip.offsetWidth > 0 && getComputedStyle(tip).display !== 'none'
        && getComputedStyle(tip).visibility !== 'hidden';
      return {
        rows: [...document.querySelectorAll('[id^="img_data_"]')].map(e => e.id.replace('img_data_', '')),
        cssHeight: im ? im.getBoundingClientRect().height : 0,
        tip: up ? tip.innerText.trim() : '',
        text: document.body ? document.body.innerText : '',
      };
    });
    // The still is a screenshot of #imgTbl, so its height in PIXELS is the CSS height times
    // the device pixel ratio -- which is what someone means by "7,581 px tall", and what a
    // print run makes k times bigger.
    const height = Math.round(seen.cssHeight * SCALE);
    // A hub track's row id carries a per-run hub_<n>_ prefix (a quickLift target's rows all
    // do), so match the plain name by suffix, the way `mouseover:` resolves a track.
    const drawn = w => seen.rows.some(r => r === w || r.endsWith('_' + w));
    const list = v => v == null ? [] : (Array.isArray(v) ? v : [v]).map(String);
    const bad = [];
    const want = list(o.rows);
    const missing = want.filter(w => !drawn(w));
    if (missing.length) bad.push(`rows not drawn: ${missing.join(', ')}`);
    if (o.exact && want.length) {
      const extra = seen.rows.filter(r => !want.some(w => r === w || r.endsWith('_' + w)));
      if (extra.length) bad.push(`unexpected rows: ${extra.join(', ')}`);
    }
    const banned = list(o.noRows).filter(w => drawn(w));
    if (banned.length) bad.push(`rows that should not be drawn: ${banned.join(', ')}`);
    if (o.height != null) {
      // A bare number is a ceiling, which is the check anyone actually wants.
      const m = /^\s*(<=|>=|<|>|=)?\s*(\d+)\s*$/.exec(String(o.height));
      if (!m) bad.push(`height: cannot read "${o.height}"`);
      else {
        const n = Number(m[2]), op = m[1] || '<=';
        const ok = op === '<' ? height < n : op === '>' ? height > n
          : op === '>=' ? height >= n : op === '=' ? height === n : height <= n;
        if (!ok) bad.push(`image is ${height}px, wanted ${op}${n}`);
      }
    }
    if (o.tip != null && !seen.tip.includes(String(o.tip)))
      bad.push(seen.tip ? `tooltip says "${seen.tip}", wanted "${o.tip}"`
                        : `no tooltip is up, wanted "${o.tip}"`);
    if (o.text != null && !seen.text.includes(String(o.text)))
      bad.push(`page does not contain "${o.text}"`);
    if (o.noText != null && seen.text.includes(String(o.noText)))
      bad.push(`page contains "${o.noText}"`);
    if (!bad.length) {
      console.log(`EXPECT ok -- ${seen.rows.length} row(s), ${height}px`);
      return;
    }
    const msg = bad.join('; ') + `\n  drawn: ${seen.rows.join(', ') || '(none)'}`;
    if (o.warn) console.warn('EXPECT (warning only):', msg);
    else throw new Error(msg);
  }
  // Shift+drag across the track image to open the browser's own drag-select dialog
  // ("Zoom In / Single Highlight / ..."), then act on it. The usual form gives one
  // genomic region and zooms:  drag: chr7:155,806,100-155,806,557
  // Any other action needs the map form, which is also how you pass shot:/track:
  //   drag: {range: "chr7:155,806,100-155,806,557", then: highlight}
  // Endpoints that are not genomic coords use a fraction
  // across the view (fromFrac:/toFrac:) or a raw pixel (fromX:/toX:) instead.
  // Optional `track:` picks the row the drag runs over (y); default is the middle of
  // the image. `shot:` captures the open dialog (e.g. the Figure 1A drag-select box).
  // `then:` = zoom (default, clicks Zoom In) | highlight (Single Highlight) | cancel
  // (Escape, leaves the view unchanged).
  async function drag(o) {
    // A bare string is the region; `range:` is the same thing with room for other
    // keys. Both expand to the from:/to: endpoints the rest of this function uses.
    if (typeof o === 'string') o = { range: o };
    o = o || {};
    if (o.range != null) {
      const m = String(o.range).match(/^\s*(.+):([\d,]+)\s*-\s*([\d,]+)\s*$/);
      if (!m) throw new Error(`drag: range "${o.range}" is not chrom:start-end`);
      o = Object.assign({}, o, { from: `${m[1]}:${m[2]}`, to: `${m[1]}:${m[3]}` });
    }
    const img = await page.locator('img[id^="img_data_"]').first().boundingBox({ timeout: 8000 }).catch(() => null);
    const tbl = await page.locator('#imgTbl').first().boundingBox({ timeout: 8000 }).catch(() => null);
    if (!img || !tbl) throw new Error('drag: track image not shown (need #imgTbl)');
    const coordFrac = at => page.evaluate(a => {
      try { const s = hgTracks.winStart, e = hgTracks.winEnd;
        const c = +String(a).replace(/.*:/, '').replace(/,/g, '');
        return Math.max(0, Math.min(1, (c - s) / (e - s))); } catch (_) { return null; }
    }, at);
    // The grey side-label strip is baked into the LEFT of every full-width track
    // image, so the genomic data area starts insideX px in — fractions/coords map
    // across [img.x+insideX, img.x+img.width], not the whole image width.
    // insideX and any px: endpoint are in the drawn image's pixels; imgPx converts them to
    // page pixels (1 normally, 1/SCALE when the image is scaled for print).
    const imgPx = await page.evaluate(() => {
      const im = document.querySelector('img[id^="img_data_"]');
      return im && im.naturalWidth ? im.getBoundingClientRect().width / im.naturalWidth : 1;
    }) || 1;
    const insideX = imgPx * await page.evaluate(() => { try { return hgTracks.insideX || 0; } catch (_) { return 0; } });
    const dataLeft = img.x + insideX, dataW = Math.max(1, img.width - insideX);
    const endX = async (px, fr, coord) => {
      if (px != null) return img.x + imgPx * Number(px);
      const f = (fr != null) ? Number(fr) : (coord != null ? await coordFrac(coord) : null);
      if (f == null) throw new Error('drag: need endpoints as coord (from:/to:), frac (fromFrac:/toFrac:) or px (fromX:/toX:)');
      return dataLeft + f * dataW;
    };
    const x1 = await endX(o.fromX, o.fromFrac, o.from);
    const x2 = await endX(o.toX, o.toFrac, o.to);
    // y band for the drawn selection box. Default: span the FULL track image (like a
    // real shift+drag, which highlights every track top-to-bottom). A named `track:`
    // narrows the band to that one row instead. The CURSOR, however, sweeps near the
    // TOP of the image (over the ruler) — a real drag is horizontal and the highlight
    // fills downward on its own; sending the cursor to the vertical center would make
    // it dive through the tracks first.
    let y = tbl.y + Math.min(90, tbl.height / 2);
    let y1 = 0, y2 = tbl.height;
    if (o.track) {
      const row = await trackBox(o.track).then(b => b.row).catch(() => null);
      if (row) { y = row.y + row.height / 2; y1 = row.y - tbl.y; y2 = row.y - tbl.y + row.height; }
    }
    // The selected genomic range: from coord endpoints we already have it; otherwise
    // derive it from the view fractions below (inside the browser).
    const coordNum = v => +String(v).replace(/.*:/, '').replace(/,/g, '');
    let posStr = null;
    if (typeof o.from === 'string' && o.from.includes(':') && o.to != null) {
      const chrom = o.from.split(':')[0], a = coordNum(o.from), b = coordNum(o.to);
      posStr = `${chrom}:${Math.min(a, b)}-${Math.max(a, b)}`;
    }
    // Visible cursor sweep across the selection (no button-down — a real drag would
    // just PAN the image). The drag-select band is grown UNDER the cursor as it moves,
    // so it reads like a genuine shift+drag rather than popping in at the end; the
    // dialog is then raised via the browser's own dragSelect entry point, the same way
    // highlight_shot.js does it.
    await glide(x1, y); await sleep(200);
    // Arm the highlighting dialog and open a zero-width selection at the start point.
    await page.evaluate(({ ix1, y1, y2 }) => {
      try {
        hgTracks.enableHighlightingDialog = true;
        dragSelect.startTime = Date.now();
        $(imageV2.imgTbl).imgAreaSelect({ x1: ix1, y1, x2: ix1, y2, show: true });
      } catch (_) {}
    }, { ix1: x1 - tbl.x, y1, y2 });
    // Sweep to the end, widening the band to the cursor's x at every step.
    const dsteps = Math.max(10, Math.round(Math.abs(x2 - x1) / 9));
    for (let i = 1; i <= dsteps; i++) {
      const px = x1 + (x2 - x1) * i / dsteps;
      await page.mouse.move(px, y);
      await page.evaluate(({ ix1, ix2, y1, y2 }) => {
        try { $(imageV2.imgTbl).imgAreaSelect({ x1: Math.min(ix1, ix2), y1, x2: Math.max(ix1, ix2), y2, show: true }); } catch (_) {}
      }, { ix1: x1 - tbl.x, ix2: px - tbl.x, y1, y2 });
      await sleep(15);
    }
    cur.x = x2; cur.y = y;
    await sleep(200);
    // Band is fully drawn — now raise the Drag-and-select dialog.
    const res = await page.evaluate(({ f1, f2, posStr }) => {
      try {
        let pos = posStr;
        if (!pos) { const s = hgTracks.winStart, e = hgTracks.winEnd;
          const c1 = Math.round(s + (e - s) * f1), c2 = Math.round(s + (e - s) * f2);
          pos = hgTracks.chromName + ':' + (Math.min(c1, c2) + 1) + '-' + Math.max(c1, c2); }
        dragSelect.selectionEndDialog(pos);
        return true;
      } catch (e) { return String((e && e.message) || e); }
    }, { f1: (x1 - dataLeft) / dataW, f2: (x2 - dataLeft) / dataW, posStr });
    const up = await page.waitForSelector('#dragSelectDialog:visible', { timeout: 4000 }).then(() => true).catch(() => false);
    if (!up) throw new Error('drag: drag-select dialog did not open' + (res === true ? '' : ' (' + res + ')'));
    await sleep(400);
    if (o.shot) await shot(o.shot);
    const act = o.then || 'zoom';
    if (act === 'cancel') { await page.keyboard.press('Escape'); }
    else {
      const label = (act === 'highlight') ? 'Single Highlight' : 'Zoom In';
      const sel = `.ui-dialog-buttonset button:has-text("${label}")`;
      // "Zoom In" either full-submits the form OR does an AJAX in-place update
      // (imageV2.inPlaceUpdate) that never fires a page load -- so don't wait on load;
      // wait for the live hgTracks window to actually change from its pre-click value.
      const before = await page.evaluate(() => { try { return hgTracks.winStart + '-' + hgTracks.winEnd; } catch (e) { return ''; } });
      await clickGlide(sel);
      if (act !== 'highlight') {
        await page.waitForFunction(prev => {
          try { return (hgTracks.winStart + '-' + hgTracks.winEnd) !== prev; } catch (e) { return false; }
        }, before, { timeout: 8000 }).catch(() => {});
        await page.waitForSelector('#imgTbl'); await sleep(400);
      }
      await captureState();
    }
  }
  async function resolveTarget(to) {
    if (/^GC[AF]_/i.test(to)) return to;
    const matches = await page.$$eval('#hglft_toDbSelect option', (os, q) => {
      const norm = t => t.toLowerCase().replace(/[^a-z0-9]+/g, ' ');
      const toks = norm(q).trim().split(' ').filter(Boolean);
      return os.filter(o => toks.every(tk => norm(o.text).includes(tk))).map(o => ({ v: o.value, t: o.text }));
    }, String(to));
    if (!matches.length) throw new Error('convert target not found in Assembly dropdown: ' + to);
    if (matches.length > 1) {
      console.warn(`WARNING: "${to}" matches ${matches.length} assemblies; using the first. Use an exact accession to disambiguate:`);
      matches.forEach(m => console.warn(`    ${m.v}  ${m.t}`));
    }
    return matches[0].v;
  }
  async function convert(o) {
    o = o || {};
    // `shot:` here can name up to three moments of the Convert page, none of which any
    // other verb can reach (after Submit the tour is already on the results page):
    //   shot: convert_filled                 -- just before Submit (the common one)
    //   shot: {opened: a, filled: b, result: c}
    //     opened  the page as it comes up, nothing chosen yet
    //     filled  target searched for, QuickLift/Hide-defaults set -- ready to Submit
    //     result  the conversion result page (the coordinate link `open: lift` clicks)
    const shots = (o.shot == null) ? {}
                : (typeof o.shot === 'string' ? { filled: o.shot } : o.shot);
    for (const k of Object.keys(shots))
      if (!['opened', 'filled', 'result'].includes(k))
        console.warn(`convert shot: unknown moment "${k}" (use opened, filled or result)`);
    try {
      await glideTo('#view'); await page.hover('#view'); await dwell(900);
      await clickGlide('a#convertMenuLink'); await page.waitForSelector('#hglft_toDbSelect', { timeout: 8000 });
    } catch (e) {
      await nav(`/cgi-bin/hgConvert?hgsid=${state.hgsid}&db=${state.db}&position=${enc(state.position)}`);
      await page.waitForSelector('#hglft_toDbSelect');
    }
    if (shots.opened) await shot(shots.opened);
    // Find the target the way a user does: TYPE it into the Convert page's own "Search for
    // target genome" bar and click the suggestion (the species autocomplete takes an
    // accession or a name, and is already filtered to assemblies hg38 can lift to). So the
    // string the script names is visibly searched for on screen, not silently selected.
    // `search:` overrides what gets typed; `pick:` disambiguates the menu.
    const term = o.search != null ? o.search : o.to;
    let searched = false;
    if (term != null && await page.locator('#toGenomeSearch:visible').count()) {
      await glideTo('#toGenomeSearch'); await page.click('#toGenomeSearch'); await dwell(160);
      await page.fill('#toGenomeSearch', '');
      const label0 = await page.locator('#toGenomeLabel').textContent().catch(() => '');
      await typeIn(page, '#toGenomeSearch', term);
      await dwell(300);
      const hit = await pickSuggest(String(term), o.pick);
      if (hit) {
        console.log(`convert: searched "${term}" -> "${hit.text}"`
          + (hit.timedOut ? ` (no strong match, waited ${hit.ms}ms)` : ''));
        const li = page.locator(SUGGEST_ROW).nth(hit.index);
        const b = await li.boundingBox().catch(() => null);
        if (b) await glide(b.x + b.width / 2, b.y + b.height / 2);
        await dwell(220);
        await li.click();
        // Picking a genome repopulates the Assembly dropdown (ajax) and rewrites the hidden
        // toDb field. Wait for the "Selected:" label to actually change instead of guessing
        // a duration -- right on a fast server, still correct on a slow one.
        await page.waitForFunction(b0 => {
          const el = document.getElementById('toGenomeLabel');
          return el && (el.textContent || '') !== b0;
        }, label0, { timeout: 5000 }).catch(() => {});
        await dwell(300);
        searched = true;
      } else {
        console.warn(`convert: "${term}" matched nothing in the target-genome search`);
      }
    }
    // The Assembly dropdown is what actually gets submitted, so confirm it landed on the
    // requested assembly; open it visibly only if the search didn't get us there.
    const val = await resolveTarget(o.to);
    const landed = await page.locator('#hglft_toDbSelect').inputValue().catch(() => null);
    if (!searched || landed !== val) {
      if (searched) console.log(`convert: Assembly dropdown is on "${landed}", picking ${val}`);
      await openSelectVisible('#hglft_toDbSelect', val);
    }
    await page.waitForSelector('#doQuickLift', { timeout: 8000 }).catch(() => {});
    await dwell(400);
    if (o.quicklift !== false) await checkGlide('#doQuickLift', true);
    await checkGlide('#hideTracksOnConvert', o.hideDefaults !== false);  // reverts on assembly change -> set explicitly
    await dwell(300);
    if (shots.filled) await shot(shots.filled);
    await clickGlide('#hglft_doConvert');
    await page.waitForLoadState('load'); await captureState();
    if (shots.result) await shot(shots.result);
  }

  // On the "Hub Connect Successful" page, click the "Open:" link for `db` so the demo
  // ends on the browser with the hub loaded. The links look like
  // hgTracks?hubUrl=...&db=<genome>&position=lastDbPos (or &genome=<genome>).
  async function openHubAssembly(db) {
    const sel = await page.evaluate((db) => {
      const esc = db.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
      const re = new RegExp('[?&](?:db|genome)=' + esc + '(?:&|$)');
      // The connect page renders the genome list twice (a short + a collapsed long copy),
      // so require the link to be VISIBLE, not just the first match in DOM order.
      const a = [...document.querySelectorAll('a[href*="hgTracks"]')]
        .find(a => re.test(a.getAttribute('href') || '') && a.getClientRects().length > 0);
      if (!a) return null;
      if (!a.id) a.id = '__hubOpen';
      return '#' + a.id;
    }, String(db));
    if (!sel) return false;
    await clickGlide(sel);
    await page.waitForSelector('#imgTbl').catch(() => {});
    await captureState();
    return true;
  }

  function norm(step) {
    if (typeof step === 'string') { const [v, ...r] = step.trim().split(/\s+/); return { verb: v, arg: r.length ? r.join(' ') : true }; }
    const k = Object.keys(step)[0]; return { verb: k, arg: step[k] };
  }
  async function run({ verb, arg }) {
    switch (verb) {
      case 'gateway': await nav(`/cgi-bin/hgGateway?db=${state.db}`); break;
      case 'go':
        if (arg === true || arg === '' || arg == null) { await clickGlide('.jwGoButtonContainer'); await page.waitForSelector('#imgTbl'); }
        else { await nav(`/cgi-bin/hgTracks?db=${state.db}&position=${enc(arg)}&${IMGVARS}`); }
        await captureState(); break;
      case 'goShow': {
        // DEMONSTRATE the position change through the UI (vs. `go:` which navs straight to
        // the new position): glide to the position box, clear it, type on screen, then let
        // the page take it from there -- "Search" (#goButton) on hgTracks, the arrow
        // (.jwGoButtonContainer) on hgGateway, so one verb covers either page.
        //
        // Takes a POSITION or a GENE NAME (or any search term the box accepts: HGVS, an
        // accession, ...). A gene name goes through the browser's own suggestion menu, the
        // way a user does it: wait for the menu, then click the matching item -- hgTracks'
        // handler sets the position from that item and submits, so we land on the gene
        // instead of the search-results page. `pick:` chooses among the suggestions when
        // the term is ambiguous (substring of the menu row); default is an exact symbol
        // match, else the first row.
        //
        //   goShow: BRCA1
        //   goShow: {gene: SHH, shot: source}
        //   goShow: {position: "chr7:155,799,529-155,812,871", shot: source}
        //   goShow: {gene: BRCA1, pick: "NM_007294", shot: source}
        const o = (typeof arg === 'string') ? { position: arg } : (arg || {});
        const pos = [o.position, o.pos, o.gene, o.search].find(v => v != null && v !== '');
        if (pos == null) { console.warn('goShow: no position or gene given'); break; }
        const term = String(pos).trim();
        // A coordinate has no suggestions to wait on; anything else is a search term.
        const isPos = /^[\w.|-]+:[\d,]+(-[\d,]+)?$/.test(term);
        if (!await page.locator('#positionInput:visible').count())
          throw new Error('goShow: no position box on this page (need hgTracks or hgGateway) -- ' + page.url());
        await glideTo('#positionInput'); await page.click('#positionInput'); await dwell(160);
        await page.fill('#positionInput', '');                            // clear the old position
        await dwell(200);
        await typeIn(page, '#positionInput', term);                       // visible typing
        await dwell(450);
        pinnedTips.length = 0;                                            // new view, old tips don't apply
        let done = null;
        if (!isPos) {
          const hit = await pickSuggest(term, o.pick != null ? o.pick : o.match);
          if (hit) {
            console.log(`goShow ${term}: suggestion "${hit.text}"`
              + (hit.timedOut ? ` (no strong match, waited ${hit.ms}ms)` : ''));
            // Address the row by index at click time (a late re-render replaces the <li>
            // elements, so a handle or a marker attribute taken earlier goes stale).
            const li = page.locator(SUGGEST_ROW).nth(hit.index);
            // Arm the nav wait BEFORE the click (the page's own handler submits the form for
            // us), and wait on 'commit' so we can tell "navigated" from "only filled the box".
            const committed = page.waitForNavigation({ waitUntil: 'commit', timeout: 15000 }).catch(() => null);
            const b = await li.boundingBox().catch(() => null);
            if (b) await glide(b.x + b.width / 2, b.y + b.height / 2);
            await sleep(220);
            await li.click();
            done = await committed;
            if (done) await page.waitForLoadState('load').catch(() => {});
          } else {
            console.warn(`goShow ${term}: no suggestion matched, submitting the term as typed`);
          }
        }
        // Coordinate, no suggestions, or hgGateway (where picking a suggestion only fills
        // the box): click the page's own go button. Arm the nav wait BEFORE the click -- on
        // hgTracks the OLD page already has an #imgTbl, so waiting on the selector alone
        // returns instantly and a following shot races the reload ("Cannot find context
        // with specified id").
        if (!done) {
          const navDone = page.waitForNavigation({ waitUntil: 'load', timeout: 30000 }).catch(() => {});
          await clickGlide((await page.locator('#goButton').count()) ? '#goButton' : '.jwGoButtonContainer');
          await navDone;
        }
        // A unique hit lands on the track image; a term with no suggestion and several
        // matches lands on the search-results page instead, which has no #imgTbl -- that's
        // legal, the script can `click` a result from there.
        await page.waitForSelector('#imgTbl', { timeout: 15000 }).catch(() => {});
        await captureState();
        await page.mouse.move(cur.x, cur.y);                              // re-show the cursor overlay
        if (o.shot) { await shot(o.shot); return; }
        break;
      }
      case 'hide': if (arg === 'all' || arg === true) { await clickGlide('#hgt\\.hideAll'); await page.waitForSelector('#imgTbl'); } break;
      case 'track': {
        let entries = Object.entries(arg);
        const isKidHide = ([, mode]) => String(mode).toLowerCase() === 'hidekids';
        // What the author spelled out AS A VISIBILITY. A `hideKids` container is deliberately
        // NOT in here: it names no mode of its own, so it must not suppress the container
        // variable derived from a child below it (`pubtator: pack` is what turns varsInPubs on).
        const named = new Set(entries.filter(e => !isKidHide(e)).map(([n]) => n));
        const idx = await tdbIndex(state.db);
        // `hideKids` is not a visibility -- it is "hide everything under this container", so
        // that a child named alongside it is left the only one drawn. A superTrack needs it:
        // unlike a composite, its own mode does NOT reach its children, so every child comes
        // up at its own trackDb visibility and an earlier `hide: all` does not stick
        // (`{varsInPubs: show}` alone draws all eight of its members). The expansion skips any
        // child the step names itself, and runs in a round of its own AFTER the rest, because
        // a subtrack hide travelling in the same request as its container can be dropped by
        // the cart (#37953) -- so the container goes on first and the hides follow.
        const kidHides = [];
        for (const e of entries.filter(isKidHide)) {
          const leaves = (await tdbHideTargets(e[0])).filter(k => !named.has(k));
          if (!leaves.length)
            console.warn(`track ${e[0]}: hideKids -- trackDb gives it no children to hide`);
          for (const k of leaves) kidHides.push([k, 'hide']);
        }
        entries = entries.filter(e => !isKidHide(e));
        // Visible gesture first: drive the real track-controls dropdowns so the mouse is
        // seen turning the tracks on. State is still applied by the nav()s below (which
        // carry the container/checkbox vars too), so these opens are non-committing.
        if (doc.trackAnim !== false)
          for (const [name, mode] of entries) {
            const csel = await ctrlSelect(name);
            if (csel) await openSelectVisible(csel, mode, 6, false);
          }
        // hgTracks RESHAPES a composite when its container visibility changes, and that wipes
        // per-subtrack overrides arriving in the same request (`clinvar=pack&clinvarCnv=hide`
        // leaves clinvarCnv_sel=1 and the CNV row still drawn). So a step that names both a
        // composite and something under it is applied in rounds -- container first, then the
        // deviations -- which is exactly what writing them as two steps does. superTracks
        // don't reshape, so they don't force a round.
        const rounds = new Map();
        for (const e of entries) {
          let d = 0;
          for (let k = e[0]; ;) {
            const n = idx && idx.get(k);
            if (!n || !n.parent) break;
            const p = idx.get(n.parent);
            if (named.has(n.parent) && !(p && p.superTrack)) d++;
            k = n.parent;
          }
          if (!rounds.has(d)) rounds.set(d, []);
          rounds.get(d).push(e);
        }
        if (kidHides.length) rounds.set(Number.MAX_SAFE_INTEGER, kidHides);
        for (const d of [...rounds.keys()].sort((a, b) => a - b)) {
          const vars = new Map();
          for (const [name, mode] of rounds.get(d))
            // A derived variable never overrides one the step names itself, whatever the
            // order: `{clinvar: pack, clinvarCnv: hide}` keeps clinvar=pack.
            for (const [k, v] of await visVars(name, mode))
              if (k === name || !named.has(k)) vars.set(k, v);
          const parts = [...vars].map(([k, v]) => `${k}=${v}`);
          console.log('track:', parts.join(' '));   // what trackDb turned the step into
          await nav(`/cgi-bin/hgTracks?db=${state.db}&position=${enc(state.position)}&${parts.join('&')}&${IMGVARS}`);
        }
        break;
      }
      case 'convert': await convert(arg); break;
      case 'hub': {
        // Attach a track hub by URL: hgTracks?hubUrl=... connects the hub and makes its
        // tracks available at their hub-declared visibility. Follow with `track:` to turn
        // specific ones on. Accepts a bare URL or {url:, db:, position:}.
        const o = (typeof arg === 'string') ? { url: arg } : (arg || {});
        if (!o.url) { console.warn('hub: no url given'); break; }
        const db = o.db || state.db;
        const pos = o.position != null ? o.position : state.position;
        const parts = [`db=${db}`, `hubUrl=${enc(o.url)}`];
        if (pos) parts.push(`position=${enc(pos)}`);
        parts.push(IMGVARS);
        await nav(`/cgi-bin/hgTracks?${parts.join('&')}`);
        await page.waitForSelector('#imgTbl').catch(() => {});
        break;
      }
      case 'addHub': {
        // DEMONSTRATE attaching a hub through the UI (vs. `hub:` which just navs):
        // My Data -> Track Hubs (hgHubConnect), the Connected Hubs tab, paste the URL,
        // click Add Hub. The cursor glides and the URL is typed visibly. Accepts a bare
        // URL or {url:, db:, shot:}.
        const o = (typeof arg === 'string') ? { url: arg } : (arg || {});
        if (!o.url) { console.warn('addHub: no url given'); break; }
        const db = o.db || state.db;
        await nav(`/cgi-bin/hgHubConnect?db=${db}`);
        await clickGlide('a[href="#unlistedHubs"]');       // Connected Hubs tab reveals the URL box
        await page.waitForSelector('#hubUrl', { state: 'visible', timeout: 8000 });
        await glideTo('#hubUrl'); await page.click('#hubUrl'); await sleep(160);
        await page.type('#hubUrl', String(o.url), { delay: 35 });  // visible typing
        await sleep(300);
        await clickGlide('#hubAddButton');
        await page.waitForLoadState('load'); await captureState();
        // Click through to the requested assembly so we end on the browser.
        if (!await openHubAssembly(db))
          console.warn(`addHub: no assembly link for db "${db}" on the connect page`);
        if (o.shot) { await shot(o.shot); return; }
        break;
      }
      case 'addPublicHub': {
        // DEMONSTRATE connecting a PUBLIC hub via the UI: My Data -> Track Hubs, the
        // Public Hubs tab, type search terms, click Search Public Hubs, then click Connect
        // on the matching hub row. Bare string = search term (also used to match the row);
        // map form {search:, match:, db:, shot:} lets you match a specific hub by label
        // (the search often returns several hubs, so set `match:` to the hub's name).
        const o = (typeof arg === 'string') ? { search: arg } : (arg || {});
        const term = o.search != null ? o.search : null;
        const match = o.match != null ? o.match : term;
        if (!match) { console.warn('addPublicHub: no search/match given'); break; }
        const db = o.db || state.db;
        await nav(`/cgi-bin/hgHubConnect?db=${db}`);
        await clickGlide('a[href="#publicHubs"]');          // Public Hubs tab (default, but show it)
        await page.waitForSelector('#hubSearchTerms', { state: 'visible', timeout: 8000 });
        if (term) {
          await glideTo('#hubSearchTerms'); await page.click('#hubSearchTerms'); await sleep(120);
          await page.type('#hubSearchTerms', String(term), { delay: 35 });
          await sleep(250);
          await clickGlide('#hubSearchButton');
          await page.waitForLoadState('load');
        }
        // Click Connect on the row whose text contains `match` (don't guess a wrong hub).
        const btnId = await page.evaluate((m) => {
          m = String(m).toLowerCase();
          for (const btn of document.querySelectorAll('input[name="hubConnectButton"]')) {
            const tr = btn.closest('tr');
            if (tr && tr.innerText.toLowerCase().includes(m)) return btn.id;
          }
          return null;
        }, match);
        if (!btnId) { console.warn(`addPublicHub: no public-hub row matching "${match}"`); break; }
        await clickGlide('#' + btnId);
        await page.waitForLoadState('load'); await captureState();
        // Click through to the requested assembly so we end on the browser.
        if (!await openHubAssembly(db))
          console.warn(`addPublicHub: no assembly link for db "${db}" on the connect page`);
        if (o.shot) { await shot(o.shot); return; }
        break;
      }
      case 'addCustomTrack': {
        // DEMONSTRATE loading a custom track via the UI: My Data -> Custom Tracks
        // (hgCustom), paste the track data (or a data URL) into the box, click Submit,
        // then click through to the browser. Accepts a bare string (the track text or a
        // URL) or {data:/url:, db:, goto: first|current, shot:}. `goto:` picks the landing
        // button -- "Go to first annotation" (default) or "Return to current position".
        const o = (typeof arg === 'string') ? { data: arg } : (arg || {});
        const data = o.data != null ? o.data : o.url;
        if (!data) { console.warn('addCustomTrack: no data/url given'); break; }
        const db = o.db || state.db;
        await nav(`/cgi-bin/hgCustom?db=${db}`);
        const ta = 'textarea[name="hgct_customText"]';
        await page.waitForSelector(ta, { state: 'visible', timeout: 8000 });
        await glideTo(ta); await page.click(ta); await sleep(160);
        // insertText inserts literally (tabs/newlines and all) -- page.type would fire a
        // Tab key and move focus out of the textarea. Animate short pastes char-by-char.
        const s = String(data);
        if (s.length <= 400) { for (const ch of s) { await page.keyboard.insertText(ch); await sleep(12); } }
        else { await page.keyboard.insertText(s); }
        await sleep(300);
        await clickGlide('#Submit');
        await page.waitForLoadState('load');
        // The manage page appears on success; click through to the browser (a data error
        // re-shows the add page instead, so guard on the button being present).
        const goSel = (o.goto === 'current') ? '#submitGoBack' : '#submit';
        if (await page.locator(goSel).count()) {
          await clickGlide(goSel);
          await page.waitForSelector('#imgTbl').catch(() => {});
          await captureState();
        } else {
          console.warn('addCustomTrack: submit did not reach the manage page (data error?)');
        }
        if (o.shot) { await shot(o.shot); return; }
        break;
      }
      case 'drag': await drag(arg); break;
      case 'open': if (arg === 'lift') { await clickGlide('main a[href*="hgTracks"]'); await page.waitForSelector('#imgTbl'); await captureState(); } break;
      case 'zoom': {
        const btn = (arg === 'in') ? '#hgt\\.in2' : '#hgt\\.out2';
        const was = await page.evaluate(() => {
          try { return `${hgTracks.winStart}-${hgTracks.winEnd}`; } catch (_) { return ''; }
        });
        await clickGlide(btn);
        await page.waitForSelector('#imgTbl');
        // The zoom buttons redraw the image in place (ajax), so #imgTbl never went away and
        // waiting for it proves nothing: the next step can read the OLD view's map boxes and
        // report an item "not found" that simply is not in view yet. Wait for the window to
        // change instead. Until FAST there was always a dwell here hiding this.
        if (was) await page.waitForFunction(
            w => { try { return `${hgTracks.winStart}-${hgTracks.winEnd}` !== w; } catch (_) { return false; } },
            was, { timeout: 15000 })
          .catch(() => console.warn(`zoom ${arg}: window still ${was} after 15s`));
        await captureState();
        break;
      }
      case 'shot': await shot(arg); return;                       // shot supplies its own dwell
      case 'pinShot': await pinShot(arg); break;                  // combined figure, off the mp4 timeline
      case 'montage': await montage(arg); break;                  // stills -> one multi-panel PNG
      case 'session': await session(arg); break;                  // the cart itself, as a loadable file
      case 'loadSession': await loadSession(arg); break;          // ... and back in again
      case 'expect': await expectState(arg); break;               // the one verb that can fail a run
      case 'mouseover': await mouseover(arg); return;             // supplies its own dwell (o.hold)
      // escape hatches
      case 'goto': await nav(arg); break;
      case 'click':
        if (arg && typeof arg === 'object' && arg.track) {
          // Click a NAMED track item -> follow its map-box link (e.g. the hgc detail page).
          const it = await itemXY(arg.track, arg.item ?? arg.title ?? arg.value,
                                  arg.title != null && arg.item == null && arg.value == null);
          await glide(it.x, it.y); await sleep(200);
          // A raw click on the data area is swallowed by hgTracks' drag-select handler, so
          // follow the item's own map-box link (the hgc detail page) directly.
          if (it.href) await nav(it.href);
          else { await page.mouse.click(it.x, it.y); await page.waitForLoadState('load').catch(() => {}); await captureState(); }
          if (arg.shot) { await shot(arg.shot); return; }
        } else {
          // Plain selector click. Strip target=_blank first so an external link (e.g. a
          // dbSNP id -> NIH) navigates in THIS tab instead of a popup we can't screenshot,
          // then wait out the navigation so a following shot captures the destination page
          // (a no-op if the click didn't navigate).
          await page.evaluate(s => document.querySelectorAll(s).forEach(e => e.removeAttribute('target')), arg).catch(() => {});
          await clickGlide(arg);
          await page.waitForLoadState('load').catch(() => {});
          await captureState();
        }
        break;
      case 'hover': await glideTo(arg); await page.hover(arg); break;
      case 'wait': await page.waitForSelector(arg, { timeout: 15000 }); break;
      case 'sleep': await sleep(Number(arg)); return;
      default: console.warn('unknown verb:', verb);
    }
    await sleep(PACE);
  }

  if (SCALE > 1)
    console.log(`scale: ${SCALE}x -- pix=${PIX}, textSize=${TEXTSIZE}, dpr=${SCALE} at ${VW}x${VH}, `
                + `stills only (no mp4) -> ${STILLDIR}`);
  if (doc.reset) await page.goto(absurl('/cgi-bin/cartReset?skipLs=1'), { waitUntil: 'domcontentloaded' });
  const steps = doc.steps || [];
  const timing = [];
  for (let i = 0; i < steps.length; i++) {
    const s = norm(steps[i]);
    const t0 = Date.now();
    try { await run(s); }
    catch (e) { console.error(`step ${i + 1} (${s.verb}) failed:`, e.message); await ctx.close(); await browser.close(); process.exit(1); }
    timing.push({ n: i + 1, verb: s.verb, ms: Date.now() - t0 });
  }

  await page.waitForTimeout(300);
  await ctx.close();
  await browser.close();

  // Where did the wall clock go? DOCENT_TIME=1 prints the per-step table -- the dwells
  // (pace/shotHold) and the page loads dominate, which is what FAST=1 trims.
  if (process.env.DOCENT_TIME) {
    const tot = timing.reduce((a, t) => a + t.ms, 0);
    console.log(`--- steps: ${(tot / 1000).toFixed(1)}s total`);
    for (const t of [...timing].sort((a, b) => b.ms - a.ms))
      console.log(`  ${(t.ms / 1000).toFixed(1)}s  step ${t.n} ${t.verb}`);
  }
  if (FAST) {
    console.log('DONE (fast: stills only, no mp4) -> stills in', STILLDIR,
                `| ${((Date.now() - T_START) / 1000).toFixed(0)}s`);
    return;
  }
  const tVid = Date.now();
  // transcode webm -> silent mp4
  const vdir = path.join(HERE, '.vid_' + base);
  const webm = fs.readdirSync(vdir).filter(f => f.endsWith('.webm')).map(f => path.join(vdir, f)).sort((a, b) => fs.statSync(b).mtimeMs - fs.statSync(a).mtimeMs)[0];
  const FF = execFileSync('python3', ['-c', 'import imageio_ffmpeg,sys;sys.stdout.write(imageio_ffmpeg.get_ffmpeg_exe())']).toString().trim();
  execFileSync(FF, ['-y', '-loglevel', 'error', '-i', webm, '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '22', '-preset', 'veryfast', '-movflags', '+faststart', OUTMP4]);
  fs.rmSync(vdir, { recursive: true, force: true });
  if (process.env.DOCENT_TIME) console.log(`--- mp4 transcode: ${((Date.now() - tVid) / 1000).toFixed(1)}s`);
  console.log('DONE ->', OUTMP4, '| stills in', STILLDIR,
              `| ${((Date.now() - T_START) / 1000).toFixed(0)}s`);
})().catch(e => { console.error(e); process.exit(1); });
