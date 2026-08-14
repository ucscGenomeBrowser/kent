Docent tests
------------

Run by hand, not by the kent tree's `make test`:

    make test               # every *.docent.yaml here
    make test T=composite   # just one
    make parity             # one script FAST and slow, and twice over
    make derive             # the derivation alone, against expected/ (no browser)
    make derive-accept      # rewrite those baselines, then read `git diff expected/`

Most tests drive a real browser against a real server, so they need the network and
the shared Playwright install (~/pwrec; see ../README.md). That is why none of this is
part of the tree-wide test target: a broken network would fail the build.

A test is an ordinary Docent script that asserts with `expect:`. It passes by exiting
0. `expect:` is the only verb that fails a run, so a test with no `expect:` step in it
tests nothing.

A script named *.xfail.docent.yaml is expected to FAIL, and the run fails if it passes.
That is how a trap gets pinned rather than merely written down.

`make derive` is the cheap half: DOCENT_DERIVE=1 resolves the `track:` steps against
trackDb and prints the cart variables without opening a browser, in about a second. It
is where Docent's own decisions live, and the baselines in expected/ are what catch a
change to visVars() or tdbHideTargets() that a rendered page would hide.

What is covered
---------------

  selftest      session: -> expect: -> loadSession:, on hg38 at SHH. Saves the cart,
                changes the view, restores it from the local file, checks rows both times.
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
