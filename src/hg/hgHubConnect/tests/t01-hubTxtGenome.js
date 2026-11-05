// hubSpace: a hub.txt in the upload batch decides the genome, and nothing
// downstream may put the session default back.
//
// Nothing here is uploaded. The checks read uppy's own file list, which is the
// metadata the upload would carry, so the run leaves nothing on the server and
// needs no write permission.

'use strict';

const hub = require('./pages/hgHubConnect');

// A real UCSC assembly that is not the session default, which is what the bug
// needed: with hg38 on both sides there is nothing to tell apart.
const GENOME = 'mm10';
const HUB_NAME = 'uiTestHubTxtGenome';
const HUB_TXT = [
    `hub ${HUB_NAME}`,
    'shortLabel uiTest hub',
    'longLabel uiTest hub for the hub.txt genome checks',
    'useOneFile on',
    `genome ${GENOME}`,
    '',
    'track uiTestTrack',
    'shortLabel uiTest track',
    'longLabel uiTest track',
    'type bigWig',
    'bigDataUrl uiTest.bw',
    '',
].join('\n');

module.exports.needs = { login: true, hgsql: false, write: false };

module.exports.main = async function (p, t) {
    const env = t.env;

    await hub.openUploadTab(p, env);
    if (!await hub.hasUploadTab(p)) {
        t.skipped('hub.txt names the genome',
            `${env.base} has no Hub Upload tab, so storeUserFiles is off there`);
        return;
    }
    await hub.openDashboard(p);
    await hub.addFile(p, 'hub.txt', HUB_TXT);

    await t.check('uppy takes the genome from hub.txt', async () => {
        const meta = await hub.metaOf(p, 'hub.txt');
        if (!meta) {
            return 'hub.txt is not in uppy\'s file list at all';
        }
        return meta.genome === GENOME ? null
            : `uppy has genome "${meta.genome}", and hub.txt says "${GENOME}"`;
    });

    await t.check('the file card opens on the genome hub.txt named', async () => {
        // The card copies file.meta when it opens, so opening it before the
        // hub.txt has been read is what makes the save below wrong (#38367).
        await hub.waitForFileCard(p);
        const shown = await hub.cardGenome(p);
        return shown === GENOME ? null
            : `the card shows genome "${shown}", and hub.txt says "${GENOME}"`;
    });

    await t.check('saving the file card keeps the genome from hub.txt', async () => {
        await hub.saveFileCard(p);
        const meta = await hub.metaOf(p, 'hub.txt');
        if (!meta) {
            return 'hub.txt left uppy\'s file list when the card was saved';
        }
        return meta.genome === GENOME ? null
            : `the card wrote genome "${meta.genome}" back over "${GENOME}", so ` +
              `the upload would be rejected for the wrong assembly`;
    });

    await t.check('saving the file card keeps the hub name from hub.txt', async () => {
        const meta = await hub.metaOf(p, 'hub.txt');
        return meta && meta.parentDir === HUB_NAME ? null
            : `the file is headed for hub "${meta ? meta.parentDir : '(gone)'}", ` +
              `and hub.txt names "${HUB_NAME}"`;
    });
};
