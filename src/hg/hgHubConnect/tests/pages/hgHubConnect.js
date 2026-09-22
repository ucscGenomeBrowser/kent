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

// ---------------------------------------------------------------- verbs

async function openUploadTab(p, env) {
    // hgHubConnect.js picks the tab from the URL hash on load, and hubCreate's
    // init() asks for the file list only when Hub Upload is the active tab.
    await site.goto(p, site.url(env, 'hgHubConnect') + '#hubUpload');
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

module.exports = {
    openUploadTab, openDashboard, addFile, waitForFileCard, saveFileCard,
    hasUploadTab, files, fileNamed, metaOf, cardGenome, cardHubName,
};
