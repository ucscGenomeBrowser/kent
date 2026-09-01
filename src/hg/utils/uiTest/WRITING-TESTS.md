# Writing a uiTest

Read `README.md` first for what this is and how to run it. This is how to add a
check.

## The shape

A test file exports two things:

```js
'use strict';

const site = require('../../utils/uiTest/lib/site');
const hgTracks = require('./pages/hgTracks');

module.exports.needs = { login: false, hgsql: false, write: false };

module.exports.main = async function (p, t) {
    const env = t.env;

    await hgTracks.reset(p, env, { db: 'hg38', position: 'chr7:155799529-155812871' });

    await t.check('hide all leaves only the ruler', async () => {
        await hgTracks.hideAll(p);
        const extra = (await hgTracks.rows(p)).filter(r => r !== 'ruler');
        return extra.length ? `also drew: ${extra.join(', ')}` : null;
    });
};
```

`p` is a Playwright page. `t` is the runner: `t.check`, `t.xcheck`, `t.skipped`,
`t.heading`, `t.log`, and `t.env`.

That is the whole interface. A test file is a plain module, so a test can be run
straight from node with no environment set:

    node src/hg/hgTracks/tests/t01-render.js     # nothing happens: this is a module
    cd src/hg/hgTracks/tests && make uiTest T=t01

## A check returns a message, not a boolean

```js
return extra.length ? `also drew: ${extra.join(', ')}` : null;
```

Falsy means it held. A string means it did not, and that string is what somebody
reads when the run goes red. Write it for them: `also drew: clinvarCnv` says what
went wrong; `expected 0 to equal 1` does not. This is the one real advantage over
`assert`, so spend the extra ten seconds on the message.

A check that throws is a failure too, so a stale selector shows up as a failed
check rather than killing the run.

`t.xcheck` is for something known to be broken that you want pinned: it fails the
run if it ever starts passing, so nobody has to remember to come back.

## Declare what the test needs

```js
module.exports.needs = { login: true, hgsql: false, write: true };
```

Resolved **before any browser launches**:

| | |
|---|---|
| `login` | skipped, with a reason, when no account is configured for the target |
| `hgsql` | skipped when `can.hgsql` is unset or `hgsql` is not on PATH |
| `write` | skipped when `can.write` is off, and **refused outright** when the target is production |

A skip is never a failure. A developer with no QA password must not see a red
run over it.

`needs.write` aimed at `rr` is exit 2 and is not skippable. That is deliberate:
skipping it would mean a green run that quietly did nothing, which is how someone
eventually uploads a `qa123456` hub to genome.ucsc.edu.

A test with `needs.login` is handed a page that is already logged in. There is
nothing to call.

## Selectors go in the page object, never in the test

`src/hg/<cgi>/tests/pages/<cgi>.js` holds every selector for that CGI. It exports

- **verbs** -- do a thing, wait for its own completion condition, return nothing
- **readers** -- return data, never assert

If a check needs a raw selector, the page object is missing a reader. Add the
reader.

Keeping the assertions out of the page object is what makes it reusable, and it
is why a check reads like a sentence about the browser rather than a sentence
about the DOM.

To borrow another CGI's page object:

```js
const { page } = require('../../utils/uiTest/lib/pages');
const hgTables = page('hgTables');
```

## Never sleep

`page.waitForTimeout` is banned in `lib/` and `pages/`. In a test it needs a
`// flake:` comment naming what it waits for and why no condition exists.
`make lint` enforces it.

A sleep is a guess about how long something takes on a machine you are not
sitting at. Every helper in `lib/wait.js` waits for a condition instead:

| | |
|---|---|
| `until(page, fn, msg)` | `waitForFunction` whose failure message is `msg`, not `Timeout 30000ms exceeded`. The workhorse |
| `eventually(fn, msg)` | poll a check-shaped function until it holds. Replaces "sleep, then assert" |
| `noPending(page)` | jQuery has nothing in flight. The right default wait after a click |
| `navigated(page, fn, post)` | run `fn`, wait for the page to settle, then apply a post-condition |
| `stable(page, sel)` | it has stopped moving, not just started existing |

Almost every sleep you are tempted to write is really `until(p, () => ..., 'the
row never appeared')`.

## Navigation is already checked

`site.goto` asserts, after every navigation in every CGI's tests, that the page
is not a kent error page. It reads `#warnBox` and `#warnList` -- what
`htmlWarnBoxSetup()` in `src/lib/htmshell.c` actually produces -- plus Apache's
own 414 and 500 pages, a "Very Early Error" title, and a body that ends in
`-- ERROR --`.

Nobody writes that check. Everybody gets it. What a suite should carry instead is
a proof the detector still works: something that **must** produce the banner.
`t01-render.js` has two, one for each branch.

When a check wants the error rather than the absence of one, navigate with plain
`p.goto` and ask `site.errorOnPage(p)`.

## Assert on what will be sent, not only on what was drawn

```js
const at = await hgTracks.position(p);     // reads hgTracks.chromName
```

`site.pageGlobal(p, expr)` reads the page's own JavaScript. An interactive zoom
stores the new window in the cart, not in the position box, so the box can be
stale while the page knows exactly where it is. The same trick reaches
`window.uppy.getFiles()` on the hubSpace page, and anywhere else the truth lives
in a variable rather than in the DOM.

`site.fetchText(p, url)` fetches through the page's own request context, so the
session cookies come along and udc's cache of a `hub.txt` is bypassed.

## Setting up a new CGI

    mkdir -p src/hg/<cgi>/tests/pages

`src/hg/<cgi>/tests/makefile`, two lines:

```make
UITEST ?= $(CURDIR)/../../utils/uiTest/uiTest
include $(dir $(UITEST))uiTest.mk
```

The path is relative because a CGI's `tests/` directory always sits the same two
directories from the harness, so a git worktree needs no override.

That gives `make uiTest`, `make headed`, `make selfcheck`, `make lint`,
`make clean`, and an inert `test:` that keeps these out of the tree-wide
`make test`. Do not add the directory to `TEST_DIRS`, `APPS`, `UTIL_DIRS` or
`UTILS_APPLIST`.

Then write `pages/<cgi>.js` and `t01-<something>.js`, and copy the shape of
`src/hg/hgTracks/tests/` while you do it.

## Setting yourself up for a login

1. Make an hgLogin account named `<user>Qa` on the server you test against.
2. `cp sample.hg.uiTest.conf ~/.hg.uiTest.conf && chmod 600 ~/.hg.uiTest.conf`
3. Fill in `default.target`, `default.account` and the `account.qa.*` lines.
4. `make selfcheck` in a tests directory, then `uiTest --selfcheck --login` to do
   one real login round trip.

One account per person. A run logs in as you, so it cannot reach a hub or a
session that belongs to somebody else's account.
