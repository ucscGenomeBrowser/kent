// The hgHubConnect page object: the Hub Upload tab and the Uppy dashboard it
// opens.
//
// Verbs do a thing and wait for their own completion condition. Readers return
// data and never assert. There are two sources of truth on this page and they
// can disagree: what the dashboard draws, and what uppy holds in its own file
// list. Both have readers here, because the list is what gets sent.

'use strict';

const site = require('../../../utils/uiTest/lib/site');
const wait = require('../../../utils/uiTest/lib/wait');

// The modal panel. The .uppy-Dashboard wrapper around it is a zero-size div
// that exists whether the modal is open or not.
const DASHBOARD = '.uppy-Dashboard-inner';
const FILE_CARD = '.uppy-Dashboard-FileCard';
// The card's two buttons: a primary "Save changes" that writes the card back
// over file.meta, and a link-styled "Cancel" that does not.
const SAVE_CARD = '.uppy-Dashboard-FileCard .uppy-c-btn-primary';
const HUB_NAME_INPUT = '#uppy-Dashboard-FileCard-input-parentDir';
// The Dashboard renders two hidden file inputs. The one with webkitdirectory takes
// a folder and keeps each file's relative path, which the hub.txt flows need.
const FILE_INPUT = 'input.uppy-Dashboard-input:not([webkitdirectory])';
const DIR_INPUT = 'input.uppy-Dashboard-input[webkitdirectory]';
const UPLOAD_BUTTON = '.uppy-StatusBar-actionBtn--upload';
const FILES_TABLE = '#filesTable';

// The node side handler upload() calls as each file finishes, one per page.
const fileDoneHandlers = new WeakMap();

// ---------------------------------------------------------------- verbs

async function openUploadTab(p, env) {
    // hgHubConnect.js picks the tab from the URL hash on load, and hubCreate's
    // init() asks for the file list only when Hub Upload is the active tab.
    // Going to the URL the page is already on changes nothing, so reload instead:
    // the file list then comes from the server, and uppy starts a new batch.
    const target = site.url(env, 'hgHubConnect') + '#hubUpload';
    if (p.url() === target) {
        await wait.navigated(p, () => p.reload({ waitUntil: 'domcontentloaded' }),
            site.assertNoCgiError);
    } else {
        await site.goto(p, target);
    }
    await p.waitForSelector('#hubUpload');
    await wait.noPending(p);
}

async function openDashboard(p) {
    // The Upload button is a DataTables button, disabled until the file list
    // has come back, which only happens for a logged-in user.
    await wait.until(p, () => {
        const b = document.querySelector('.uploadButton');
        return !!b && !b.classList.contains('disabled');
    }, 'the Upload button never became clickable');
    await p.click('.uploadButton');
    await p.waitForSelector(DASHBOARD);
}

async function addFile(p, name, text) {
    // The Dashboard renders two hidden file inputs, one of them carrying
    // webkitdirectory for a folder drop. Pick the plain one.
    const handle = await p.evaluateHandle(() =>
        [...document.querySelectorAll('input.uppy-Dashboard-input')]
            .find(i => i.type === 'file' && !i.webkitdirectory));
    const input = handle.asElement();
    if (!input) {
        throw new Error('the Uppy dashboard has no file input to drop a file through');
    }
    await input.setInputFiles({
        name: name,
        mimeType: 'text/plain',
        buffer: Buffer.from(text),
    });
    const fail = await wait.eventually(async () => {
        const f = await fileNamed(p, name);
        return f ? null : `uppy never took ${name}`;
    }, `uppy never took ${name}`);
    if (fail) {
        throw new Error(fail);
    }
}

async function waitForUppyCount(p, n, what) {
    await wait.until(p, (want) => window.uppy && uppy.getFiles().length >= want,
        `uppy never held ${n} files after adding ${what}`, { arg: n });
}

async function waitForBatchParse(p) {
    // A batch with a hub.txt is read in the browser before its genomes are known,
    // and the Upload button is held off until that is done.
    await wait.until(p, () => typeof hubBatchParsesInFlight === 'undefined' ||
        hubBatchParsesInFlight === 0, 'the hub.txt in the batch was never parsed');
}

async function addDiskFiles(p, paths) {
    // Files from disk, picked flat, so each one lands with no relative path.
    const before = (await files(p)).length;
    await p.setInputFiles(FILE_INPUT, paths);
    await waitForUppyCount(p, before + paths.length, paths.join(', '));
    await waitForBatchParse(p);
}

async function addFolder(p, dir, count) {
    // A whole folder, as a user drops one. count is how many files it holds.
    const before = (await files(p)).length;
    await p.setInputFiles(DIR_INPUT, dir);
    await waitForUppyCount(p, before + count, dir);
    await waitForBatchParse(p);
}

async function openFileCard(p, name) {
    // Uppy opens a card by itself for a new file with no genome, so the card for
    // this file may already be up.
    const openFor = await cardFileName(p);
    if (openFor === name) {
        return;
    }
    if (openFor !== null) {
        await saveFileCard(p);
    }
    await p.click(`button[aria-label="Edit file ${name}"]`);
    await wait.until(p, (n) => {
        const dash = uppy.getPlugin('Dashboard');
        const id = dash && dash.getPluginState().fileCardFor;
        return !!id && uppy.getFile(id).name === n;
    }, `the file card for ${name} never opened`, { arg: name });
}

async function editFile(p, name, opts) {
    // Set a file's genome and hub name through its card, the way a user does.
    // genome must be one of the picker's own choices (hg38, hs1, hg19, mm39, mm10).
    const o = opts || {};
    await openFileCard(p, name);
    if (o.genome !== undefined) {
        await p.selectOption(`[id="${name}DbSelect"]`, o.genome);
    }
    if (o.hub !== undefined) {
        await p.fill(HUB_NAME_INPUT, o.hub);
    }
    await saveFileCard(p);
}

async function upload(p, opts) {
    // Press Upload and wait for the batch to finish. Throws, with the page's own
    // message, when the page refuses the batch or a file fails on the server.
    //
    // onFileDone, when given, is called in node as each file's upload succeeds,
    // with that file's hub path and name. By then the pre-finish hook has written
    // the file's hubSpace rows, so it can look at the table between files. With
    // oneAtATime as well, each file's first request waits until onFileDone has
    // returned for the file before it, so nothing else lands in between.
    //
    // overwrite says yes to the page asking whether to replace files already in
    // the hub. Without it a batch that would replace a file throws.
    const o = opts || {};
    const timeout = o.timeout || 180000;
    if (o.onFileDone) {
        // a binding lives as long as the page and can be exposed only once, so
        // expose one that calls whatever this upload asked for
        if (!fileDoneHandlers.has(p)) {
            await p.exposeFunction('__uiTestFileDone',
                (dir, name) => fileDoneHandlers.get(p)(dir, name));
        }
        fileDoneHandlers.set(p, o.onFileDone);
    }
    await p.evaluate(([report, oneAtATime]) => {
        const st = { started: false, done: null, errors: [] };
        window.__uiTestUpload = st;
        let gate = Promise.resolve();
        const release = {};
        const reported = new Set();
        if (oneAtATime) {
            // tus sends one POST per file to create it, and waits on a promise
            // returned from onBeforeRequest
            uppy.getPlugin('Tus').setOptions({
                onBeforeRequest: (req, file) => {
                    if (req.getMethod() !== 'POST') {
                        return undefined;
                    }
                    const before = gate;
                    gate = new Promise((r) => { release[file.id] = r; });
                    return before;
                },
            });
            uppy.on('upload-error', (f) => {
                if (release[f.id]) {
                    release[f.id]();
                }
            });
        }
        uppy.once('upload', () => { st.started = true; });
        uppy.once('complete', (r) => {
            st.done = { failed: r.failed.map(f => `${f.name}: ${f.error}`) };
        });
        uppy.on('info-visible', () => {
            const info = uppy.getState().info || [];
            const last = info[info.length - 1];
            if (last && last.type === 'error') {
                st.errors.push(last.message);
            }
        });
        if (report) {
            uppy.on('upload-success', (f) => {
                if (reported.has(f.id)) {
                    return;
                }
                reported.add(f.id);
                // uppy does not wait on its listeners, the gate above does
                window.__uiTestFileDone(f.meta.parentDir || '', f.name)
                    .finally(() => {
                        if (release[f.id]) {
                            release[f.id]();
                        }
                    });
            });
        }
    }, [!!o.onFileDone, !!o.oneAtATime]);
    const asked = [];
    const answer = (d) => {
        asked.push(d.message());
        return o.overwrite ? d.accept() : d.dismiss();
    };
    // the Upload button sits behind an open file card, as it does for a user
    if (await cardFileName(p) !== null) {
        await saveFileCard(p);
    }
    p.on('dialog', answer);
    try {
        // the question comes from onBeforeUpload, so it is answered by the time
        // the click returns, and a no there cancels the batch
        await p.click(UPLOAD_BUTTON);
        if (asked.length && !o.overwrite) {
            throw new Error(`the page asked "${asked.join(' | ')}" and the test said no`);
        }
        await wait.until(p, () => {
            const st = window.__uiTestUpload;
            return !!st.done || (!st.started && st.errors.length > 0);
        }, 'the upload never finished', { timeout });
    } finally {
        p.off('dialog', answer);
    }
    const st = await site.pageGlobal(p, 'window.__uiTestUpload');
    if (!st.done) {
        throw new Error(`the page refused the batch: ${st.errors.join(' | ')}`);
    }
    if (st.done.failed.length) {
        throw new Error(`files failed to upload: ${st.done.failed.join(' | ')}`);
    }
}

async function deleteHub(p, hubName) {
    // Select the hub's own row in the file table and delete it with everything in
    // it, as a user does from the top level of the table.
    await wait.until(p, (n) => window.hubCreate && n in hubCreate.uiState.filesHash,
        `there is no hub ${hubName} in the file table`, { arg: hubName });
    const clicked = await p.evaluate(([sel, n]) => {
        const table = jQuery(sel).DataTable();
        const idx = table.rows().indexes().toArray()
            .find(i => table.row(i).data().fullPath === n);
        if (idx === undefined) {
            return false;
        }
        const box = table.row(idx).node().querySelector('input.dt-select-checkbox');
        if (!box) {
            return false;
        }
        box.click();
        return true;
    }, [FILES_TABLE, hubName]);
    if (!clicked) {
        throw new Error(`hub ${hubName} has no row to select at the top of the table`);
    }
    const accept = d => d.accept();
    p.on('dialog', accept);
    try {
        await p.locator('button:visible:text-is("Delete selected")').first().click();
        await wait.until(p, (n) => !(n in hubCreate.uiState.filesHash),
            `hub ${hubName} was still listed after the delete`, { arg: hubName });
    } finally {
        p.off('dialog', accept);
    }
}

async function waitForFileCard(p) {
    await p.waitForSelector(FILE_CARD);
}

async function saveFileCard(p) {
    // "Save changes", which is what a user clicks to get back to the upload
    // list. It writes the card's own copy of the metadata back over file.meta.
    await p.waitForSelector(FILE_CARD);
    await p.click(SAVE_CARD);
    await p.waitForSelector(FILE_CARD, { state: 'detached' });
}

// ---------------------------------------------------------------- readers

async function hasUploadTab(p) {
    return p.evaluate(() =>
        !!document.querySelector('#tabs > ul > li > a[href="#hubUpload"]'));
}

async function files(p) {
    // uppy's own list, which is what will be sent.
    const out = await site.pageGlobal(p,
        'uppy.getFiles().map(f => ({name: f.name, meta: f.meta}))');
    return out || [];
}

async function fileNamed(p, name) {
    return (await files(p)).find(f => f.name === name) || null;
}

async function metaOf(p, name) {
    const f = await fileNamed(p, name);
    return f ? f.meta : null;
}

async function cardGenome(p) {
    // The card shows the genome as a locked text box when a hub.txt or a 2bit
    // decided it, and as the assembly picker otherwise.
    return p.evaluate(() => {
        const card = document.querySelector('.uppy-Dashboard-FileCard');
        if (!card) {
            return null;
        }
        const locked = card.querySelector('input[id$="AsmHubInput"]');
        if (locked) {
            return locked.value;
        }
        const picker = card.querySelector('select[id$="DbSelect"]');
        return picker ? picker.value : null;
    });
}

async function cardHubName(p) {
    return p.evaluate((sel) => {
        const el = document.querySelector(sel);
        return el ? el.value : null;
    }, HUB_NAME_INPUT);
}

async function cardFileName(p) {
    // The name of the file whose card is open, or null when no card is open.
    return p.evaluate(() => {
        const dash = window.uppy && uppy.getPlugin('Dashboard');
        const id = dash && dash.getPluginState().fileCardFor;
        return id ? uppy.getFile(id).name : null;
    });
}

async function listedRow(p, fullPath) {
    // The row the page holds for a path, from the file list the server sent at
    // page load. These are the values the page checks a new upload against.
    return p.evaluate((path) => {
        const r = window.hubCreate && hubCreate.uiState.filesHash[path];
        if (!r) {
            return null;
        }
        return {
            fileName: r.fileName, fileType: r.fileType, genome: r.genome,
            hubType: r.hubType, parentDir: r.parentDir, fullPath: r.fullPath,
        };
    }, fullPath);
}

module.exports = {
    openUploadTab, openDashboard, addFile, addDiskFiles, addFolder, editFile,
    openFileCard, upload, deleteHub, waitForFileCard, saveFileCard,
    hasUploadTab, files, fileNamed, metaOf, cardGenome, cardHubName, cardFileName,
    listedRow,
};
