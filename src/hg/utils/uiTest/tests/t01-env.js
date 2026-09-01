// The conf file and the target table: parsing, include, delete, permissions,
// and which of CLI, environment and conf wins.

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const t = require('./assert');
const env = require('../lib/env');

const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'uiTestSelf-'));
const write = (name, text, mode) => {
    const f = path.join(tmp, name);
    fs.writeFileSync(f, text);
    fs.chmodSync(f, mode == null ? 0o600 : mode);
    return f;
};

// --- name=value, comments, blank lines ---
const basic = write('.hg.uiTest.conf', [
    '# a comment',
    '',
    'default.target = hgwbeta',
    'account.qa.user   =   someoneQa   ',
    'can.hgsql = yes',
].join('\n'));
const c1 = env.readConf(basic);
t.is(c1.vals['default.target'], 'hgwbeta', 'reads a value');
t.is(c1.vals['account.qa.user'], 'someoneQa', 'trims space around name and value');
t.is(c1.vals['# a comment'], undefined, 'ignores a comment');

// --- include, relative to the including file, and delete ---
write('.shared.conf', 'target.tip = http://127.0.0.1:8081/cgi-bin\ncan.write = no\n');
const withInc = write('.inc.conf', [
    'include .shared.conf',
    'delete can.write',
    'default.target = tip',
].join('\n'));
const c2 = env.readConf(withInc);
t.is(c2.vals['target.tip'], 'http://127.0.0.1:8081/cgi-bin', 'include pulls in the other file');
t.is(c2.vals['can.write'], undefined, 'delete removes a variable the include set');
t.is(c2.files.length, 2, 'both files are recorded');

// A missing include is ignored, the same as hgConfig.c does.
const missing = write('.missing.conf', 'include .noSuchFile.conf\nk = v\n');
t.is(env.readConf(missing).vals['k'], 'v', 'a missing include is not an error');

// --- a file that is not there at all is not an error either ---
t.is(env.readConf(path.join(tmp, '.absent.conf')).files.length, 0,
    'an absent conf file reads as empty');

// --- permissions: the port of checkConfigPerms() ---
const loose = write('.loose.conf', 'k = v\n', 0o640);
t.throws(() => env.readConf(loose), /allows group or other access/,
    'a group-readable dot file is rejected');
const notDot = write('shared.conf', 'k = v\n', 0o644);
t.is(env.readConf(notDot).vals['k'], 'v',
    'a file whose name does not start with a dot is not permission-checked');

// --- bad syntax ---
const noEq = write('.bad.conf', 'this line has no equals sign\n');
t.throws(() => env.readConf(noEq), /invalid format in config file/, 'a line with no = is rejected');
const badInc = write('.badinc.conf', 'include one two\n');
t.throws(() => env.readConf(badInc), /invalid format for config include/,
    'an include with two arguments is rejected');

// --- target resolution ---
t.is(env.resolveTarget('genome-test', {}), 'https://genome-test.gi.ucsc.edu/cgi-bin',
    'a compiled shorthand resolves');
t.is(env.resolveTarget('hgwdev-someone', {}), 'https://hgwdev-someone.gi.ucsc.edu/cgi-bin',
    'hgwdev-<user> expands to a sandbox');
t.is(env.resolveTarget('http://127.0.0.1:8081/cgi-bin/', {}), 'http://127.0.0.1:8081/cgi-bin',
    'a full URL passes through, with any trailing slash trimmed');
t.is(env.resolveTarget('tip', { 'target.tip': 'http://127.0.0.1:8081/cgi-bin' }),
    'http://127.0.0.1:8081/cgi-bin', 'a conf target.<name> line resolves');
t.is(env.resolveTarget('hgwdev', { 'target.hgwdev': 'https://elsewhere/cgi-bin' }),
    'https://elsewhere/cgi-bin', 'the conf wins over the compiled table');
t.throws(() => env.resolveTarget('nonsense', {}), /unknown target/,
    'an unknown target is a config error, not a silent URL');

// --- precedence: CLI > env > conf > compiled ---
const prec = write('.prec.conf', 'default.target = hgwbeta\n');
t.is(env.load({ conf: prec }).target, 'hgwbeta', 'the conf beats the compiled default');
process.env.UITEST_TARGET = 'hgwdev';
t.is(env.load({ conf: prec }).target, 'hgwdev', 'the environment beats the conf');
t.is(env.load({ conf: prec, target: 'genome-test' }).target, 'genome-test',
    'a CLI flag beats the environment');
delete process.env.UITEST_TARGET;
t.is(env.load({ conf: path.join(tmp, '.absent.conf') }).target, 'genome-test',
    'with no conf at all the compiled default is genome-test');

// --- accounts ---
const acct = write('.acct.conf', [
    'default.account = qa',
    'account.qa.user = someoneQa',
].join('\n'));
const withAcct = env.load({ conf: acct, target: 'hgwdev' });
t.is(withAcct.account.user, 'someoneQa', 'an account resolves for an ordinary target');
const onRr = env.load({ conf: acct, target: 'rr' });
t.is(onRr.account, null, 'rr gets no account even when one is configured');
t.ok(/no account configured for target rr/.test(onRr.accountReason),
    'and says why');
const noUser = write('.nouser.conf', 'default.account = qa\n');
const bad = env.load({ conf: noUser, target: 'hgwdev' });
t.is(bad.account, null, 'an account block with no user line yields no account');
t.ok(/account\.qa\.user/.test(bad.accountReason), 'and names the missing line');
const none = write('.none.conf', [
    'default.account = qa',
    'account.qa.user = someoneQa',
    'target.hgwbeta.account = (none)',
].join('\n'));
t.is(env.load({ conf: none, target: 'hgwbeta' }).account, null,
    '(none) turns off login for one target');

// --- booleans ---
['yes', 'on', 'true', '1', 'YES'].forEach(v =>
    t.ok(env.asBoolean(v, false), `"${v}" is true`));
['no', 'off', 'false', '0', ''].forEach(v =>
    t.ok(!env.asBoolean(v, true), `"${v}" is false`));
t.ok(env.asBoolean(undefined, true), 'an unset value takes the default');

fs.rmSync(tmp, { recursive: true, force: true });
t.done('t01-env');
