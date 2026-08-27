// env.js -- everything uiTest knows before a browser exists: the conf file, the
// target servers, the test accounts, and what this machine is allowed to do.
//
// This is the bottom layer. It must never require playwright, name a CGI, or
// make a network call. Everything above it asks this module where to point and
// what it is permitted to do, and gets an answer without a browser running.
//
// Precedence for every key, one rule: CLI flag > environment > conf > compiled
// default. The compiled defaults are enough on their own, so uiTest works with
// no conf file at all against genome-test.

'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execSync } = require('child_process');

// Exit codes. One table, used at every layer, so "my password expired" never
// looks like "the feature broke".
const EXIT = {
    OK: 0,        // everything that ran passed (skips allowed)
    FAIL: 1,      // a check failed -- a bug in the thing under test
    CONFIG: 2,    // usage or configuration error
    INFRA: 3,     // no browser, server unreachable, login broken
    SKIPPED: 4,   // everything was skipped and --strict was given
};

function taggedError(msg, code) {
    const e = new Error(msg);
    e.exitCode = code;
    return e;
}

function configError(msg) {
    return taggedError(msg, EXIT.CONFIG);
}

function infraError(msg) {
    return taggedError(msg, EXIT.INFRA);
}

// The same servers docent names in its own SERVERS map. Duplicated on purpose:
// docent is not edited by this work, and making it require a file under uiTest
// would invert the dependency and leave it unrunnable if that file moved.
const SERVERS = {
    'rr': 'https://genome.ucsc.edu/cgi-bin',
    'genome-test': 'https://genome-test.gi.ucsc.edu/cgi-bin',
    'hgwdev': 'https://hgwdev.gi.ucsc.edu/cgi-bin',
    'hgwbeta': 'https://hgwbeta.soe.ucsc.edu/cgi-bin',
};

const DEFAULT_TARGET = 'genome-test';
const DEFAULT_TIMEOUT = 30000;
const CONF_FILE = path.join(os.homedir(), '.hg.uiTest.conf');

function expandHome(p) {
    if (!p) {
        return p;
    }
    if (p === '~') {
        return os.homedir();
    }
    if (p.startsWith('~/')) {
        return path.join(os.homedir(), p.slice(2));
    }
    return p;
}

function checkConfigPerms(file) {
    // Port of checkConfigPerms() in hg/lib/hgConfig.c. A config file whose name
    // starts with "." must not be readable by group or other, because it holds a
    // password. Same wording as the C, so someone who has seen it once knows it.
    if (path.basename(file)[0] !== '.') {
        return;
    }
    let st;
    try {
        st = fs.statSync(file);
    } catch (e) {
        return;   // does not exist, which is allowed
    }
    if ((st.mode & 0o077) !== 0) {
        throw configError(`config file ${file} allows group or other access, ` +
            `must only allow user access`);
    }
}

function parseConfFile(file, depth, into) {
    // Kent name=value format: "#" comments, blank lines, "include <path>" and
    // "delete <var> ...". Follows parseConfigFile() in hg/lib/hgConfig.c,
    // including its behaviour of ignoring a file that is not there.
    if (depth > 10) {
        throw configError(`maximum config include depth exceeded: ${file}`);
    }
    checkConfigPerms(file);
    let text;
    try {
        text = fs.readFileSync(file, 'utf8');
    } catch (e) {
        if (e.code === 'ENOENT') {
            return into;
        }
        throw configError(`cannot read ${file}: ${e.message}`);
    }
    into.files.push(file);
    text.split('\n').forEach((raw, i) => {
        const line = raw.trim();
        if (line === '' || line[0] === '#') {
            return;
        }
        if (/^include\s/.test(line)) {
            const rest = line.replace(/^include\s+/, '').trim();
            if (rest === '' || /\s/.test(rest)) {
                throw configError(
                    `invalid format for config include: ${file}:${i + 1}: ${line}`);
            }
            const inc = rest[0] === '/' ? rest : path.join(path.dirname(file), rest);
            parseConfFile(inc, depth + 1, into);
            return;
        }
        if (/^delete\s/.test(line)) {
            line.replace(/^delete\s+/, '').trim().split(/\s+/)
                .forEach(v => delete into.vals[v]);
            return;
        }
        const eq = line.indexOf('=');
        if (eq < 0) {
            throw configError(`invalid format in config file ${file}:${i + 1}: ${line}`);
        }
        into.vals[line.slice(0, eq).trim()] = line.slice(eq + 1).trim();
    });
    return into;
}

function readConf(file) {
    return parseConfFile(file, 0, { vals: {}, files: [] });
}

function asBoolean(v, dflt) {
    if (v == null) {
        return dflt;
    }
    return /^(yes|on|true|1)$/i.test(String(v).trim());
}

function resolveTimeout(v) {
    // Missing or unparseable falls back to DEFAULT_TIMEOUT. 0 is honoured as
    // "no timeout", the meaning Playwright gives setDefaultTimeout(0). A
    // negative number is never coherent, so it is rejected rather than
    // silently substituted.
    if (v == null || String(v).trim() === '') {
        return DEFAULT_TIMEOUT;
    }
    const n = Number(v);
    if (Number.isNaN(n)) {
        return DEFAULT_TIMEOUT;
    }
    if (n < 0) {
        throw configError(`timeout.default is "${v}", which is negative`);
    }
    return n;
}

function onPath(prog) {
    try {
        execSync(`command -v ${prog}`, { stdio: 'ignore' });
        return true;
    } catch (e) {
        return false;
    }
}

function resolveTarget(name, vals) {
    // A conf `target.<name>` line wins over the compiled table, so someone can
    // add the docker QA instances without a code change.
    const fromConf = vals[`target.${name}`];
    if (fromConf) {
        if (!/^https?:\/\//i.test(fromConf)) {
            throw configError(`target.${name} in your conf is "${fromConf}", which is not ` +
                `a http(s) URL`);
        }
        return fromConf.replace(/\/$/, '');
    }
    if (SERVERS[name]) {
        return SERVERS[name];
    }
    if (/^hgwdev-[a-z0-9._-]+$/i.test(name)) {
        return `https://${name}.gi.ucsc.edu/cgi-bin`;   // personal sandbox
    }
    if (/^https?:\/\//i.test(name)) {
        return name.replace(/\/$/, '');
    }
    throw configError(`unknown target "${name}". Known: ` +
        `${Object.keys(SERVERS).join(', ')}, hgwdev-<user>, a full http(s) URL, ` +
        `or a target.<name> line in your conf file.`);
}

function sameHost(base, rrBase) {
    // Compares hostnames rather than full URLs, so "rr" and every spelling of
    // genome.ucsc.edu/cgi-bin match. A base that new URL() cannot parse is
    // treated as production: failing open here would let credentials and
    // writes reach the real site.
    let host;
    try {
        host = new URL(base).hostname.toLowerCase().replace(/^www\./, '');
    } catch (e) {
        return true;
    }
    const rrHost = new URL(rrBase).hostname.toLowerCase().replace(/^www\./, '');
    return host === rrHost;
}

function load(opts) {
    const o = opts || {};
    const confFile = expandHome(o.conf || process.env.UITEST_CONF || CONF_FILE);
    const conf = readConf(confFile);
    const vals = conf.vals;

    const target = o.target || process.env.UITEST_TARGET ||
        vals['default.target'] || DEFAULT_TARGET;
    const base = resolveTarget(target, vals);
    const isRr = target === 'rr' || sameHost(base, SERVERS['rr']);

    // Which account, if any. "(none)" is how a conf says "never log in here".
    // rr defaults to no account whatever else is configured: credentials do not
    // go to production unless someone deliberately says so in their conf.
    let accountName = o.account || process.env.UITEST_ACCOUNT ||
        vals[`target.${target}.account`] ||
        (isRr ? '(none)' : vals['default.account']);
    let account = null;
    let accountReason = '';
    if (!accountName || accountName === '(none)') {
        accountReason = conf.files.length
            ? `no account configured for target ${target}`
            : `no conf file at ${confFile}`;
        accountName = null;
    } else {
        const user = vals[`account.${accountName}.user`];
        if (!user) {
            accountReason = `account "${accountName}" has no ` +
                `account.${accountName}.user line in ${confFile}`;
            accountName = null;
        } else {
            account = { name: accountName, user: user };
        }
    }

    const hgsqlWanted = asBoolean(vals['can.hgsql'], false);
    const hgsqlThere = hgsqlWanted && onPath('hgsql');
    let hgsqlReason = '';
    if (!hgsqlWanted) {
        hgsqlReason = `can.hgsql is not set in ${confFile}`;
    } else if (!hgsqlThere) {
        hgsqlReason = 'hgsql is not on PATH';
    }

    const env = {
        EXIT: EXIT,
        confFile: confFile,
        confFiles: conf.files,
        confPresent: conf.files.length > 0,
        target: target,
        base: base,
        isRr: isRr,
        account: account,
        accountReason: accountReason,
        canHgsql: hgsqlThere,
        hgsqlReason: hgsqlReason,
        canWrite: asBoolean(vals['can.write'], true),
        artifacts: expandHome(o.artifacts || process.env.UITEST_ARTIFACTS ||
            vals['artifacts'] || path.join(os.homedir(), 'uiTest')),
        pwPrefix: expandHome(o.pwPrefix || process.env.UITEST_PW_PREFIX ||
            vals['pw.prefix'] || null),
        timeout: resolveTimeout(vals['timeout.default']),
        headed: !!o.headed || asBoolean(process.env.UITEST_HEADED, false),
        slowMo: Number(o.slowMo) || 0,
        get: (key, dflt) => (vals[key] == null ? dflt : vals[key]),
    };

    // The password is resolved only when something actually logs in, so a run
    // that needs no account never shells out to a password manager.
    env.password = function () {
        if (!account) {
            return null;
        }
        const plain = vals[`account.${account.name}.password`];
        if (plain) {
            return plain;
        }
        const cmd = vals[`account.${account.name}.passwordCmd`];
        if (!cmd) {
            throw configError(`account "${account.name}" has neither ` +
                `account.${account.name}.password nor ` +
                `account.${account.name}.passwordCmd in ${confFile}`);
        }
        let out;
        try {
            out = execSync(cmd, { encoding: 'utf8' });
        } catch (e) {
            throw configError(`account.${account.name}.passwordCmd failed: ${cmd}`);
        }
        const pw = out.split('\n')[0].trim();
        if (!pw) {
            throw configError(`account.${account.name}.passwordCmd printed nothing: ${cmd}`);
        }
        return pw;
    };

    return env;
}

module.exports = {
    load, resolveTarget, readConf, checkConfigPerms, expandHome, asBoolean,
    configError, infraError,
    EXIT, SERVERS, CONF_FILE, DEFAULT_TARGET, DEFAULT_TIMEOUT,
};
