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
const STILLDIR = doc.stills ? path.resolve(HERE, doc.stills) : path.join(HERE, 'stills', base);

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
const [VW, VH] = doc.size || [1000, 760];
const PIX = doc.pix || 850;
const PACE = Math.round((doc.pace ?? 1.2) * 1000);             // dwell after each step
const SHOTHOLD = Math.round((doc.shotHold ?? 2.2) * 1000);    // extra dwell (pause) at a shot

// track-name -> cart params (composite tracks expand to their learned "clean" config)
const TRACKS = {
  mane: m => [`mane=${m}`],
  dbSnp155: m => [`dbSnp155Composite=${m}`, `dbSnp155Common=${m}`, `dbSnp155ViewVariants=${m}`, `dbSnp155ViewErrs=hide`],
  clinvar: m => [`clinvar=${m}`, `clinvarMain=dense`, `clinvarSubLolly=${m}`, `clinvarCnv=hide`],
};
// The control-dropdown name and the data-image id can differ from the shortcut name:
// composites are turned on via their composite cart var, and dbSNP's visible pixels
// live in the "Common" subtrack's data image.
const CTRL = { dbSnp155: 'dbSnp155Composite' };          // select[name=...] in the track controls
const DATAIMG = { dbSnp155: 'dbSnp155Common', clinvar: 'clinvarMain' };  // #img_data_... that holds the drawn items
const ctrlName = t => CTRL[t] || t;
const imgTrack = t => DATAIMG[t] || t;

const sleep = ms => new Promise(r => setTimeout(r, ms));
const enc = s => encodeURIComponent(String(s));
const state = { db: doc.db || 'hg38', position: doc.position || '', hgsid: '' };

// ---------- animated cursor (same technique as the walkthrough-video skill's record.js) ----------
const CURSOR_INIT = () => {
  const add = () => {
    if (document.getElementById('__cur')) return;
    const c = document.createElement('div');
    c.id = '__cur';
    c.style.cssText = 'position:fixed;left:0;top:0;z-index:2147483647;pointer-events:none;width:24px;height:24px;margin-left:-3px;margin-top:-2px;filter:drop-shadow(0 1px 1px rgba(0,0,0,.4));';
    c.innerHTML = '<svg width="24" height="24" viewBox="0 0 24 24"><path d="M3 2 L3 19 L7.5 14.5 L10.5 21.5 L13.5 20.2 L10.6 13.5 L17 13.5 Z" fill="#111" stroke="#fff" stroke-width="1.3"/></svg>';
    document.documentElement.appendChild(c);
    const place = (x, y) => { c.style.transform = `translate(${x}px,${y}px)`; };
    place(120, 120);
    document.addEventListener('mousemove', e => place(e.clientX, e.clientY), true);
    document.addEventListener('mousedown', e => {
      const r = document.createElement('div');
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

(async () => {
  fs.mkdirSync(STILLDIR, { recursive: true });
  const browser = await chromium.launch({ headless: true, args: ['--force-color-profile=srgb'] });
  const ctx = await browser.newContext({
    viewport: { width: VW, height: VH }, deviceScaleFactor: 1,
    recordVideo: { dir: path.join(HERE, '.vid_' + base), size: { width: VW, height: VH } },
  });
  await ctx.addInitScript(CURSOR_INIT);
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
  }
  async function nav(u) { pinnedTips.length = 0; await page.goto(absurl(u), { waitUntil: 'load' }); await captureState(); await page.mouse.move(cur.x, cur.y); }
  async function glide(x, y) {
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
  async function shot(name) {
    const p = path.join(STILLDIR, name + '.png');
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
    } else {
      const el = (await page.locator('#imgTbl').count()) ? page.locator('#imgTbl') : page.locator('main');
      await el.screenshot({ path: p });
    }
    console.log('SHOT', p);
    await sleep(SHOTHOLD);
  }
  // Resolve a track name to its DOM key + data-image and row bounding boxes. On a
  // quickLift/Convert target the tracks come from a hub, so ids gain a dynamic
  // `hub_<n>_` prefix -- match by suffix so the YAML can just say `track: quickLiftChain`.
  async function trackBox(t) {
    const want0 = imgTrack(t);
    const key = await page.evaluate(k => {
      if (document.getElementById('img_data_' + k)) return k;
      const el = [...document.querySelectorAll('[id^="img_data_"]')]
        .find(e => e.id === 'img_data_' + k || e.id.endsWith('_' + k));
      return el ? el.id.replace('img_data_', '') : k;
    }, want0);
    const img = await page.locator(`#img_data_${key}`).first().boundingBox({ timeout: 8000 }).catch(() => null);
    const row = await page.locator(`#imgTbl tr#tr_${key}`).first().boundingBox({ timeout: 8000 }).catch(() => null);
    if (!img || !row) throw new Error(`track "${t}" not shown (need #img_data_${key} + #tr_${key})`);
    return { key, img, row };
  }
  // Resolve a NAMED item to a point {x,y} + its map-box HREF (the item's hgc link). We
  // pick the map <area> whose href(&i=<name>)/title carries the name AND whose box sits in
  // this track's own ROW band (so stacked items on other rows don't win); fall back to the
  // JSON mouseOver spans (wig/dense tracks, no per-item href).
  async function itemXY(t, want, titleOnly) {
    const { key, img, row } = await trackBox(t);
    const band = { top: row.y, bot: row.y + row.height };
    const area = await page.evaluate(({ want, titleOnly, band, imgBox }) => {
      const areas = [...document.querySelectorAll('map[name^="map_"] area')];
      const cands = [];
      for (const a of areas) {
        const href = a.getAttribute('href') || '';
        const title = a.getAttribute('title') || a.getAttribute('data-tooltip') || '';
        const hay = titleOnly ? title : (href + ' ' + title);
        if (!hay.includes(want)) continue;
        const c = (a.getAttribute('coords') || '').split(',').map(Number);
        if (c.length < 4) continue;
        // origin = the image this map is attached to (fall back to the data image box)
        const m = a.closest('map'), nm = m && m.getAttribute('name');
        const im = nm && document.querySelector(`img[usemap="#${nm}"]`);
        const r = im ? im.getBoundingClientRect() : null;
        const ox = r ? r.left : imgBox.x, oy = r ? r.top : imgBox.y;
        const cx = ox + (c[0] + c[2]) / 2, cy = oy + (c[1] + c[3]) / 2;
        cands.push({ cx, cy, href, inBand: cy >= band.top - 1 && cy <= band.bot + 1 });
      }
      const inBand = cands.filter(h => h.inBand);
      return (inBand[0] || cands[0]) || null;
    }, { want: String(want), titleOnly: !!titleOnly, band, imgBox: img });
    if (area) return { x: area.cx, y: area.cy, href: area.href };
    const span = await page.evaluate(({ keys, want }) => {
      const md = window.mapData; if (!md || !md.spans) return null;
      for (const k of keys) {
        const arr = md.spans[k]; if (!arr) continue;
        const s = arr.find(r => String(r.value || '').includes(want));
        if (s) return { x1: s.x1, x2: s.x2 };
      }
      return null;
    }, { keys: [key, t], want: String(want) });
    if (!span) throw new Error(`item "${want}" not found in track "${t}" (searched map-box areas + mouseOver spans)`);
    return { x: img.x + (span.x1 + span.x2) / 2, y: row.y + row.height / 2, href: null };
  }
  // POSITIONAL point: at:/frac:/x: -> x, y forced to the track row's middle. The grey
  // side-label strip (insideX) is baked into the image's left, so a fraction/coord maps
  // across [img.x+insideX, img.x+img.width], not the whole image width.
  async function posXY(t, o) {
    const { img, row } = await trackBox(t);
    const insideX = await page.evaluate(() => { try { return hgTracks.insideX || 0; } catch (_) { return 0; } });
    let x;
    if (o.x != null) x = img.x + insideX + Number(o.x);
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
    const { x, y } = (want != null)
      ? await itemXY(t, want, o.title != null && o.item == null && o.value == null)
      : await posXY(t, o);
    // Dismiss any tooltip a previous mouseover left open (over the grey side-label strip,
    // which has no items) so THIS item raises a FRESH tooltip -- otherwise a back-to-back
    // pinned mouseover can capture the previous item's stale popup text.
    await page.mouse.move(2, y); cur.x = 2; cur.y = y; await sleep(120);
    await glide(x, y);
    // small jiggle so the mousemove handler definitely fires and positions the tooltip
    await page.mouse.move(x + 1, y); await sleep(60); await page.mouse.move(x, y);
    await page.waitForSelector('#mouseoverContainer', { state: 'visible', timeout: 2500 }).catch(() => {});
    // Optionally RECORD this tooltip so a later `pinShot:` can show several mouseovers
    // open together in one figure. We only record (position + the tooltip's own HTML)
    // here -- nothing is injected into the recorded page, so the mp4 still shows just the
    // transient native tooltip. `pin:` on the step overrides the document-level
    // `pinMouseovers:` default. Records accumulate within a view and are cleared on nav.
    const pin = (o.pin != null) ? o.pin : (doc.pinMouseovers === true);
    if (pin) await recordTip(x, y);
    await sleep(o.hold != null ? Number(o.hold) * 1000 : SHOTHOLD);
    if (o.shot) await shot(o.shot);
  }
  // Grab the live mouseover tooltip's HTML and anchor it at the ITEM's coordinate (x,y
  // that mouseover just hovered), expressed RELATIVE TO the track image (#imgTbl). The
  // browser parks its own tooltip at a near-fixed spot, so two tips would stack; anchoring
  // to the item keeps each pinned tooltip on its own feature (and robust to the throwaway
  // page's image sitting at a different offset).
  async function recordTip(x, y) {
    const t = await page.evaluate(({ x, y }) => {
      const c = document.getElementById('mouseoverContainer');
      if (!c || !c.offsetWidth) return null;
      const im = document.getElementById('imgTbl');
      const ir = im ? im.getBoundingClientRect() : { left: 0, top: 0 };
      return { dx: x - ir.left + 8, dy: y - ir.top + 8, html: c.outerHTML };
    }, { x, y });
    if (t) pinnedTips.push(t);
  }
  // Render every recorded tooltip open at once in a still, WITHOUT touching the recorded
  // page (so the mp4 is unaffected): reload the current view on a throwaway page that
  // shares the session cookie (cart), inject the recorded tooltips, screenshot, discard.
  async function pinShot(name) {
    if (!pinnedTips.length) { console.warn(`pinShot ${name}: no pinned mouseovers recorded (set pin: true / pinMouseovers: true)`); return; }
    const url = page.url();
    const ctx2 = await browser.newContext({ viewport: { width: VW, height: VH }, deviceScaleFactor: 1 });
    await ctx2.addCookies(await ctx.cookies());
    const pg2 = await ctx2.newPage();
    await pg2.goto(url, { waitUntil: 'load' });
    await pg2.waitForSelector('#imgTbl', { timeout: 8000 }).catch(() => {});
    await pg2.evaluate((tips) => {
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
      }
    }, pinnedTips);
    const p = path.join(STILLDIR, name + '.png');
    const clip = await pg2.evaluate(() => {
      const im = document.getElementById('imgTbl'); if (!im) return null;
      const els = [im, ...document.querySelectorAll('.__pinnedTip')];
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
  // Shift+drag across the track image to open the browser's own drag-select dialog
  // ("Zoom In / Single Highlight / ..."), then act on it. Endpoints are given as
  // genomic coords (from:/to:), a fraction across the view (fromFrac:/toFrac:) or a
  // raw pixel (fromX:/toX:). Optional `track:` picks the row the drag runs over (y);
  // default is the middle of the image. `shot:` captures the open dialog (e.g. the
  // Figure 1A drag-select box). `then:` = zoom (default, clicks Zoom In) | highlight
  // (Single Highlight) | cancel (Escape, leaves the view unchanged).
  async function drag(o) {
    o = o || {};
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
    const insideX = await page.evaluate(() => { try { return hgTracks.insideX || 0; } catch (_) { return 0; } });
    const dataLeft = img.x + insideX, dataW = Math.max(1, img.width - insideX);
    const endX = async (px, fr, coord) => {
      if (px != null) return img.x + Number(px);
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
      const key = imgTrack(o.track);
      const row = await page.locator(`#imgTbl tr#tr_${key}`).first().boundingBox().catch(() => null);
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
    try {
      await glideTo('#view'); await page.hover('#view'); await sleep(900);
      await clickGlide('a#convertMenuLink'); await page.waitForSelector('#hglft_toDbSelect', { timeout: 8000 });
    } catch (e) {
      await nav(`/cgi-bin/hgConvert?hgsid=${state.hgsid}&db=${state.db}&position=${enc(state.position)}`);
      await page.waitForSelector('#hglft_toDbSelect');
    }
    const val = await resolveTarget(o.to);
    await openSelectVisible('#hglft_toDbSelect', val);
    await page.waitForSelector('#doQuickLift', { timeout: 8000 }).catch(() => {});
    await sleep(400);
    if (o.quicklift !== false) await checkGlide('#doQuickLift', true);
    await checkGlide('#hideTracksOnConvert', o.hideDefaults !== false);  // reverts on assembly change -> set explicitly
    await sleep(300);
    await clickGlide('#hglft_doConvert');
    await page.waitForLoadState('load'); await captureState();
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
        else { await nav(`/cgi-bin/hgTracks?db=${state.db}&position=${enc(arg)}&pix=${PIX}`); }
        await captureState(); break;
      case 'hide': if (arg === 'all' || arg === true) { await clickGlide('#hgt\\.hideAll'); await page.waitForSelector('#imgTbl'); } break;
      case 'track': {
        const parts = [];
        for (const [name, mode] of Object.entries(arg)) {
          // Visible gesture: drive the real track-controls dropdown so the mouse is
          // seen turning the track on. State is still applied by the nav() below
          // (so composite "clean" configs come through), so this open is non-committing.
          if (doc.trackAnim !== false) {
            const csel = `select[name="${ctrlName(name)}"]`;
            if (await page.locator(csel).count()) await openSelectVisible(csel, mode, 6, false);
          }
          const fn = TRACKS[name]; parts.push(...(fn ? fn(mode) : [`${name}=${mode}`]));
        }
        await nav(`/cgi-bin/hgTracks?db=${state.db}&position=${enc(state.position)}&${parts.join('&')}&pix=${PIX}`);
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
        parts.push(`pix=${PIX}`);
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
      case 'zoom': { const btn = (arg === 'in') ? '#hgt\\.in2' : '#hgt\\.out2'; await clickGlide(btn); await page.waitForSelector('#imgTbl'); break; }
      case 'shot': await shot(arg); return;                       // shot supplies its own dwell
      case 'pinShot': await pinShot(arg); break;                  // combined figure, off the mp4 timeline
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

  if (doc.reset) await page.goto(absurl('/cgi-bin/cartReset?skipLs=1'), { waitUntil: 'domcontentloaded' });
  const steps = doc.steps || [];
  for (let i = 0; i < steps.length; i++) {
    const s = norm(steps[i]);
    try { await run(s); }
    catch (e) { console.error(`step ${i + 1} (${s.verb}) failed:`, e.message); await ctx.close(); await browser.close(); process.exit(1); }
  }

  await page.waitForTimeout(300);
  await ctx.close();
  await browser.close();

  // transcode webm -> silent mp4
  const vdir = path.join(HERE, '.vid_' + base);
  const webm = fs.readdirSync(vdir).filter(f => f.endsWith('.webm')).map(f => path.join(vdir, f)).sort((a, b) => fs.statSync(b).mtimeMs - fs.statSync(a).mtimeMs)[0];
  const FF = execFileSync('python3', ['-c', 'import imageio_ffmpeg,sys;sys.stdout.write(imageio_ffmpeg.get_ffmpeg_exe())']).toString().trim();
  execFileSync(FF, ['-y', '-loglevel', 'error', '-i', webm, '-c:v', 'libx264', '-pix_fmt', 'yuv420p', '-crf', '22', '-preset', 'veryfast', '-movflags', '+faststart', OUTMP4]);
  fs.rmSync(vdir, { recursive: true, force: true });
  console.log('DONE ->', OUTMP4, '| stills in', STILLDIR);
})().catch(e => { console.error(e); process.exit(1); });
