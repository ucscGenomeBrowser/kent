// hubSpace: which genome a hub's directory rows carry, read from hgcentraltest
// after each upload and after each file of a batch. refs #38588
//
// The hub's own directory row carries the genome of the hub's hub.txt row: the
// one genome a hub.txt names, or blank for a hub.txt with a genomesFile, which
// names several. A subdirectory row gets the genome of a file uploaded directly
// into it. The page reads the hub row when a later upload names the hub: a
// wrong genome there refuses files for the hub's other genomes, and for an
// assembly hub silently rewrites a new file's genome to the wrong one.
//
// makeParentDirRows in hg/lib/userdata.c makes the directory rows, and
// syncHubRowDbToHubTxt sets the hub row after each upload. These cases watch
// that:
//
//   one genome, file at the hub root           root gets the genome
//   one genome, file in sub/, then one at root root gets the genome from the
//                                              first upload's hub.txt, and keeps it
//   one genome, own hub.txt plus sub/test.bb   root gets the genome whatever order
//                                              the two land in (the t05 race)
//   two genomes, own hub.txt, per-genome dirs  root stays blank
//   two genomes, own hub.txt, all files at root  root stays blank
//   a later file into the per-genome hub root  accepted, and root stays blank
//   two assemblies, own hub.txt                root stays blank, every row assemblyHub
//
// and what happens when a hub.txt is uploaded over the hub's own:
//
//   same genome, hub.txt edited                accepted, hub.txt row has the new
//                                              md5, root keeps its genome
//   one genome becomes two                     accepted, root and hub.txt row blank
//   two genomes become one                     accepted, root and hub.txt row hg38
//
// The uploads go through the shared tusd hooks in /usr/local/apache/cgi-bin/hooks,
// whatever the target. The target only supplies the page and its JavaScript.
// The hubs this makes are deleted at the end unless a check failed, so a failure
// leaves them in the account to look at.

'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const site = require('../../utils/uiTest/lib/site');
const hub = require('./pages/hgHubConnect');

// where hgwdev's tusd hooks write hubSpace rows
const CENTRAL = 'hgcentraltest';
const INPUT = path.join(__dirname, 'input');
const EMAIL = 'genome-www@soe.ucsc.edu';

module.exports.needs = { login: true, hgsql: true, write: true };

function hubRows(env, hubName) {
    // Every hubSpace row of this account at or under the hub's own directory.
    const user = env.account.user.replace(/'/g, "''");
    return site.hgsql(env, CENTRAL,
        'select fileName, parentDir, fileType, db, hubType, md5sum, location from hubSpace ' +
        `where userName='${user}' and (location like '%/${hubName}' ` +
        `or location like '%/${hubName}/%')`);
}

function rootRow(rows, hubName) {
    return rows.find(r => r.fileType === 'dir' && r.parentDir === '' &&
        r.fileName === hubName) || null;
}

function shortPath(r, hubName) {
    // the location from the hub's own directory down
    const at = r.location.indexOf(`/${hubName}`);
    return at < 0 ? r.location : r.location.slice(at + 1);
}

function showRows(rows, hubName) {
    return rows.map(r => `${shortPath(r, hubName)} db="${r.db}" ${r.hubType}`).join('; ');
}

function writeTree(dir, files) {
    // files maps a path under dir to its text, or to {copy: name in input/}
    fs.rmSync(dir, { recursive: true, force: true });
    for (const [rel, what] of Object.entries(files)) {
        const dest = path.join(dir, rel);
        fs.mkdirSync(path.dirname(dest), { recursive: true });
        if (typeof what === 'string') {
            fs.writeFileSync(dest, what);
        } else {
            fs.copyFileSync(path.join(INPUT, what.copy), dest);
        }
    }
    return Object.keys(files).length;
}

function splitHubTxt(name) {
    return [`hub ${name}`, `shortLabel ${name}`, `longLabel uiTest hub ${name}`,
        'genomesFile genomes.txt', `email ${EMAIL}`, ''].join('\n');
}

function bigBedTrackDb(track, file) {
    return [`track ${track}`, `shortLabel ${track}`, `longLabel uiTest ${track}`,
        'type bigBed 6', `bigDataUrl ${file}`, ''].join('\n');
}

module.exports.main = async function (p, t) {
    const env = t.env;
    const tag = 'dirDb' + String(Date.now()).slice(-6);
    const work = path.join(t.dir, 'fixtures', tag);
    const made = [];

    async function freshDashboard() {
        // uppy takes one batch per page, so every upload starts from a new page
        await hub.openUploadTab(p, env);
        await hub.openDashboard(p);
    }

    async function uploadWatchingRoot(hubName, oneAtATime) {
        // Upload the batch in the dashboard, and after each file lands read the
        // hub's root row. With oneAtATime the next file waits for that reading, so
        // each one shows the table right after that file. Without it the files run
        // side by side, as they do for a user, and a reading can already include
        // a file that finished just after the one named.
        const seen = [];
        await hub.upload(p, {
            oneAtATime: oneAtATime,
            onFileDone: (dir, name) => {
                const root = rootRow(hubRows(env, hubName), hubName);
                seen.push(`${dir}/${name} -> root db ` +
                    (root ? `"${root.db}"` : '(no row)'));
            },
        });
        return seen;
    }

    async function uploadOne(file, genome, hubPath) {
        await freshDashboard();
        await hub.addDiskFiles(p, [path.join(INPUT, file)]);
        await hub.editFile(p, file, { genome: genome, hub: hubPath });
        await hub.upload(p);
    }

    function rootIs(hubName, want, seen) {
        const rows = hubRows(env, hubName);
        const root = rootRow(rows, hubName);
        if (!root) {
            return `no hubSpace row for the hub directory ${hubName}. Rows: ` +
                showRows(rows, hubName);
        }
        if (root.db === want) {
            return null;
        }
        let msg = `the ${hubName} row has db "${root.db}", expected "${want}". ` +
            `Rows: ${showRows(rows, hubName)}`;
        if (seen) {
            msg += `. As the files landed: ${seen.join(', ')}`;
        }
        return msg;
    }

    async function splitBatchReady(hubName, genomes) {
        // The page must have taken the batch as a hub with several genomes before
        // its upload says anything about one.
        const meta = (await hub.files(p)).map(f => ({ name: f.name, m: f.meta }));
        const notSplit = meta.filter(f => f.m.batchSplitHub !== 'true');
        if (notSplit.length) {
            return `the page did not take the batch as a split hub: ` +
                notSplit.map(f => f.name).join(', ');
        }
        const got = [...new Set(meta.map(f => f.m.genome).filter(Boolean))].sort();
        if (got.join(',') !== genomes.slice().sort().join(',')) {
            return `the batch carries genomes ${JSON.stringify(got)}, expected ` +
                JSON.stringify(genomes);
        }
        const away = meta.filter(f => !(f.m.parentDir === hubName ||
            (f.m.parentDir || '').startsWith(hubName + '/')));
        return away.length ? `files headed outside ${hubName}: ` +
            away.map(f => `${f.name} -> ${f.m.parentDir}`).join(', ') : null;
    }

    // ------------------------------------------------------------------ one genome
    t.heading('one genome, a file at the hub root');
    const oneRoot = `${tag}One`;
    made.push(oneRoot);
    await uploadOne('test.bb', 'hg38', oneRoot);

    await t.check('the hub row takes the file genome', async () =>
        rootIs(oneRoot, 'hg38'));

    await t.check('the page lists the hub with the genome the table holds', async () => {
        await hub.openUploadTab(p, env);
        const listed = await hub.listedRow(p, oneRoot);
        if (!listed) {
            return `the page has no row for ${oneRoot}`;
        }
        return listed.genome === 'hg38' ? null :
            `the page lists ${oneRoot} as genome "${listed.genome}", the table says hg38`;
    });

    t.heading('one genome, a file in a subdirectory, then one at the hub root');
    const oneSub = `${tag}Sub`;
    made.push(oneSub);
    await uploadOne('test.bb', 'hg38', `${oneSub}/sub`);

    await t.check('a file in sub/ gives the hub row and sub/ the genome', async () => {
        // the hub row from the hub.txt written for this upload, sub/ from the file
        const rows = hubRows(env, oneSub);
        const root = rootRow(rows, oneSub);
        const sub = rows.find(r => r.fileType === 'dir' &&
            shortPath(r, oneSub) === `${oneSub}/sub`);
        if (!root || !sub) {
            return `missing a directory row. Rows: ${showRows(rows, oneSub)}`;
        }
        if (root.db !== 'hg38' || sub.db !== 'hg38') {
            return `hub row db "${root.db}" and sub row db "${sub.db}", expected ` +
                `"hg38" for both. Rows: ${showRows(rows, oneSub)}`;
        }
        return null;
    });

    await uploadOne('test.bb', 'hg38', oneSub);

    await t.check('a later file at the hub root gives the hub row its genome', async () =>
        rootIs(oneSub, 'hg38'));

    t.heading('one genome, the batch brings its own hub.txt and a file in sub/');
    const oneOwn = `${tag}Own`;
    made.push(oneOwn);
    let count = writeTree(path.join(work, oneOwn), {
        'hub.txt': [`hub ${oneOwn}`, `shortLabel ${oneOwn}`, `longLabel uiTest ${oneOwn}`,
            'useOneFile on', `email ${EMAIL}`, '', 'genome hg38', '',
            bigBedTrackDb('uiTestHg38', 'sub/test.bb')].join('\n'),
        'sub/test.bb': { copy: 'test.bb' },
    });
    await freshDashboard();
    await hub.addFolder(p, path.join(work, oneOwn), count);
    // side by side, since which file lands first is the point
    let seen = await uploadWatchingRoot(oneOwn, false);

    await t.check('the hub row ends with the hub.txt genome, whichever file lands first',
        async () => rootIs(oneOwn, 'hg38', seen));

    // ----------------------------------------------------------- several genomes
    t.heading('two genomes, own hub.txt, a directory per genome');
    const nested = `${tag}Nest`;
    made.push(nested);
    count = writeTree(path.join(work, nested), {
        'hub.txt': splitHubTxt(nested),
        'genomes.txt': ['genome hg38', 'trackDb hg38/trackDb.txt', '',
            'genome mm39', 'trackDb mm39/trackDb.txt', ''].join('\n'),
        'hg38/trackDb.txt': bigBedTrackDb('uiTestHg38', 'test.bb'),
        'hg38/test.bb': { copy: 'test.bb' },
        'mm39/trackDb.txt': bigBedTrackDb('uiTestMm39', 'mouse.bb'),
        'mm39/mouse.bb': { copy: 'mouse.bb' },
    });
    await freshDashboard();
    await hub.addFolder(p, path.join(work, nested), count);

    const nestedReady = await t.check('the page takes the batch as a hub of two genomes',
        async () => splitBatchReady(nested, ['hg38', 'mm39']));
    if (nestedReady) {
        seen = await uploadWatchingRoot(nested, true);

        await t.check('the hub row stays blank', async () => rootIs(nested, '', seen));

        await t.check('each genome directory row has its own genome', async () => {
            const rows = hubRows(env, nested);
            const bad = ['hg38', 'mm39'].filter(g => !rows.some(r => r.fileType === 'dir' &&
                shortPath(r, nested) === `${nested}/${g}` && r.db === g));
            return bad.length ? `no ${bad.join(', ')} directory row carrying its genome. ` +
                `Rows: ${showRows(rows, nested)}` : null;
        });

        t.heading('a later hg38 file uploaded straight into that hub');
        await t.check('the page accepts it', async () => {
            await uploadOne('test.bb', 'hg38', nested);
            return null;
        });

        await t.check('the hub row is still blank afterwards', async () =>
            rootIs(nested, ''));
    } else {
        t.skipped('the hub row stays blank', 'the batch was not a split hub');
    }

    t.heading('two genomes, own hub.txt, every file at the hub root');
    const flat = `${tag}Flat`;
    made.push(flat);
    count = writeTree(path.join(work, flat), {
        'hub.txt': splitHubTxt(flat),
        'genomes.txt': ['genome hg38', 'trackDb hg38.trackDb.txt', '',
            'genome mm39', 'trackDb mm39.trackDb.txt', ''].join('\n'),
        'hg38.trackDb.txt': bigBedTrackDb('uiTestHg38', 'test.bb'),
        'test.bb': { copy: 'test.bb' },
        'mm39.trackDb.txt': bigBedTrackDb('uiTestMm39', 'mouse.bb'),
        'mouse.bb': { copy: 'mouse.bb' },
    });
    await freshDashboard();
    await hub.addFolder(p, path.join(work, flat), count);

    const flatReady = await t.check('the page takes the flat batch as a hub of two genomes',
        async () => splitBatchReady(flat, ['hg38', 'mm39']));
    if (flatReady) {
        seen = await uploadWatchingRoot(flat, true);
        await t.check('the flat hub row stays blank', async () => rootIs(flat, '', seen));
    } else {
        t.skipped('the flat hub row stays blank', 'the batch was not a split hub');
    }

    t.heading('two assemblies, own hub.txt, a directory per assembly');
    const asm = `${tag}Asm`;
    made.push(asm);
    const asmA = `${tag.toLowerCase()}a`;
    const asmB = `${tag.toLowerCase()}b`;
    const genomeStanza = g => [`genome ${g}`, `twoBitPath ${g}/${g}.2bit`,
        `trackDb ${g}/trackDb.txt`, `organism uiTest`, `scientificName uiTest ${g}`,
        `description uiTest ${g}`, 'defaultPos qaChr1:1-100', ''].join('\n');
    count = writeTree(path.join(work, asm), {
        'hub.txt': splitHubTxt(asm),
        'genomes.txt': [genomeStanza(asmA), genomeStanza(asmB)].join('\n'),
        [`${asmA}/${asmA}.2bit`]: { copy: 'qaAsm.2bit' },
        [`${asmA}/trackDb.txt`]: '# no tracks\n',
        [`${asmB}/${asmB}.2bit`]: { copy: 'qaAsm.2bit' },
        [`${asmB}/trackDb.txt`]: '# no tracks\n',
    });
    await freshDashboard();
    await hub.addFolder(p, path.join(work, asm), count);

    const asmReady = await t.check('the page takes the batch as a hub of two assemblies',
        async () => splitBatchReady(asm, [asmA, asmB]));
    if (asmReady) {
        seen = await uploadWatchingRoot(asm, true);

        await t.check('the assembly hub row stays blank', async () =>
            rootIs(asm, '', seen));

        await t.check('every row of the assembly hub is assemblyHub', async () => {
            const rows = hubRows(env, asm);
            const off = rows.filter(r => r.hubType !== 'assemblyHub');
            return off.length ? `not assemblyHub: ${showRows(off, asm)}` : null;
        });
    } else {
        t.skipped('the assembly hub row stays blank', 'the batch was not a split hub');
    }

    // ------------------------------------------------------ hub.txt re-uploads
    // Uploading a hub.txt over the hub's own rewrites the hub.txt row. The hub
    // row must follow it: the genome when the new hub.txt names one, blank when
    // it lists several in a genomes.txt.

    function rowsState(hubName) {
        const rows = hubRows(env, hubName);
        const root = rootRow(rows, hubName);
        const txt = rows.find(r => r.fileType === 'hub.txt' &&
            shortPath(r, hubName) === `${hubName}/hub.txt`);
        return {
            rows: rows,
            root: root ? root.db : null,
            hubTxt: txt ? txt.db : null,
            md5: txt ? txt.md5sum : null,
            text: `hub row db "${root ? root.db : '(none)'}", hub.txt row db ` +
                `"${txt ? txt.db : '(none)'}"`,
        };
    }

    function oneGenomeHubTxt(name, label, bigDataUrl) {
        return [`hub ${name}`, `shortLabel ${name}`, `longLabel ${label}`,
            'useOneFile on', `email ${EMAIL}`, '', 'genome hg38', '',
            bigBedTrackDb('uiTestHg38', bigDataUrl)].join('\n');
    }

    async function startOneGenomeHub(name) {
        const n = writeTree(path.join(work, name), {
            'hub.txt': oneGenomeHubTxt(name, `uiTest ${name}`, 'test.bb'),
            'test.bb': { copy: 'test.bb' },
        });
        await freshDashboard();
        await hub.addFolder(p, path.join(work, name), n);
        await hub.upload(p);
    }

    async function uploadHubTxtAlone(name, text) {
        // just the hub.txt, picked as a single file the way a user replaces one
        const dir = path.join(work, `${name}.again`);
        writeTree(dir, { 'hub.txt': text });
        await freshDashboard();
        await hub.addDiskFiles(p, [path.join(dir, 'hub.txt')]);
        await hub.upload(p, { overwrite: true });
    }

    t.heading('re-upload: the same genome, the hub.txt edited');
    const reSame = `${tag}ReSame`;
    made.push(reSame);
    await startOneGenomeHub(reSame);
    const reSameText = oneGenomeHubTxt(reSame, 'uiTest hub.txt edited', 'test.bb');
    let before = rowsState(reSame);

    await t.check('the edited hub.txt is accepted', async () => {
        await uploadHubTxtAlone(reSame, reSameText);
        return null;
    });

    await t.check('the hub.txt row holds the new file', async () => {
        const after = rowsState(reSame);
        const want = crypto.createHash('md5').update(reSameText).digest('hex');
        return after.md5 === want ? null :
            `the hub.txt row md5 is ${after.md5}, the new hub.txt is ${want}`;
    });

    await t.check('the hub row keeps its genome', async () => {
        const after = rowsState(reSame);
        return after.root === 'hg38' ? null :
            `before: ${before.text}. after: ${after.text}`;
    });

    t.heading('re-upload: one genome becomes two');
    const reSplit = `${tag}ReSplit`;
    made.push(reSplit);
    await startOneGenomeHub(reSplit);
    // test.bb is already in the hub, so the new trackDb can point at it
    count = writeTree(path.join(work, `${reSplit}.split`, reSplit), {
        'hub.txt': splitHubTxt(reSplit),
        'genomes.txt': ['genome hg38', 'trackDb hg38.trackDb.txt', '',
            'genome mm39', 'trackDb mm39.trackDb.txt', ''].join('\n'),
        'hg38.trackDb.txt': bigBedTrackDb('uiTestHg38', 'test.bb'),
        'mm39.trackDb.txt': bigBedTrackDb('uiTestMm39', 'mouse.bb'),
        'mouse.bb': { copy: 'mouse.bb' },
    });
    before = rowsState(reSplit);

    const reSplitTaken = await t.check('a split hub.txt over a one-genome hub is accepted',
        async () => {
            await freshDashboard();
            await hub.addFolder(p, path.join(work, `${reSplit}.split`, reSplit), count);
            const notReady = await splitBatchReady(reSplit, ['hg38', 'mm39']);
            if (notReady) {
                return notReady;
            }
            await hub.upload(p, { overwrite: true });
            return null;
        });
    if (reSplitTaken) {
        await t.check('the hub row goes blank with the hub.txt row', async () => {
            const after = rowsState(reSplit);
            return after.root === '' && after.hubTxt === '' ? null :
                `before: ${before.text}. after: ${after.text}`;
        });
    } else {
        t.skipped('the hub row goes blank with the hub.txt row',
            'the split hub.txt was not uploaded');
    }

    t.heading('re-upload: two genomes become one');
    const reOne = `${tag}ReOne`;
    made.push(reOne);
    count = writeTree(path.join(work, reOne), {
        'hub.txt': splitHubTxt(reOne),
        'genomes.txt': ['genome hg38', 'trackDb hg38/trackDb.txt', '',
            'genome mm39', 'trackDb mm39/trackDb.txt', ''].join('\n'),
        'hg38/trackDb.txt': bigBedTrackDb('uiTestHg38', 'test.bb'),
        'hg38/test.bb': { copy: 'test.bb' },
        'mm39/trackDb.txt': bigBedTrackDb('uiTestMm39', 'mouse.bb'),
        'mm39/mouse.bb': { copy: 'mouse.bb' },
    });
    await freshDashboard();
    await hub.addFolder(p, path.join(work, reOne), count);
    await hub.upload(p);
    before = rowsState(reOne);

    const reOneTaken = await t.check('a one-genome hub.txt over a split hub is accepted',
        async () => {
            await uploadHubTxtAlone(reOne,
                oneGenomeHubTxt(reOne, `uiTest ${reOne} as one genome`, 'hg38/test.bb'));
            return null;
        });
    if (reOneTaken) {
        await t.check('the hub row takes the genome the new hub.txt names', async () => {
            const after = rowsState(reOne);
            return after.root === 'hg38' && after.hubTxt === 'hg38' ? null :
                `before: ${before.text}. after: ${after.text}`;
        });
    } else {
        t.skipped('the hub row takes the genome the new hub.txt names',
            'the one-genome hub.txt was not uploaded');
    }

    // --------------------------------------------------------------- cleanup
    if (t.failedHere) {
        t.log(`\nleft in the account to look at: ${made.join(', ')}`);
        return;
    }
    await hub.openUploadTab(p, env);
    for (const name of made) {
        try {
            await hub.deleteHub(p, name);
        } catch (e) {
            t.log(`could not delete ${name}: ${e.message.split('\n')[0]}`);
        }
    }
};
