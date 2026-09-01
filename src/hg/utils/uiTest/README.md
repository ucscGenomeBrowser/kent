# uiTest -- browser tests for the Genome Browser CGIs

One harness, so browser testing stops being three private efforts that share no
login, no target list and no way of being run.

    cd src/hg/hgTracks/tests
    make uiTest

That works with no setup and no conf file: it runs ten checks against
genome-test and tells you what it found. Everything below is for when you want
more than that.

## What this is

`uiTest` runs the tests in one CGI's `tests/` directory. It dispatches by
extension and merges both engines into one result:

| File | Engine |
|---|---|
| `t*.js` | the JavaScript runner in `lib/run.js` |
| `*.docent.yaml` | `docent.js`, run as a subprocess |

Docent (`src/hg/utils/docent/`, RM #37892) is not modified by any of this. The
integration is its exit code, which is why the two can share a directory without
either one having to know about the other.

Which to write:

> **"Go somewhere, turn tracks on and off, assert what is drawn"** -- write a
> `.docent.yaml`.
>
> **Needs a login, files, the database, JavaScript internals, an HTTP status, a
> timing number, or a comparison between two page loads** -- write a `t*.js`.

## Running

    make uiTest                       # everything in this directory
    make uiTest T=t01                 # one file
    make uiTest G='hide all'          # one check
    make uiTest TARGET=hgwdev-$USER   # against your sandbox
    make headed                       # show the browser, slowly
    make selfcheck                    # check the setup, run nothing
    make lint                         # the waitForTimeout policy

`uiTest --help` lists every flag. `make selfcheck` first when something looks
wrong: it separates "the setup is broken" from "the feature is broken", which is
the distinction the exit codes exist to preserve.

## Exit codes

| Code | Meaning |
|---|---|
| 0 | everything that ran passed (skips allowed) |
| 1 | a check failed -- a bug in the thing under test |
| 2 | usage or configuration error (bad conf permissions, unknown target, a write aimed at production) |
| 3 | infrastructure (no browser, server unreachable, login broken) |
| 4 | everything was skipped and `--strict` was given |

**Skipped never folds into passed.** The summary reads `10 passed, 0 failed, 2
skipped`, and a suite that quietly skipped everything must not read as green. An
`*.xfail.docent.yaml` that passes is a failure, the same rule docent applies.

## The conf file

`~/.hg.uiTest.conf`, mode 600. **Optional** -- with no conf file at all, uiTest
runs against genome-test and skips anything that needs a login or the database.
Copy `sample.hg.uiTest.conf` and edit it when you want your own sandbox or a
login.

Same format as `~/.hg.conf`: `name=value`, `#` comments, `include` and `delete`.
It is a separate file on purpose: `~/.hg.conf` is read by every CGI and by
`hgsql`, and test credentials do not belong in that blast radius. A file whose
name starts with `.` and that allows group or other access is rejected before
anything runs -- the same rule `checkConfigPerms()` in `hg/lib/hgConfig.c`
applies.

Precedence for every key: **CLI flag > environment > conf file > compiled
default.** Environment names are `UITEST_TARGET`, `UITEST_ACCOUNT`,
`UITEST_CONF`, `UITEST_ARTIFACTS`, `UITEST_HEADED` and `UITEST_PW_PREFIX`.

### Targets

`rr`, `genome-test`, `hgwdev`, `hgwbeta` and `hgwdev-<user>` are compiled in, so
they work with no conf file. A `target.<name>` line adds more -- the docker QA
instances, say. A full `http(s)` URL works as a target too. An unknown target is
a configuration error rather than a silently-tried hostname.

### Test accounts: one per person

Make yourself an hgLogin account named `<user>Qa` and name it in your own conf.
This is a safety property, not a preference. A run logs in as you, and a hub or
a session belongs to whoever's account made it, so a run of yours cannot reach a
hub or a session somebody else was in the middle of. A shared account would let
one person's run reach another's work.

A test that declares `needs.login` and finds no account is **skipped with a
reason**, never failed. A developer with no QA password must not see a red run.

## Playwright

One shared, pinned install at `/hive/groups/browser/uiTest/pw`. Braney owns it;
its `README.md` there carries the pin and the recipe for moving it. Docent points
at the same directory, so there is one Playwright on this machine and not one per
person.

`lib/pw.js` finds it, in this order:

1. `UITEST_PW_PREFIX`
2. `pw.prefix` in your conf
3. `/hive/groups/browser/uiTest/pw`
4. `$HOME/.uiTestPw`
5. a bare `require('playwright')`

**The /tmp trap.** `NODE_PATH` loses to any `node_modules` directory in an
ancestor of the working directory, and hgwdev has a stale
`/tmp/node_modules/playwright` owned by someone else. Anything run with
`NODE_PATH` set and a working directory under `/tmp` loads that instead and dies.
uiTest loads Playwright by absolute path rather than through `NODE_PATH`, and an
absolute-path require does not consult ancestor `node_modules` at all, so a run
works from any working directory whatever `NODE_PATH` says. Docent does use a
bare `require`, so when uiTest runs a docent script it sets `NODE_PATH` for that
subprocess and gives it an explicit working directory that is not `/tmp`.

`tests/t03-pw.js` pins this down: it moves its own working directory into a tree
carrying exactly that decoy and checks which copy gets loaded.

If the installed version does not match the pin, uiTest **warns and carries on**,
and records both in `results.json`. Version skew should be visible, not a wall.

## What a run leaves behind

    <artifacts>/<target>/<YYYYMMDD-HHMMSS>-<cgi>/
        results.json     machine readable, and the source of truth
        run.log
        shots/NN-<check>.png
        trace/<test>.zip
        <script>.docent.log

`results.json` is schema-versioned and carries the target, the account, the git
commit, the node and Playwright versions, the pin, totals by status, and for each
check its name, status, milliseconds, the authored failure message, **the URL
that produced it**, and the paths to its screenshot and trace.

A trace is written only for a test file that failed, so tracing is free on a
green run. When one does fail:

    npx playwright show-trace <artifacts>/.../trace/t01-render.zip

That replays the failed run with DOM snapshots, network and console. It is the
most useful thing in this directory and nobody finds it on their own.

There is no HTML report yet, on purpose. `results.json` is written from the
start, so a report is a pure add-on later that reads that file and can never
disagree with it -- and by then we will know from use what it should contain.

## Where things live

| | |
|---|---|
| `lib/env.js` | the conf file, targets, accounts, what this machine may do. No playwright, no CGI names, no network |
| `lib/pw.js` | finding Playwright |
| `lib/browser.js` | launching, contexts, tracing, screenshots. Could drive any website |
| `lib/site.js` | hgLogin, the login cache, CGI URLs, the always-on error assertion, reading the page's own JavaScript |
| `lib/wait.js` | the anti-flake helpers |
| `lib/run.js` | checks, skips, xfail, results |
| `lib/pages.js` | finds a CGI's page object |
| `uiTest.mk` | the make include a CGI's `tests/makefile` picks up in two lines |
| `tests/` | the harness's own tests. No browser, no network -- `make test` here really runs |

A CGI's tests live in `src/hg/<cgi>/tests/`, never here. `src/hg/hgTracks/tests/`
is the worked example; copy it.

Those `tests/` directories are **not** swept by the tree-wide `make test`.
`src/hg/makefile`'s `testAll` covers `APPS`, and the CGIs are in `BROWSER_BINS`,
so nothing reaches them today. `uiTest.mk` supplies an inert `test:` target
anyway, so the day a CGI joins `APPS`, `make test` stays green on a machine with
no network instead of trying to open a browser.

## Flake

`page.waitForTimeout` is banned in `lib/` and in `pages/`, and in a test it must
carry a `// flake:` comment naming what it waits for and why no condition exists.
`make lint` enforces that. Use `lib/wait.js` instead: `until`, `eventually`,
`noPending`, `navigated`, `stable`.

**The framework does not retry a check and does not retry a suite.** A flaky
check is a bug in the check or in the CGI, and an automatic retry hides both.
With a person at the terminal the right answer to a flake is `make headed` and a
look.
