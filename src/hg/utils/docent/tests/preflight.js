#!/usr/bin/env node
/* preflight.js [DIR]
 *
 * Checks the things a directory of Docent tests depends on but does not contain: the
 * saved sessions the scripts load, the hubs they attach, the custom-track URLs they
 * fetch, and the server they all point at. No browser, so it runs in seconds and can be
 * run far more often than the suite it guards.
 *
 * It exists because of the way these tests fail without it. A saved session that has
 * been renamed or deleted produces no error: hgTracks answers 200 with a page titled
 * "Very Early Error" whose body reads "Could not find session NAME for user USER", the
 * page carries no track image, and every `noText:` assertion on it passes. The run goes
 * green while testing nothing. A hub URL that has moved behaves the same way.
 *
 * Fixtures are read out of the scripts rather than from a list kept beside them, so the
 * two cannot drift. A fixture named in no script is not checked, which is the point.
 *
 * Exit 0 if every fixture resolved, 1 otherwise, so a nightly run can tell "the fixtures
 * are gone" from "a bug came back". Without this they are the same red.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');

const DIR = process.argv[2] || '.';
const SERVERS = {
  'rr': 'https://genome.ucsc.edu/cgi-bin',
  'genome-test': 'https://genome-test.gi.ucsc.edu/cgi-bin',
  'hgwdev': 'https://hgwdev.gi.ucsc.edu/cgi-bin',
  'hgwbeta': 'https://hgwbeta.soe.ucsc.edu/cgi-bin',
};
const resolveTarget = t => {
  if (!t) return SERVERS['genome-test'];
  if (SERVERS[t]) return SERVERS[t];
  if (/^hgwdev-[a-z0-9._-]+$/i.test(t)) return `https://${t}.gi.ucsc.edu/cgi-bin`;
  return t;
};
const enc = encodeURIComponent;

const fixtures = [];
const add = f => fixtures.push(f);

async function get(url) {
  const r = await fetch(url, { redirect: 'follow' });
  return { status: r.status, body: await r.text() };
}

// A session is present when the page does NOT carry hgSession's own not-found message.
// Requiring a track image instead would be wrong: a session may legitimately restore a
// view with every track hidden.
function sessionCheck(server, user, name) {
  const url = `${server}/hgTracks?hgS_doOtherUser=submit`
    + `&hgS_otherUserName=${enc(user)}&hgS_otherUserSessionName=${enc(name)}`;
  return async () => {
    const { status, body } = await get(url);
    if (status !== 200) return `HTTP ${status}`;
    // Match the stem of the message rather than rebuilding the whole string, which would
    // depend on how the name was escaped on the way in.
    if (/Could not find session/i.test(body)) return 'no such session on this server';
    if (/<TITLE>\s*Very Early Error/i.test(body)) return 'server returned an early error';
    return null;
  };
}

function urlCheck(url, wantText) {
  return async () => {
    const { status, body } = await get(url);
    if (status !== 200) return `HTTP ${status}`;
    if (!body.trim()) return 'empty response';
    if (wantText && !body.includes(wantText)) return `no "${wantText}" in the response`;
    return null;
  };
}

const fileCheck = file => async () => fs.existsSync(file) ? null : 'no such file';

const scripts = fs.readdirSync(DIR).filter(f => f.endsWith('.docent.yaml')).sort();
const seenServer = new Map();

for (const f of scripts) {
  let doc;
  try {
    doc = yaml.load(fs.readFileSync(path.join(DIR, f), 'utf8')) || {};
  } catch (e) {
    add({ script: f, kind: 'script', label: f, check: async () => `unreadable YAML: ${e.message}` });
    continue;
  }
  const server = resolveTarget(doc.target).replace(/\/$/, '');
  if (!seenServer.has(server)) seenServer.set(server, f);
  const base = path.basename(f, '.docent.yaml');

  for (const step of (doc.steps || [])) {
    if (!step || typeof step !== 'object') continue;
    const verb = Object.keys(step)[0];
    const arg = step[verb];
    const o = (arg && typeof arg === 'object') ? arg : null;

    if (verb === 'loadSession') {
      if (o && o.user && o.name) {
        add({ script: f, kind: 'session', label: `${o.user}/${o.name}`,
              check: sessionCheck(server, o.user, o.name) });
      } else if (o && o.file) {
        // A session: step earlier in the same script writes this, so it is only a
        // fixture when nothing here creates it.
        const writes = (doc.steps || []).some(s => s && typeof s === 'object' && s.session === o.file);
        if (!writes) {
          add({ script: f, kind: 'file', label: o.file,
                check: fileCheck(path.join(DIR, 'sessions', base, `${o.file}.txt`)) });
        }
      } else if (typeof arg === 'string' && /^https?:/.test(arg)) {
        add({ script: f, kind: 'session-url', label: arg, check: urlCheck(arg) });
      }
    } else if (verb === 'goto' && typeof arg === 'string') {
      // A hub can also arrive inside a goto: URL, which is the only way to write a test
      // about the genome= form (the hub: verb builds db=). Pull hubUrl out of the query
      // so those hubs are checked too, rather than being invisible to preflight because
      // of how the step happens to be spelled.
      const m = /[?&]hubUrl=([^&]+)/.exec(arg);
      if (m) {
        const url = decodeURIComponent(m[1]);
        add({ script: f, kind: 'hub', label: url, check: urlCheck(url, 'hub') });
      }
    } else if (verb === 'hub' || verb === 'addHub') {
      const url = (typeof arg === 'string') ? arg : (o && o.url);
      // A hub.txt replaced by a directory listing or an error page still answers 200, so
      // require the one word every hub.txt has to contain.
      if (url) add({ script: f, kind: 'hub', label: url, check: urlCheck(url, 'hub') });
    } else if (verb === 'addCustomTrack') {
      if (o && o.url) add({ script: f, kind: 'ct-url', label: o.url, check: urlCheck(o.url) });
      const rel = o && (o.file || o.pasteFile);
      if (rel) add({ script: f, kind: 'file', label: rel, check: fileCheck(path.join(DIR, rel)) });
    }
  }
}

for (const [server, f] of seenServer) {
  fixtures.unshift({ script: f, kind: 'server', label: server,
                     check: urlCheck(`${server}/hgTracks?db=hg38&pix=800`) });
}

(async () => {
  if (!fixtures.length) {
    console.log(`preflight: ${scripts.length} script(s), no external fixtures to check`);
    process.exit(0);
  }
  // One fixture named by several scripts is one check, reported against all of them.
  const byKey = new Map();
  for (const fx of fixtures) {
    const k = `${fx.kind} ${fx.label}`;
    if (!byKey.has(k)) byKey.set(k, { ...fx, scripts: [] });
    byKey.get(k).scripts.push(fx.script);
  }
  const all = [...byKey.values()];
  const results = await Promise.all(all.map(async fx => {
    let why;
    try {
      why = await fx.check();
    } catch (e) {
      why = `unreachable: ${e.message}`;
    }
    return { ...fx, why };
  }));

  results.sort((a, b) => a.kind.localeCompare(b.kind) || a.label.localeCompare(b.label));
  for (const r of results) {
    console.log(`  ${(r.why ? 'MISSING' : 'ok').padEnd(8)}${r.kind.padEnd(12)}${r.label}`);
    if (r.why) {
      console.log(`            ${r.why}  --  needed by ${[...new Set(r.scripts)].join(', ')}`);
    }
  }
  const bad = results.filter(r => r.why).length;
  console.log(`preflight: ${all.length} fixture(s) for ${scripts.length} script(s), ${bad} missing`);
  process.exit(bad ? 1 : 0);
})();
