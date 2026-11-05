/* targetConf.js -- where a Docent run is pointed, and what that server is configured like.
 *
 * Required by docent.js and by tests/preflight.js. It exists because those two have to
 * agree: preflight checks the fixtures for the server the run will actually drive, and a
 * run that resolved its target, its hg.conf or its account even slightly differently
 * would be checked against the wrong machine. They used to carry a copy each.
 *
 * Four questions, in order, each answered from the one before:
 *
 *   resolveTarget   a `target:` (or DOCENT_TARGET) -> the .../cgi-bin URL to drive
 *   hgConfFor       that URL -> the hg.conf it reads, when the server is on this machine
 *   centralDbFor    that conf -> which hgcentral it uses
 *   loginLookup     that central -> the account to sign in with
 *
 * Nothing here opens a browser or the network: it is file reading and string work, which
 * is why preflight can ask all of it in the seconds before a run.
 */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');

// `target:` takes a shorthand from this table, a bare `hgwdev-<user>` sandbox name, or a
// full https://.../cgi-bin URL. Default is genome-test, so a script that forgets to say
// where it runs does not silently hit someone's sandbox.
const SERVERS = {
  'rr': 'https://genome.ucsc.edu/cgi-bin',
  'genome-test': 'https://genome-test.gi.ucsc.edu/cgi-bin',
  'hgwdev': 'https://hgwdev.gi.ucsc.edu/cgi-bin',
  'hgwbeta': 'https://hgwbeta.soe.ucsc.edu/cgi-bin',
};

function resolveTarget(t) {
  if (!t) return SERVERS['genome-test'];
  if (SERVERS[t]) return SERVERS[t];
  if (/^hgwdev-[a-z0-9._-]+$/i.test(t)) return `https://${t}.gi.ucsc.edu/cgi-bin`;  // sandbox
  return t;                                                                         // full URL
}

// The server a run drives: DOCENT_TARGET wins over the script's own `target:`, so a suite
// written against one server can be pointed at another without editing the scripts.
function serverFor(target) {
  return resolveTarget(process.env.DOCENT_TARGET || target).replace(/\/$/, '');
}

// Which hg.conf that server reads, when it runs on THIS machine. There is no way to ask a
// browser over http what its hg.conf says, so this is a lookup by convention rather than a
// measurement, and it answers null for anything off this host -- hgwbeta, the RR, a
// colleague's machine.
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
    // The registry holds the http port; the same park answers https on that port + 1000.
    const httpPort = String(u.protocol === 'https:' ? Number(u.port) - 1000 : Number(u.port));
    for (const line of reg.split('\n')) {
      const f = line.split('\t');
      if (f[1] === httpPort) return path.join(root, f[0], 'cgi-bin', 'hg.conf');
    }
  }
  return null;
}

// hg.conf as hg/lib/hgConfig.c reads it: `include` pulls another file in relative to the
// including one, `delete` drops a name, and a later assignment wins over an earlier one
// (parseConfigLine hashAdds and cfgOption reads the most recently added). So the value
// this returns is the EFFECTIVE one -- a sandbox conf that sets nothing still shows what
// it inherits from the shared conf it includes.
//
// Callers print only the settings they name. The includes lead to hg.conf.private, which
// holds database passwords.
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

// Servers whose hg.conf is on another machine, so it cannot be read and has to be known.
const CENTRAL_BY_HOST = {
  'genome.ucsc.edu': 'hgcentral',
  'genome-euro.ucsc.edu': 'hgcentral',         // its own database of the same name
  'genome-asia.ucsc.edu': 'hgcentral',         // ... and so is this one
  'hgwbeta.soe.ucsc.edu': 'hgcentralbeta',
  'hgw0.soe.ucsc.edu': 'hgcentral',            // the RR node that takes a release first
};

// Which hgcentral a server reads. Read from its hg.conf wherever that is possible, because
// a sandbox may say so for itself: of the personal confs on hgwdev today, 45 set
// central.db to hgcentraltest and two do not (hgcentralgsid, hgcentralbeta).
function centralDbFor(server) {
  const file = hgConfFor(server);
  if (file) {
    const db = readHgConf(file).get('central.db');
    if (db) return db;
  }
  try { return CENTRAL_BY_HOST[new URL(server).hostname] || null; } catch (e) { return null; }
}

// ---------- the account a `login:` step signs in with ----------
// Keyed by hgcentral DATABASE, not by server: an account is a row in gbMembers in one of
// them, the way a named session is. genome-test, hgwdev, every hgwdev-<name> sandbox and
// every ticket park read hgcentraltest and share one account; hgwbeta reads hgcentralbeta
// and the RR reads hgcentral.
function loginFile() {
  return process.env.DOCENT_LOGIN_FILE || path.join(os.homedir(), '.docentLogin');
}

// Parse the sectioned file into [{central, user, password}], in file order. Lines before
// any [section] are ignored rather than treated as a default: an unsectioned file is one
// written against the older single-account form, and silently using it everywhere is how
// an hgcentraltest password would reach the RR.
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

// The one place that decides which account a server gets, so the run and the fixture check
// cannot disagree about it. Returns the facts and, when it cannot be used, one sentence
// saying why -- that sentence is the substantive half and is shared; each caller phrases
// its own success line, since one logs and the other prints a fixture row.
//
//   { central, user, password, section, source, why }
//
// `section` is the heading it matched ('hgcentraltest', 'default'), `source` the file it
// came from or 'the environment'. On failure user and password are null and `why` is set.
// No password is returned to anything that prints, and none is ever logged.
function loginLookup(server) {
  const env = process.env;
  const central = centralDbFor(server);
  const where = central ? `central.db ${central}` : 'central.db unknown';
  const fail = why => ({ central, user: null, password: null, section: null, source: null, why });
  // A single-run override, for trying an account without writing it down. It applies to
  // whatever server this run drives, which is why it wins over the file.
  if (env.DOCENT_LOGIN_USER && env.DOCENT_LOGIN_PASSWORD)
    return { central, user: env.DOCENT_LOGIN_USER, password: env.DOCENT_LOGIN_PASSWORD,
             section: null, source: 'the environment', why: null };
  const file = loginFile();
  let text;
  try { text = fs.readFileSync(file, 'utf8'); }
  catch (e) {
    return fail(`no ${file}, and no DOCENT_LOGIN_USER/DOCENT_LOGIN_PASSWORD. Write it with a `
      + `[<hgcentral database>] section holding "user=" and "password=" lines (mode 0600)`);
  }
  // Refuse a file anyone else can read, the way hg/lib/hgConfig.c checkConfigPerms refuses
  // a group- or world-readable hg.conf. A test that quietly used a readable password file
  // would make one on every machine it ran on.
  const perm = fs.statSync(file).mode & 0o777;
  if (perm & 0o077)
    return fail(`${file} is readable by group or other (mode ${perm.toString(8)}); chmod 600 it`);
  const secs = loginSections(text);
  const match = (central && secs.find(x => x.central === central))
             || secs.find(x => x.central === 'default');
  if (!match)
    return fail(`${file} has no section for ${where}`
      + (secs.length ? ` (it has ${secs.map(x => `[${x.central}]`).join(' ')})` : ' (it has no [section] at all)')
      + '; add one, or a [default]');
  if (!match.user || !match.password)
    return fail(`[${match.central}] in ${file} needs a "user=" and a "password=" line`);
  return { central, user: match.user, password: match.password,
           section: match.central, source: file, why: null };
}

module.exports = { SERVERS, resolveTarget, serverFor, hgConfFor, readHgConf,
                   CENTRAL_BY_HOST, centralDbFor, loginFile, loginSections, loginLookup };
