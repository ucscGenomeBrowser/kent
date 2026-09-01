// What a run writes down.
//
// A results.json often ends up under public_html, so two things must never
// reach it: an hgsid, which is a live session anybody reading the file could
// then load, and a request line long enough to bury the rest of the file. A
// check is allowed to send a 9000-character URL on purpose.

'use strict';

const t = require('./assert');
const { safeUrl, slug } = require('../lib/run');

const base = 'https://genome-test.gi.ucsc.edu/cgi-bin/hgTracks';

t.is(safeUrl(''), '', 'no url reads as empty');
t.is(safeUrl(`${base}?db=hg38&position=chr7:1-100`),
    `${base}?db=hg38&position=chr7%3A1-100`,
    'an ordinary url survives, with its query re-encoded');

t.ok(!/hgsid/.test(safeUrl(`${base}?db=hg38&hgsid=493240588_ZE1HAVzBqVIoAZ0t&position=chr1`)),
    'the hgsid is stripped');
t.ok(/db=hg38/.test(safeUrl(`${base}?db=hg38&hgsid=493240588_ZE1HAVzBqVIoAZ0t&position=chr1`)),
    'and the rest of the query survives');
t.ok(!/hgsid=[^&]/.test(safeUrl(`not a url at all?db=hg38&hgsid=123_abc&x=1`)),
    'the hgsid is stripped even from something that will not parse as a URL');

const long = safeUrl(`${base}?padding=${'a'.repeat(9000)}`);
t.ok(long.length < 400, `a 9000-character url is cut down (it is now ${long.length})`);
t.ok(/characters\]$/.test(long), 'and says how long it really was');

t.is(slug('hide all leaves only the ruler'), 'hide-all-leaves-only-the-ruler',
    'a check name becomes a filename');
t.ok(slug('x'.repeat(200)).length <= 60, 'a very long check name is cut to fit a filename');

t.done('t04-results');
