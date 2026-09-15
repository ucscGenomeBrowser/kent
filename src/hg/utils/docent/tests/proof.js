#!/usr/bin/env node
/* proof.js [DIR] [SCRIPT...]
 *
 * Reports what evidence each Docent test in DIR actually has that it would catch the
 * bug it was written for, and tallies it.
 *
 * The problem it solves. A regression test written after the fix asserts the behavior
 * the ticket says is correct, but nobody has seen it fail for the reason it exists. A
 * loose assertion in that state is indistinguishable from no test at all, and a reader
 * a year from now cannot tell which scripts have been proven and which are hopeful.
 * That evidence was being recorded in prose in each script's header, where counting it
 * meant a grep and reading, so the claim "these are regression tests" could not be made
 * out loud with a number behind it.
 *
 * So every script carries a top-level `proof:` key, a list of one line per piece of
 * evidence, each beginning with a level token from LEVELS below and a date:
 *
 *     proof:
 *       - "sandbox-ab 2026-09-09 -- ticket sandbox on 48099, patched and control builds"
 *       - "server-flip 2026-09-10 -- 68f831e1209 reached genome-test, the xfail passed"
 *
 * QUOTE every line. Nearly all of them name a ticket, and a bare `#` in an unquoted YAML
 * scalar starts a comment, so the note is silently cut off at the ticket number and the
 * evidence disappears from the middle of the sentence. That is checked for below rather
 * than left as a trap.
 *
 * docent.js reads only the keys it names off the parsed document, so `proof:` costs it
 * nothing and is never executed. Nothing here drives a browser either: this reads the
 * yaml and prints, so it is instant and can go in a commit message.
 *
 * Exit 1 if any proof line is malformed or names a level that is not in the vocabulary,
 * because a vocabulary nobody enforces drifts into free text within a month, and free
 * text cannot be tallied. A script with no `proof:` key at all is NOT an error -- the
 * tests next door are not regression tests and have no bug to have proven -- but it is
 * counted and listed under "unrecorded" so it cannot hide.
 */
'use strict';
const fs = require('fs');
const path = require('path');
const yaml = require('js-yaml');

// Weakest first. The order is the point: it is what `make proof` sorts and sums by, and
// what tells you whether the directory is getting stronger.
const LEVELS = [
  ['assertion-only',
   'asserts the fixed behavior; never seen to fail for the reason it exists'],
  ['xfail',
   'seen failing right now for the reason it exists; the fix has not shipped'],
  ['sandbox-ab',
   'seen failing on a build with the bug and passing on a build with the fix, both built by hand'],
  ['server-flip',
   'seen failing then passing on a real server as a real build arrived'],
  ['caught-regression',
   'went red for a regression that was then filed and fixed'],
];
const RANK = new Map(LEVELS.map(([n], i) => [n, i]));

// Same shape as preflight.js: DIR first, then optional bare script names, because the
// nightly hands both of them the same committed list.
const DIR = process.argv[2] || '.';
const named = process.argv.slice(3).map(s => s.replace(/\.docent\.yaml$/, ''));

const all = fs.readdirSync(DIR).filter(f => f.endsWith('.docent.yaml')).sort();
let files = all;
if (named.length) {
  // A name that matches nothing has to be an error, not a quiet omission. `make proof
  // T=rm3831` would otherwise print a clean tally of zero scripts and read as all clear.
  const missing = named.filter(n => !all.includes(`${n}.docent.yaml`));
  if (missing.length) {
    console.log(`no such script in ${DIR}: ${missing.join(', ')}`);
    process.exit(1);
  }
  files = named.map(n => `${n}.docent.yaml`);
}
if (!files.length) { console.log(`no *.docent.yaml in ${DIR}`); process.exit(1); }

// A line is `<level> <YYYY-MM-DD> -- <what was seen>`. The prose after the dashes is
// required: a level and a date with no account of what was watched is a claim, not
// evidence, and the next reader has no way to check it.
const LINE = /^([a-z-]+) (\d{4}-\d{2}-\d{2}) -- (\S.*)$/;

const rows = [];
let bad = 0;

for (const f of files) {
  const base = f.replace(/\.docent\.yaml$/, '');
  let doc, text;
  try {
    text = fs.readFileSync(path.join(DIR, f), 'utf8');
    // An unquoted proof line that names a ticket loses everything from the `#` on, and
    // the parse cannot tell you so -- what comes back is a shorter sentence that still
    // reads like a whole one. Catch it in the raw text, where the `#` is still there.
    for (const l of text.split('\n')) {
      const m = /^ {2}- (?!["'])(.*\s#.*)$/.exec(l);
      if (m && /^[a-z-]+ \d{4}-\d{2}-\d{2} --/.test(m[1])) {
        console.log(`${base}: UNQUOTED PROOF LINE -- everything from the # is a YAML comment`);
        console.log(`  ${l.trim()}`);
        bad++;
      }
    }
    doc = yaml.load(text) || {};
  } catch (e) {
    console.log(`${base}: UNREADABLE -- ${e.message}`);
    bad++;
    continue;
  }
  const raw = doc.proof;
  if (raw === undefined) { rows.push({ base, level: null, lines: [] }); continue; }
  const lines = Array.isArray(raw) ? raw : [raw];
  const parsed = [];
  for (const l of lines) {
    const m = LINE.exec(String(l).trim());
    if (!m) {
      console.log(`${base}: BAD PROOF LINE -- ${l}`);
      console.log(`  wanted: <level> <YYYY-MM-DD> -- <what was seen>`);
      bad++;
      continue;
    }
    if (!RANK.has(m[1])) {
      console.log(`${base}: UNKNOWN PROOF LEVEL "${m[1]}"`);
      console.log(`  known: ${LEVELS.map(([n]) => n).join(', ')}`);
      bad++;
      continue;
    }
    parsed.push({ level: m[1], date: m[2], note: m[3] });
  }
  // A script is filed under the STRONGEST thing it has been seen to do. The weaker
  // entries stay on the script, because how it got there is the interesting part.
  const best = parsed.reduce((a, b) => (a && RANK.get(a.level) >= RANK.get(b.level) ? a : b), null);
  rows.push({ base, level: best ? best.level : null, lines: parsed });
}
if (bad) { console.log(`\n${bad} bad proof line(s) -- fix these before believing the tally`); process.exit(1); }

const width = Math.max(0, ...rows.map(r => r.base.length));
const byLevel = new Map(LEVELS.map(([n]) => [n, []]));
const unrecorded = [];
for (const r of rows) (r.level ? byLevel.get(r.level) : unrecorded).push(r);

// assertion-only is printed as bare names. Its note is the same sentence on every
// script by construction -- there is nothing to say about a bug nobody watched -- and
// thirty lines of it buries the four that carry the evidence.
const wrap = (names, indent) => {
  const out = []; let line = indent;
  for (const n of names) {
    if (line.length + n.length + 2 > 90) { out.push(line); line = indent; }
    line += (line === indent ? '' : '  ') + n;
  }
  if (line.trim()) out.push(line);
  return out.join('\n');
};

for (const [name, what] of LEVELS) {
  const got = byLevel.get(name);
  if (!got.length) continue;
  console.log(`\n${name} (${got.length}) -- ${what}`);
  if (name === 'assertion-only') { console.log(wrap(got.map(r => r.base), '  ')); continue; }
  for (const r of got)
    for (const l of r.lines)
      console.log(`  ${r.base.padEnd(width)}  ${l.level} ${l.date} -- ${l.note}`);
}
if (unrecorded.length) {
  console.log(`\nunrecorded (${unrecorded.length}) -- no proof: key`);
  for (const r of unrecorded) console.log(`  ${r.base}`);
}

// The summary line is the claim, and it is deliberately the pessimistic reading:
// anything weaker than sandbox-ab has not been watched fail for its own reason.
const proven = LEVELS.slice(RANK.get('sandbox-ab'))
                     .reduce((n, [l]) => n + byLevel.get(l).length, 0);
console.log(`\n${rows.length} script(s): ${proven} watched to fail for the reason it exists`
          + ` and then pass, ${rows.length - proven} not.`);
