// What a test's `needs` block decides, before any browser launches: run, skip
// with a reason, or refuse outright.
//
// The one that matters most is the last: a test that writes to a server must
// never run against production, and that is not skippable. Skipping it would
// mean a green run that quietly did nothing, which is how somebody eventually
// uploads a qa123456 hub to genome.ucsc.edu.

'use strict';

const t = require('./assert');
const { resolveNeeds } = require('../lib/run');
const { EXIT } = require('../lib/env');

const dev = {
    target: 'hgwdev', isRr: false, canWrite: true, canHgsql: true,
    account: { name: 'qa', user: 'someoneQa' },
    accountReason: '', hgsqlReason: '', confFile: '/home/someone/.hg.uiTest.conf',
};
const bare = {
    target: 'genome-test', isRr: false, canWrite: true, canHgsql: false,
    account: null,
    accountReason: 'no conf file at /home/someone/.hg.uiTest.conf',
    hgsqlReason: 'can.hgsql is not set in /home/someone/.hg.uiTest.conf',
    confFile: '/home/someone/.hg.uiTest.conf',
};
const rr = { ...dev, target: 'rr', isRr: true, account: null, accountReason: 'production' };

t.is(resolveNeeds(undefined, bare), null, 'a test with no needs block always runs');
t.is(resolveNeeds({}, bare), null, 'an empty needs block always runs');
t.is(resolveNeeds({ login: false, hgsql: false, write: false }, bare), null,
    'a needs block that asks for nothing always runs');

t.is(resolveNeeds({ login: true }, dev), null, 'needs.login runs when an account is configured');
t.is(resolveNeeds({ login: true }, bare), bare.accountReason,
    'needs.login skips with the reason when there is no account');

t.is(resolveNeeds({ hgsql: true }, dev), null, 'needs.hgsql runs where hgsql is available');
t.is(resolveNeeds({ hgsql: true }, bare), bare.hgsqlReason,
    'needs.hgsql skips off a machine with hgsql');

t.is(resolveNeeds({ write: true }, dev), null, 'needs.write runs where can.write is set');
t.ok(/can\.write is not set/.test(resolveNeeds({ write: true }, { ...dev, canWrite: false })),
    'needs.write skips where can.write is off');

const e = t.throws(() => resolveNeeds({ write: true }, rr), /production/,
    'needs.write against rr is refused outright');
t.is(e && e.exitCode, EXIT.CONFIG, 'and it exits 2, a configuration error');

// The refusal comes first, so a machine that also has can.write off still gets
// the loud answer rather than a quiet skip.
const e2 = t.throws(() => resolveNeeds({ write: true }, { ...rr, canWrite: false }), /production/,
    'the rr refusal beats every skip');
t.is(e2 && e2.exitCode, EXIT.CONFIG, 'and it is still exit 2');

t.done('t02-needs');
