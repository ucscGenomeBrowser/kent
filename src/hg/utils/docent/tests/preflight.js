#!/usr/bin/env node
/* preflight.js [DIR] [SCRIPT...]
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
 *
 * With no script names it checks every *.docent.yaml in DIR, which is what you want
 * interactively. Name scripts to check only those: the nightly run passes the COMMITTED
 * list, because a directory also holds work in progress and a dead fixture belonging to
 * a script that is not running should not be reported as a problem.
 *
 * It also prints how the target is CONFIGURED, which is the other half of reading a
 * redirected run. `make test TARGET=hgwdev-you` against a personal sandbox can go red
 * for reasons that are not the code: a different curatedHubPrefix changes what a
 * quickLift hop produces, and a db.trackDb with a private table in front of the shared
 * one changes what `exact: true` counts. Those failures look exactly like a bug, and
 * telling the two apart has cost an hour more than once. So when the target is served
 * from THIS machine the settings that decide it are printed beside the fixtures, and the
 * log then carries its own explanation.
 *
 * Only the handful of settings named in HG_CONF_KEYS is ever printed. hg.conf includes
 * hg.conf.private, which holds database passwords, so the reader below parses whatever
 * the includes lead to and prints nothing that is not on that list.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const os = require('os');
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

// The settings that change what a test sees, and nothing else. central.db says which
// hgcentral the named sessions above were looked for in; db.trackDb which trackDb tables
// the track names resolve against; curatedHubPrefix which curated hubs are attached; the
// browser.* three whether the features several scripts here depend on are even on.
const HG_CONF_KEYS = ['central.db', 'db.trackDb', 'curatedHubPrefix',
                      'browser.quickLift', 'browser.quickLiftAlignments',
                      'browser.recTrackSets'];

// Which hg.conf the server named by a target: reads, when that server runs on this
// machine. There is no way to ask a browser over http what its hg.conf says, so this is
// a lookup by convention rather than a measurement, and it answers null for anything off
// this host -- hgwbeta, the RR, a colleague's machine.
function hgConfFor(server) {
  let u;
  try { u = new URL(server); } catch (e) { return null; }
  const host = u.hostname;                            // 127.0.0.1 keeps its dots
  const name = host.split('.')[0];                    // hgwdev-braney out of the FQDN
  if (name === 'hgwdev' || name === 'genome-test') return '/usr/local/apache/cgi-bin/hg.conf';
  if (name.startsWith('hgwdev-'))                     // a sandbox or a demo browser
    return `/usr/local/apache/cgi-bin-${name.slice('hgwdev-'.length)}/hg.conf`;
  if ((host === '127.0.0.1' || host === 'localhost') && u.port) {
    // A ticket park from `ts`: its port is in the registry, and the frozen hg.conf sits
    // under the ticket's own directory. A park is the one target whose conf is NOT the
    // live sandbox's, which is the whole reason for parking it.
    const root = process.env.TS_ROOT || path.join(os.homedir(), 'ticketSandboxes');
    let reg;
    try { reg = fs.readFileSync(path.join(root, 'ports.tsv'), 'utf8'); } catch (e) { return null; }
    for (const line of reg.split('\n')) {
      const f = line.split('\t');
      if (f[1] === u.port) return path.join(root, f[0], 'cgi-bin', 'hg.conf');
    }
  }
  return null;
}

// hg.conf as hg/lib/hgConfig.c reads it: `include` pulls another file in relative to the
// including one, `delete` drops a name, and a later assignment wins over an earlier one
// (parseConfigLine hashAdds and cfgOption reads the most recently added).
function readHgConf(file, out = new Map(), seen = new Set(), depth = 0) {
  if (depth > 10 || seen.has(file)) return out;
  seen.add(file);
  let text;
  try { text = fs.readFileSync(file, 'utf8'); } catch (e) { return out; }
  for (const raw of text.split('\n')) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    if (/^include\s/.test(line))
      readHgConf(path.resolve(path.dirname(file), line.slice(7).trim()), out, seen, depth + 1);
    else if (/^delete\s/.test(line))
      for (const name of line.slice(6).trim().split(/\s+/)) out.delete(name);
    else {
      const eq = line.indexOf('=');
      if (eq > 0) out.set(line.slice(0, eq).trim(), line.slice(eq + 1).trim());
    }
  }
  return out;
}

// Which hgcentral a server reads. Read from its hg.conf when that is on this machine,
// because a sandbox may say so for itself -- 45 of the personal confs on hgwdev set
// central.db to hgcentraltest and two do not. Otherwise a table for the servers whose
// conf is somewhere else. docent.js carries the same lookup; keep the two in step.
const CENTRAL_BY_HOST = {
  'genome.ucsc.edu': 'hgcentral',
  'genome-euro.ucsc.edu': 'hgcentral',         // its own database of the same name
  'genome-asia.ucsc.edu': 'hgcentral',         // ... and so is this one
  'hgwbeta.soe.ucsc.edu': 'hgcentralbeta',
};
function centralDbFor(server) {
  const file = hgConfFor(server);
  if (file) {
    const db = readHgConf(file).get('central.db');
    if (db) return db;
  }
  try { return CENTRAL_BY_HOST[new URL(server).hostname] || null; } catch (e) { return null; }
}

const PAD = '              ';
function reportTargetConf(server) {
  console.log(`  target      ${server}`);
  const file = hgConfFor(server);
  if (!file) {
    console.log(`${PAD}not on this machine, so its hg.conf cannot be read from here`);
    return;
  }
  if (!fs.existsSync(file)) {
    console.log(`${PAD}${file} -- no such file`);
    return;
  }
  const conf = readHgConf(file);
  const w = Math.max(...HG_CONF_KEYS.map(k => k.length));
  console.log(`${PAD}${file}`);
  for (const k of HG_CONF_KEYS)
    console.log(`${PAD}${k.padEnd(w)}  ${conf.has(k) ? conf.get(k) : 'unset'}`);
}

// The credentials a `login:` step needs, resolved the same way docent.js resolves them:
// keyed by the HGCENTRAL the server reads, because an account is a row in gbMembers in
// one of them. genome-test, hgwdev, every sandbox and every ticket park read
// hgcentraltest, so one account covers all of them; hgwbeta and the RR are separate sets
// of accounts. Checked here because a missing password is exactly the kind of fixture
// this program exists for -- the run would otherwise get as far as hgLogin before it
// said so.
//
// No password, and no line of the file, is ever printed, and no login is attempted: a
// wrong password fails loudly at the step itself, which is the one thing preflight cannot
// do for it.
function loginSections(text) {
  const out = [];
  let cur = null;
  for (const raw of text.split('\n')) {
    const line = raw.trim();
    if (!line || line.startsWith('#')) continue;
    const sec = /^\[(.+)\]$/.exec(line);
    if (sec) { cur = { central: sec[1].trim(), user: '', password: '' }; out.push(cur); continue; }
    const eq = line.indexOf('=');
    if (eq < 0 || !cur) continue;
    const k = line.slice(0, eq).trim(), v = line.slice(eq + 1).trim();
    if (k === 'user' || k === 'password') cur[k] = v;
  }
  return out;
}

// Returns {label, why}: what to show for this server's account, and why it is unusable.
// Keyed by the hgcentral the server reads, since that is where gbMembers lives.
function loginAccount(server) {
  const env = process.env;
  if (env.DOCENT_LOGIN_USER && env.DOCENT_LOGIN_PASSWORD)
    return { label: `${env.DOCENT_LOGIN_USER} (from the environment)`, why: null };
  const central = centralDbFor(server);
  const where = central ? `central.db ${central}` : 'central.db unknown';
  const file = env.DOCENT_LOGIN_FILE || path.join(os.homedir(), '.docentLogin');
  let text;
  try { text = fs.readFileSync(file, 'utf8'); }
  catch (e) { return { label: where, why: `no ${file}, and no DOCENT_LOGIN_USER/PASSWORD` }; }
  const perm = fs.statSync(file).mode & 0o777;
  if (perm & 0o077)
    return { label: where, why: `${file} is readable by group or other (mode ${perm.toString(8)}); chmod 600 it` };
  const secs = loginSections(text);
  const match = (central && secs.find(x => x.central === central))
             || secs.find(x => x.central === 'default');
  if (!match)
    return { label: where, why: `${file} has no section for ${where}`
      + (secs.length ? ` (it has ${secs.map(x => `[${x.central}]`).join(' ')})` : ' (it has no [section] at all)') };
  if (!match.user || !match.password)
    return { label: `[${match.central}]`, why: `[${match.central}] in ${file} needs a "user=" and a "password=" line` };
  return { label: `${match.user} (from [${match.central}], ${where})`, why: null };
}

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

const named = process.argv.slice(3)
  .map(a => a.endsWith('.docent.yaml') ? a : `${a}.docent.yaml`);
const scripts = (named.length ? named
                             : fs.readdirSync(DIR).filter(f => f.endsWith('.docent.yaml'))).sort();
const seenServer = new Map();

for (const f of scripts) {
  let doc;
  try {
    doc = yaml.load(fs.readFileSync(path.join(DIR, f), 'utf8')) || {};
  } catch (e) {
    add({ script: f, kind: 'script', label: f, check: async () => `unreadable YAML: ${e.message}` });
    continue;
  }
  // DOCENT_TARGET redirects the run, so the server fixture has to be the one that will
  // actually be driven rather than the one the script names. Same override as docent.js.
  const server = resolveTarget(process.env.DOCENT_TARGET || doc.target).replace(/\/$/, '');
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
    } else if (verb === 'login') {
      // Keyed by server, so a directory pointed at two of them reports two accounts.
      const acct = loginAccount(server);
      add({ script: f, kind: 'login', label: `${acct.label} on ${server}`,
            check: async () => acct.why });
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
  // Before the fixtures, because it is what a red line further down has to be read
  // against. Costs no network and no browser.
  for (const server of seenServer.keys()) reportTargetConf(server);
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
