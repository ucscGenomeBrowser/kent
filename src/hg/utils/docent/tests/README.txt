Docent tests
------------

Run by hand, not by the kent tree's `make test`:

    make test               # every *.docent.yaml here
    make test T=composite   # just one
    make parity             # one script FAST and slow, and twice over
    make derive             # the derivation alone, against expected/ (no browser)
    make derive-accept      # rewrite those baselines, then read `git diff expected/`

    make test TARGET=hgwdev-demo9      # the same scripts, against another server

TARGET overrides the `target:` each script carries, for every target above, and takes
the same values it does: a shorthand (rr, genome-test, hgwdev, hgwbeta), a bare
hgwdev-<name> sandbox or demo, or a full .../cgi-bin URL. It is how you try a suite
against a branch build -- a sandbox, a ticket park from `ts`, a demo browser -- without
editing the scripts. `make preflight TARGET=...` checks that server rather than the one
the scripts name, so the fixture check and the run agree.

Read a redirected run's failures with the server in mind. A script asserts what its OWN
server draws, so a red one somewhere else can be the other machine's trackDb rather than
a bug: a demo sandbox that carries only one assembly fails every script on the others,
and a sandbox trackDb with a track the RR has not released changes what `exact: true`
counts. Redirecting is for trying a suite elsewhere, not for moving it: the committed
scripts stay pointed at the server they were written against, which is the one the
nightly reads.

`make preflight` prints how the target is configured, so the log says which server was
driven AND how it differs from the one the scripts name:

    target      https://hgwdev-braney.gi.ucsc.edu/cgi-bin
                /usr/local/apache/cgi-bin-braney/hg.conf
                central.db                   hgcentraltest
                db.trackDb                   trackDb_braney,trackDb
                curatedHubPrefix             braney
                browser.quickLift            on
                browser.quickLiftAlignments  on
                browser.recTrackSets         on

Those are read off the hg.conf the server reads, following its includes the way
hg/lib/hgConfig.c does, so the value printed is the EFFECTIVE one -- a sandbox conf that
sets nothing still shows what it inherits from the shared conf it includes. Only a fixed
list of settings is printed, because hg.conf includes hg.conf.private.

It works for a server on this machine: genome-test, hgwdev, an hgwdev-<name> sandbox or
demo, or a ticket park from `ts` on 127.0.0.1 (looked up by port in its registry). For
hgwbeta or the RR it says the conf cannot be read from here, which is true and is better
than a guess.

Even with that in the log, a config difference and a code difference can still look
alike. The reliable way to tell them apart is to swap only the BINARY: drop a control
build's CGIs into the same sandbox, leave its hg.conf alone, and re-run. If the failures
follow the binary they are the code.

Most tests drive a real browser against a real server, so they need the network and
the shared Playwright install (/hive/groups/browser/uiTest/pw; see ../README.md). That is why none of this is
part of the tree-wide test target: a broken network would fail the build.

A test is an ordinary Docent script that asserts with `expect:`. It passes by exiting
0. `expect:` is the only verb that CHECKS anything, so a test with no `expect:` step in
it tests nothing: `track:` accepts a name no assembly has and still exits 0.

Other verbs do fail a run, so do not read the line above as "nothing else can stop it".
A verb throws when it cannot do what it was told -- `mouseover:` cannot find the item,
`loadSession:` cannot find the file, `drag:`, `convert:` and `go:` likewise -- and
docent.js turns any step's throw into `step N (verb) failed` and exit 1. None of them
looks at whether the page came out right, which is the part only `expect:` does.

A script named *.xfail.docent.yaml is expected to FAIL, and the run fails if it passes.
That is how a trap gets pinned rather than merely written down.

`make derive` is the cheap half: DOCENT_DERIVE=1 resolves the `track:` steps against
trackDb and prints the cart variables without opening a browser, in about a second. It
is where Docent's own decisions live, and the baselines in expected/ are what catch a
change to visVars() or tdbHideTargets() that a rendered page would hide.

The trackDb listing is cached in $TMPDIR for a day (docent.js, TDB_TTL), and a cold
fetch prints one provenance line that a warm run does not. That line would make the
first `make derive` of the day differ from a baseline captured warm, for a reason that
has nothing to do with trackDb changing, so the makefile strips it from both the run and
the baseline. Everything else trackDb says about itself is kept, including the two lines
that report a hub genome or an unreachable hubApi.

What is covered
---------------

  selftest      session: -> expect: -> loadSession:, on hg38 at SHH. Saves the cart,
                changes the view, restores it from the local file, checks rows both times.
  heavysession  the same three steps as selftest, on a Recommended Track Set: 34 rows in,
                saved, moved away, loaded back, `exact: true` on both halves. selftest
                round-trips two rows, which barely reaches outIfNotPresent() in hgSession
                -- the function that writes a trackDb default for every track that is
                deliberately NOT in the cart, and the one a broken save-and-reload path
                shows up in. A path that dropped four rows left selftest green.
  firstrequest  a track turned on has to be drawn by the request that turned it on, with
                no `go:`, `open:` or `convert:` in between. The bug it exists for lags by
                exactly one request, so any script that navigates before asserting reads a
                correct image and passes. It names wgEncodeRegMarkH3k4me1 for the reason
                in its header: a top-level track or a default-visible child would pass on
                the broken build too.
  collection    hgCollection, which no other script here or in regress/ reaches. It shares
                visibility logic with hgTracks by COPY rather than by call:
                hg/hgCollection/hgCollection.c carries its own isParentVisible(), a
                verbatim copy of the one in hg/lib/trackHub.c, and it decides what goes in
                the builder's "Visible Tracks" folder. Needs a login, so it is the one
                script here that uses `login:`. Asserts on the folder's own jsTree class
                first -- open when checkForVisible() found something, leaf when it did not
                -- and then on the leaves inside it.
  search        hgSearch, and the THIRD copy of isParentVisible() -- the one in
                hg/lib/hgFind.c at line 2957, feeding isTrackVisible() at 2977. It sets
                category->visibility, which is what files a result under "Visible Tracks"
                rather than "Currently Hidden Tracks". Turns on a searchable GENCODE
                archive subtrack, whose containers are hidden by default, and asserts the
                result lands on the visible side. No login needed, unlike collection.
  composite     clinvar with clinvarCnv hidden: the two-request split (#37953). One
                request would leave clinvarCnv_sel=1 and the CNV row drawn.
  views         hideKids on the VIEW that holds the subtrack, with the sibling views
                hidden by name. Also covers the `_sel` checkbox, since the subtrack is
                `parent wgEncodeRegDnaseSignal off`, and pins the superTrack side effect
                below.
  views.xfail   the same thing aimed at the COMPOSITE instead, which loses the row.
                Expected to fail.
  supertrack    varsInPubs hideKids + one member: `exact: true`, because a test that only
                checked the member was present would pass with all six drawn.
  urllen        {cCREs: hideKids} must not become the 1701-variable, 42,020-character GET
                that Apache answered with 414. Checks `noText: "Too Long"`, since a 414
                renders as a perfectly good page; the derive baseline pins it at 3.
  customtrack   addCustomTrack: with inline BED, tabs and newlines surviving the trip.
  scale         a 3x run draws the same rows as a 1x one.
  ordered       `ordered: true` on rows:, and the fact that a row which was not drawn is
                reported by rows: alone rather than failing the order check as well.
  ordered.xfail the same two rows named the wrong way round. Expected to fail -- a flag
                that cannot fail is not a check, it is a second copy of the set test.
  pagechecks    the `expect:` checks that read the PAGE rather than the track image --
                `url:`/`noUrl:` on the address, `has:`/`noHas:` on a CSS selector -- plus
                the positional form of `click:` (`{track:, frac:}`), which follows the
                item box nearest a point. All four exist for bugs that rows:, height: and
                text: cannot see: a search term's zero-width space stripped out of a URL
                (#36387), a center label attached to the wrong row (#37785), and an item
                that cannot be named at all because its track is `type bigBed 3` (#36335).
  pagechecks    the same four aimed the wrong way at once. Expected to fail. The message
    .xfail      names every check that failed, so one run says which of the four broke.
  colorchecks   `color:`, the one check that reads the track IMAGE: is:/not: on the color
                a row is mostly drawn in, `part: label` for the center label instead of
                the items, `at:` for one item rather than the whole row, and the list
                form. It exists for #36212, where a track that sets both `itemRgb on` and
                `color` draws its items in the wrong one -- same rows, same height, same
                names, same tooltips, so nothing but the pixels can tell.
  colorchecks   the same six aimed wrong, all in ONE expect: step so the message has to
    .xfail      name all six. Expected to fail. The comment lists them in order; read the
                log rather than trusting the exit code.
  expectfail    an assertion that is plainly false. Expected to fail -- if it ever passes,
    .xfail      `expect:` has stopped throwing and every other test here means nothing.
  make parity   FAST vs slow, and a rerun, on composite. FAST drops the dwells and the
                recording and must not change what the page ends up showing; the rerun
                catches state left behind in the cart.

Two things these tests found
----------------------------

Worth knowing before writing more:

  * Turning on anything under a superTrack sends `<superTrack>=show`, and every OTHER
    member then comes up at its own trackDb visibility -- so `hide: all` is undone for
    them. views asserts wgEncodeRegMarkH3k27ac comes back, rather than working around it.
    Whether Docent should be cleverer here is an open question, not a settled one.
  * `hideKids` on a VIEW has to enumerate its leaves (a view holds no sub-containers to
    stop at), so views sends 188 variables in a 6,986-character request. That is under
    Apache's 8,190 limit with less room than is comfortable. Its `noText: "Too Long"` is
    what turns a future overflow into a clear failure instead of a strange one.

Still to write
--------------

  mouseover:      by item: on stacked items, and the timing case where a neighbour's
                  tooltip is still up on arrival
  pinShot:        several tooltips in one figure, cursors drawn
  convert:        quickLift onto a GenArk haplotype, hideDefaults re-checked -- note a
                  session taken after it cannot be checked in, see #38046
  drag:           each of then: zoom / highlight / cancel
  addHub:,
  addPublicHub:   the two hub attach paths (a stable hub URL is the hard part)
  montage:        panel order, lettering, a named shot that was never taken
  goShow:         the suggestion menu, including a `pick:` that matches nothing
  loadSession:    the three remote forms -- only the local-file form is covered
  the YAML lint   `{item:name}` with no space warns and drops the argument. This needs a
                  test that reads stderr, which the harness does not do yet.

A test that needs a stable server-side fixture (a hub, a custom track) should carry it
in the script rather than assume something on disk.

The one fixture that cannot be carried anywhere is a login. hgCollection refuses a
visitor who is not signed in, and the login cookie is checked against a salted hash, so
a script that needs that page uses the `login:` step, and the step reads an account from
~/.docentLogin. That file is one [section] per HGCENTRAL DATABASE, because an account is
a row in gbMembers in one of them: genome-test, hgwdev, every sandbox and every ticket
park read hgcentraltest and share one account, while hgwbeta and the RR are separate sets
of accounts. Which central a server reads is read from its hg.conf rather than guessed
from the host -- a sandbox can point itself somewhere else, and two on hgwdev do today.
`make preflight` says which account and which central it resolved for the server being
driven, and refuses a file that is readable by group or other. No password is ever
printed and none can be written in a script. ../README.md under `login` has the format.

colorchecks is the one exception, and the reason is worth knowing before someone else
hits it. `color:` has to address a ROW by name, and a custom track cannot be addressed
by name at all: hgTracks assigns its row id (`ct_<name>_<number>`), which is why
customtrack asserts on label text instead of on `rows:`. So an inline custom track --
the self-contained way to get a known color onto the page -- is the one fixture this
check cannot use. It reads ~/public_html/docentFixtures/itemRgbHub/ instead, which
tests/regress/rm36212.xfail needs anyway, and which `make preflight` checks is still
there.
