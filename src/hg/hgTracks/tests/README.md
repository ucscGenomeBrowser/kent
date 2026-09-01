# hgTracks UI tests

Browser tests for hgTracks. They drive a real Chromium against a real server, so
they need a network and they are **not** part of the tree-wide `make test`.

    make uiTest                      # run them against genome-test
    make uiTest TARGET=hgwdev-$USER  # against your own sandbox
    make headed                      # watch the browser do it
    make uiTest G='hide all'         # one check
    make selfcheck                   # is the setup sound?

The harness is `src/hg/utils/uiTest/`. Read its `README.md` for setup and
`WRITING-TESTS.md` before adding a check.

## What is here

`t01-render.js` -- ten checks, none of which needs an account or the database, so
this runs for anyone with no conf file at all.

| # | Check | What it catches |
|---|---|---|
| 1 | hgTracks draws an image table | the CGI-500 and blank-page class. The cheapest check here, and the one that fails first when something is badly wrong |
| 2 | the default track set is drawn | a narrow list on purpose: `ruler` and `knownGene`. A broad one would fail every time the defaults are tuned, which trains people to ignore it |
| 3 | the image height is bounded | a track that suddenly draws 8000px |
| 4 | a malformed position produces the error banner | proves the always-on error detector is alive. Every navigation asserts the page is not an error page; this check asserts that assertion still works |
| 5 | an over-long request is reported, not photographed | the other branch of the detector. Apache answers 414 and a 414 renders as a perfectly good page |
| 6 | turning a track to pack changes its row height | the cart accepted the visibility and hgTracks did not apply it |
| 7 | hide all leaves only the ruler | a track leaking on by default |
| 8 | hiding one subtrack leaves its sibling alone | the two-request composite split, #37953 |
| 9 | the position box round-trips a formatted range | reads `hgTracks.chromName` from the page's own JavaScript, not the position box, which can be stale after an interactive zoom |
| 10 | an item raises its own tooltip | the page object does the show/hide timing, so a neighbour's leftover tooltip is never mistaken for this one |

Checks 4 and 7 are the hgTracks cases from `src/utils/qa/qaTestScript.py`,
rewritten with real assertions. That script has 285 driver calls and no
assertions at all -- it fails only when Selenium cannot find an element. These
two are a side-by-side for porting the rest at whatever pace suits.

`defaultView.docent.yaml` -- the same view asserted in Docent, so `make uiTest`
shows both engines running and merging into one `results.json`.

`pages/hgTracks.js` -- every selector, in one place. A check that needs a raw
selector means this file is missing a reader.

## Which engine

> **"Go somewhere, turn tracks on and off, assert what is drawn"** -- write a
> `.docent.yaml`.
>
> **Needs a login, files, the database, JavaScript internals, an HTTP status, a
> timing number, or a comparison between two page loads** -- write a `t*.js`.

`{rows: [...], exact: true, height: "<3000"}` is several assertions in one line
that someone who does not write JavaScript can read and change. Do not
reimplement that in a `t*.js`.

## Adding a check

Read `WRITING-TESTS.md` in the harness directory. In short: put the selector in
`pages/hgTracks.js`, put the assertion in a `t*.js`, and write the failure
message for whoever reads it later.
