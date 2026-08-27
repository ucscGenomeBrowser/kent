// pages.js -- find a CGI's page object.
//
// A test says page('hgTracks') and gets the selectors for hgTracks, wherever the
// kent tree happens to be checked out. Nobody writes ../../.. by hand, and a
// test in one CGI can borrow another CGI's page object when it has to cross
// between them.

'use strict';

const fs = require('fs');
const path = require('path');
const { configError } = require('./env');

// .../src/hg/utils/uiTest/lib -> .../src
const kentSrc = path.resolve(__dirname, '..', '..', '..', '..');

function pageFile(cgi) {
    return path.join(kentSrc, 'hg', cgi, 'tests', 'pages', `${cgi}.js`);
}

function page(cgi) {
    const f = pageFile(cgi);
    if (!fs.existsSync(f)) {
        throw configError(`no page object for ${cgi}: expected ${f}`);
    }
    return require(f);
}

module.exports = { page, pageFile, kentSrc };
