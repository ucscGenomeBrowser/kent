// hubSpace: deleting one assembly's folder out of a hub of several assemblies.
// refs #38684
//
// The Hub Upload page refuses to delete an assembly hub's 2bit on its own, since
// that leaves the hub pointing at a missing file. It must still let the user
// delete a folder holding the 2bit. In a hub with a genomes.txt each assembly
// has its own folder, and deleting one of them is how a user drops an assembly.
// deleteFileList in hg/js/hgMyData.js makes the call.
//
//   the 2bit alone, selected inside its folder   refused, with an alert
//   the 2bit's folder, selected inside the hub   deleted, with its files
//   the other assembly and the hub's own files   left alone
//
// The hub this makes is deleted at the end unless a check failed, so a failure
// leaves it in the account to look at.

'use strict';

const fs = require('fs');
const path = require('path');
const site = require('../../utils/uiTest/lib/site');
const hub = require('./pages/hgHubConnect');

// where hgwdev's tusd hooks write hubSpace rows
const CENTRAL = 'hgcentraltest';
const INPUT = path.join(__dirname, 'input');
const EMAIL = 'genome-www@soe.ucsc.edu';

module.exports.needs = { login: true, hgsql: true, write: true };

function hubPaths(env, hubName) {
    // The path below the user's directory of every hubSpace row in the hub.
    const user = env.account.user.replace(/'/g, "''");
    const rows = site.hgsql(env, CENTRAL,
        'select location from hubSpace ' +
        `where userName='${user}' and (location like '%/${hubName}' ` +
        `or location like '%/${hubName}/%')`);
    return rows.map(r => r.location.slice(r.location.indexOf(`/${hubName}`) + 1));
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

module.exports.main = async function (p, t) {
    const env = t.env;
    const tag = 'del2b' + String(Date.now()).slice(-6);
    const work = path.join(t.dir, 'fixtures', tag);
    const asm = `${tag}Asm`;
    const asmA = `${tag.toLowerCase()}a`;
    const asmB = `${tag.toLowerCase()}b`;
    const dirA = `${asm}/${asmA}`;
    const twoBitA = `${dirA}/${asmA}.2bit`;

    const genomeStanza = g => [`genome ${g}`, `twoBitPath ${g}/${g}.2bit`,
        `trackDb ${g}/trackDb.txt`, `organism uiTest`, `scientificName uiTest ${g}`,
        `description uiTest ${g}`, 'defaultPos qaChr1:1-100', ''].join('\n');
    const count = writeTree(path.join(work, asm), {
        'hub.txt': [`hub ${asm}`, `shortLabel ${asm}`, `longLabel uiTest hub ${asm}`,
            'genomesFile genomes.txt', `email ${EMAIL}`, ''].join('\n'),
        'genomes.txt': [genomeStanza(asmA), genomeStanza(asmB)].join('\n'),
        [`${asmA}/${asmA}.2bit`]: { copy: 'qaAsm.2bit' },
        [`${asmA}/trackDb.txt`]: '# no tracks\n',
        [`${asmB}/${asmB}.2bit`]: { copy: 'qaAsm.2bit' },
        [`${asmB}/trackDb.txt`]: '# no tracks\n',
    });

    t.heading('a hub of two assemblies, a folder per assembly');
    const uploaded = await t.check('the hub uploads', async () => {
        await hub.openUploadTab(p, env);
        await hub.openDashboard(p);
        await hub.addFolder(p, path.join(work, asm), count);
        await hub.upload(p);
        const have = hubPaths(env, asm);
        return have.includes(twoBitA) ? null :
            `no row for ${twoBitA} after the upload. Rows: ${have.join(', ')}`;
    });
    if (!uploaded) {
        t.skipped('the 2bit alone is refused', 'the hub did not upload');
        t.skipped('the 2bit folder is deleted', 'the hub did not upload');
        return;
    }

    t.heading('the 2bit alone, selected inside its folder');
    await t.check('the 2bit alone is refused', async () => {
        // a new page, so the file list comes from the server
        await hub.openUploadTab(p, env);
        await hub.openDir(p, asm);
        await hub.openDir(p, dirA);
        await hub.selectRow(p, twoBitA);
        const alerts = await hub.deleteSelected(p, [twoBitA]);
        if (!alerts.some(m => m.includes('Cannot delete'))) {
            return `the page did not refuse. Alerts: ${JSON.stringify(alerts)}`;
        }
        return hubPaths(env, asm).includes(twoBitA) ? null :
            `the page refused, but the row for ${twoBitA} is gone`;
    });

    t.heading('the 2bit folder, selected inside the hub');
    await t.check('the 2bit folder is deleted', async () => {
        await hub.openUploadTab(p, env);
        await hub.openDir(p, asm);
        await hub.selectRow(p, dirA);
        const alerts = await hub.deleteSelected(p, [dirA, twoBitA]);
        if (alerts.length) {
            return `the page refused: ${alerts.join(' | ')}`;
        }
        const left = hubPaths(env, asm).filter(x => x === dirA || x.startsWith(dirA + '/'));
        return left.length ? `rows left in ${dirA}: ${left.join(', ')}` : null;
    });

    await t.check('the other assembly and the hub files are left', async () => {
        const have = hubPaths(env, asm);
        const want = [asm, `${asm}/hub.txt`, `${asm}/genomes.txt`, `${asm}/${asmB}`,
            `${asm}/${asmB}/${asmB}.2bit`, `${asm}/${asmB}/trackDb.txt`];
        const missing = want.filter(x => !have.includes(x));
        return missing.length ? `missing after the delete: ${missing.join(', ')}` : null;
    });

    // --------------------------------------------------------------- cleanup
    if (t.failedHere) {
        t.log(`\nleft in the account to look at: ${asm}`);
        return;
    }
    await hub.openUploadTab(p, env);
    try {
        await hub.deleteHub(p, asm);
    } catch (e) {
        t.log(`could not delete ${asm}: ${e.message.split('\n')[0]}`);
    }
};
