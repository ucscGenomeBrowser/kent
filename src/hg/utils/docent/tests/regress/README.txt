Docent regression tests
-----------------------

One script per fixed bug, named for its ticket. Run by hand:

    make test               # every *.docent.yaml here
    make test T=rm36382     # just one
    make proof              # what evidence each script has, and the tally

The candidate list this directory is being built from, with the recipe and assertion
worked out for each ticket, is at

    /hive/groups/browser/redmineNotes/37892/claude/2026-09-04_1100_regression_candidates.md

What these are, and what they are not
-------------------------------------

Each script asserts the behavior the ticket says is correct, on genome-test. Most were
written after the fix had already shipped, so most have never been seen to fail for the
reason they exist. `make proof` says exactly how many have and which ones, reading a
`proof:` key that every script carries; as of 2026-09-10 it is 4 of 37. That is a
deliberate choice about cost, and for the other 33 it puts the whole weight on how tight
the assertion is:

  * name the error string the ticket quoted in `noText:`, not a generic "Error"
  * prefer `rows: [...] exact: true` and `noRows:` over a bare `rows:`
  * a test that only checks a row is PRESENT usually passes on the buggy build too,
    because the bug was an extra row, a wrong label, or a bad tooltip

Three things will rot these tests
---------------------------------

Most recipes start from the saved session named in the ticket, because that is the
cheapest way to reach the exact state. A session that is deleted does not fail loudly:
hgTracks serves a page saying it could not find it, and every `noText:` check on that
page passes. So a session-based test also asserts something that is only true when the
session really loaded.

Six recipes need a test hub on a colleague's public_html. Same problem, same remedy.

Fixtures we own live in ~/public_html/docentFixtures/, and `make preflight` checks that
every hub a script here names still answers. Copy a reporter's hub in there rather than
loading theirs, so nothing outside this repository can change what a test measures.

A fixture hub must never name a track anything the assembly might also call it. A track
name resolves to `img_data_<name>` first and only then to a hub row's `hub_<n>_<name>`,
so an exact native id wins: the hub row is on the page, and every `track:`, `mouseover:`
and `rows:` in the script reads the NATIVE row instead. Nothing warns. rm35920's fixture
called its track `ultras`, hg38 has its own `ultras`, and that script asserted a tooltip
off the native data for as long as it existed -- it looked green and tested nothing.
Prefix a fixture's track names with the ticket number.

Two rules for a test about visibility, cart state or session loading
---------------------------------------------------------------------

Both came out of the #37547 cart-visibility branch, where nine scripts here went red for
a real bug and a second bug of the same shape went past every one of them.

**Assert before you navigate.** A bug in this area can lag by exactly one request: the
visibility reaches the cart, the image drawn in reply does not carry the row, and the
next request -- any next request -- draws it. A script shaped `track:` -> `go:` ->
`expect:` supplies that extra request itself and passes on the broken build. A script
shaped `track:` -> `expect:` fails. So when the point of a script is that a track comes
on, put an `expect:` straight after the `track:` step, before any `go:`, `open:` or
`convert:`, even when the script needs the navigation for its own reason afterwards. It
costs one assertion.

Twenty-five scripts here have a `track:` step and forty do not. Of the twenty-five, all
but two already assert straight after it. The two are rm36514 and rm37520, and neither
can: both turn on mane at the ticket's own position, chr7:156,982,676-156,996,015, where
mane has no features on hg38 at all, so there is no row to assert and no other check on
that page can tell the cart from the image -- the track controls below it show `pack` on
a broken build too, because the cart really did take the visibility. Their headers say so, so that the gap is
not read as an oversight and closed with a check that passes on anything.
tests/firstrequest.docent.yaml covers the class once, on its own.

**Name a child of a container that is hidden by default.** Two shapes of track stay green
on a build whose visibility handling is broken, for two different reasons:

  * A TOP-LEVEL track. hgTracks adds every top-level track as a lightweight stub so the
    track controls can list it, so it is built whatever the cart lookup returned.
    microsat, gtexGene and windowmaskerSdust all drew on the broken #37547 build.
  * A DEFAULT-VISIBLE child. Its container is in the list already.
    wgEncodeRegMarkH3k27ac drew while its sibling wgEncodeRegMarkH3k4me1 did not -- same
    superTrack, same request, opposite verdicts.

wgEncodeRegMarkH3k4me1 is the known-good example: `visibility hide`, and
`superTrack wgEncodeReg hide` under a superTrack that is itself `superTrack on hide`.
This is the cheapest rule in this file -- it changes which track a script names, not how
the script is written -- and without it a script reads as coverage and is not.

Reading a run that was redirected somewhere else
--------------------------------------------------

`make test TARGET=...` points the whole directory at another server, and a red script
there can be that machine's configuration rather than a bug. `make preflight TARGET=...`
now prints the target's central.db, db.trackDb, curatedHubPrefix and quickLift settings
when the server is on this machine, so the log carries its own explanation; see
../README.txt. When that is not enough, swap only the BINARY -- drop a control build's
CGIs into the same sandbox, leave its hg.conf alone, and re-run. If the failures follow
the binary they are the code. On #37547 that experiment is what turned nine plausible
failures into nine proven ones.

Proof: which scripts have been watched to fail for their own reason
-------------------------------------------------------------------

Every script carries a top-level `proof:` key, one quoted line per piece of evidence,
each `<level> <YYYY-MM-DD> -- <what was seen>`. docent.js reads only the keys it names,
so the key costs a run nothing. `make proof` tallies it and fails on a line that is
malformed or names a level outside the vocabulary, which is what keeps it countable.

The levels, weakest first:

  assertion-only     asserts the fixed behavior; never seen to fail for its own reason
  xfail              seen failing right now for its own reason; the fix has not shipped
  sandbox-ab         seen failing on a build with the bug and passing on a build with
                     the fix, both built by hand
  release-ab         seen failing on a RELEASED version that predates the fix and passing
                     on one that carries it
  server-flip        seen failing then passing on a real server as a real build arrived
  caught-regression  went red for a regression that was then filed and fixed

RELEASE-AB IS THE ONE TO REACH FOR, and it is the standard this directory now works to.
Build a released version into a ticket sandbox and point the whole directory at it:

    ts create NNNNN "v503_branch, detached, as the A/B baseline"
    make -j 24 cgi CGI_BIN=$TS/cgi USER=bin DOCUMENTROOT_USER=$TS/htdocs-$LOGNAME
    make test TARGET=http://127.0.0.1:PORT/cgi-bin

A script that fails there and passes on genome-test has been watched to fail for its own
reason, and the claim is reproducible by anyone from a tag, forever. sandbox-ab is the
same shape with a tree you patched yourself, which is weaker for one reason: nobody else
can rebuild it, and after a rebase nor can you.

Three things it needs. Build the JS and the HTDOCS into the freeze as well, not only the
CGIs -- three of the twenty-two fixes in the 2026-09-17 batch live in hg/js or
htdocs/style and would not have travelled with a CGI-only build. Check that the baseline
really predates the fix (`git merge-base --is-ancestor <fix> origin/vNNN_branch`): three
of that batch's fixes were already in v503, so v503 is the wrong baseline for them and an
older release is their standard. And read every failure MESSAGE rather than the verdict,
because a release is a month of unrelated change as well as the fix you are testing.

The hand-patched route is documented below because it is still what you do for a fix too
recent for any release. Build the fix into a ticket sandbox, point a copy of the script
at that port with `target: http://127.0.0.1:PORT/cgi-bin`, and record which checks
flipped. What it CANNOT do is undo a fix that later work has built on: `git revert` then
conflicts, and a reconstructed "master minus this one commit" is a state that never
existed. Two attempts to force it through a wholesale file restore did not even compile,
both times in code unrelated to the ticket. Take the conflict as the answer and use a
release instead.

server-flip is the one this directory gets for free, and it is better evidence, because
nothing about the server changed except the build. Commit a script for an unshipped fix
as an .xfail. `make test` fails when an xfail PASSES, so the morning the fix reaches
genome-test the nightly goes red and says so. nightly.sh appends that to

    /hive/users/braney/docentNightly/flips.log

one line per script ever, outside the checkout because --update resets the tree. Then
drop the .xfail from the name and add the server-flip line to the script's proof: key.
rm38272, rm36212 and rm38310 all arrived that way.

rm36212 is still the one to read before writing another
--------------------------------------------------------

It is the worked example of both routes: sandbox-ab on 2026-09-09 against parked #36212
on port 48099, then server-flip the same morning when cbb406cd96e reached genome-test.

It is also the first script to assert a COLOR, using `expect: {color: ...}`, because it
is the first bug here that leaves the page identical -- same rows, same height, same item
names, same tooltips. When rows:, height:, text: and has: are all blind to a bug, the
pixels are what is left. See tests/colorchecks.docent.yaml for the check itself.

rm38310 is the second color check, and the second script watched both ways
---------------------------------------------------------------------------

Same recipe as rm36212, and worth reading for the reason it needs pixels, which is
different. Its bug does not draw the wrong color; it replaces the row with the bigWarn
bar, 240,240,180 (undefinedYellowColor, hg/hgTracks/simpleTracks.c), and paints an error
message INSIDE the png. So the row is still drawn, still the same name, and every text
check on the page passes -- the message is in the image, where noText: cannot reach it.
That is also why the ticket was filed saying there was no warning at all.

Two things fall out of it that apply to any script here:

  * `rows:` cannot express "this track drew its items". The broken build draws the row.
    `color:` with `is:` on the item color and `not: "240,240,180"` can, and a failure
    prints what each row really came out.
  * A drawn item that cannot be clicked through is half a bug. rm38310 clicks its item
    and asserts the item's POSITION on the hgc page, because the aborted hgc page carries
    the track's longLabel twice in its own header and a text: check on that alone passes
    on it.

Measured both ways on 2026-09-09: the whole directory was run against the #38310 ticket
sandbox twice, once with the patched hgTracks and hgc and once with unpatched controls
built from the same tree. Thirty-seven scripts, identical verdicts, except this one.

Ten scripts for multi-region view, and the two traps they hit
--------------------------------------------------------------

rm22144, rm23922, rm26772, rm27855, rm29452, rm29787, rm30833, rm34250, rm35472 and
rm37175 are one batch, written 2026-09-12, and between them they cover the four modes
(exon, custom regions, alt haplotype, exit), the dialog, the custom-region BED reader,
hideEmptySubtracks across windows and highlights in both directions across the mode
change. Before them the only script here that entered multi-region at all was rm35580,
which uses singleAltHaplo to reach a different bug.

Two things learned writing them, both of which cost a red run first:

**Never assert on a `title` attribute.** hgTracks' own tooltip code moves a title into
`data-tooltip` once the page's JavaScript has run, so `area[title="chr1:10001-11000"]`
matches nothing in the live DOM even though the server sent exactly that. The server
writes both attributes on a map box; assert `data-tooltip`. The same applies to the
buttons, where the title changes with the mode and would otherwise be a second, free
assertion -- it is not available.

**Multi-region is reachable from the URL, and the dialog is not.** `virtModeType=`,
`multiRegionsBedInput=` (the textarea's own cart variable, newlines as %0A),
`singleAltHaploId=`, `virtWinFull=on` and `<composite>.hideEmptySubtracks=on` all work on
a `goto:`, which is how nine of the ten set their state -- Docent has no verb that types
into an arbitrary field, so the textarea and the alt-haplotype input cannot be filled.
What still needs the real dialog is anything the page's JavaScript decides: rm29452's
disabled radio and its status line are invisible to curl, because the server sends the
same HTML on a build with the bug and a build without it.

`virtWinFull=on` is worth knowing for a third reason: without it a region change lands
zoomed in on one region, so a second region is off screen and a script cannot tell a
region that failed to resolve from one that is merely not in view.

Ten more for quickLift, five on hgTracks and five on hgc
----------------------------------------------------------

rm36048, rm36059, rm36125, rm36370, rm36942, rm37646, rm37815, rm38032, rm38042 and
rm38146 are one batch, written 2026-09-12. Fourteen scripts here already lifted something
(they are the ones that call `convert: {quicklift: true}`); these add the parts of the
lift that had no test: the order tracks come out in, an item bigger than the chains
quickLift loads, the spanned-item merge, a lolly subtrack, the hide-target-defaults
checkbox, and five details pages -- GENCODE archive, hgGene, NCBI RefSeq, the Alignment
Differences description, and the same page with a GenArk assembly as the SOURCE.

Each one costs a convert, which is about 17 seconds: hgConvert plus a hub build plus the
click through to the browser. Budget for that before adding more.

Three things worth reusing from them:

**The lift is set up through the UI and read from the map.** There is no URL that makes a
quickLift hub, so every script here does `convert:` then `open: lift`. What comes back
carries a per-run `hub_<n>_` prefix on every row id and every map box, so `rows:` matches
by suffix and a `has:` selector has to use a substring (`area[href*="clinvarSubLolly"]`),
never an exact id.

**Do not assert a count that a data update can move.** rm38042 and rm36048 both read the
spanned-item merge box, and the tooltip on it counts the items merged -- 45 for ClinVar on
2026-09-12. That number is reloaded by an otto cron every month. Both scripts assert that
the box is THERE (`area[data-tooltip^="Merged "]`) and leave the count to a comment, so a
red morning is news about quickLift rather than about ClinVar.

**A details page carries the track's own labels, so name something else.** rm36125 asserts
SHH's N-terminal peptide, rm36059 a UniProtKB section, rm36370 two section headings that
were missing, rm38146 the query sequence read out of a two bit file. Each of those is
absent from the page the ticket was filed about and present on the fixed one; the track
name and longLabel are on both.

Ten for hgTrackUi, and what makes that page testable
------------------------------------------------------

rm20460, rm32263, rm34651, rm35906, rm36484, rm36668, rm36917, rm37130, rm37282 and
rm37743 are one batch, written 2026-09-12. Four scripts here already touched hgTrackUi in
passing (rm37389, rm37489, rm38126, rm38272); these are about the page itself: the
superTrack configuration page, composite and subtrack configuration, filters, the color
override, the parent link, and two bad-input paths.

They are also the cheapest scripts in the directory -- one to three seconds each, because
hgTrackUi draws no image and most of them never leave it.

**There is no track image, so `rows:` is not available and a positive `text:` is
mandatory.** A crash gives the browser an empty document, where every `noText:` and every
`noHas:` passes. Every script here names something the real page says.

**Most of what hgTrackUi does is in ids, names and classes, so `has:`/`noHas:` carries
these tests.** README says to reach for a selector last, and that is still right for
hgTracks, where rows, height, text and color can usually say it instead. On a settings
page the bug often IS the markup: a shared id that should be per-track (rm34651), a stray
tag inside a select (rm36484), a control that should not be offered for this track type
(rm20460), a class that greys a dropdown (rm37282). Name the id or class the commit
changed, and say in the header which one it is.

**A cart round trip is what tells a control that works from one that only looks right.**
rm35906's clear-filters button set every dropdown to All on screen on the buggy build too;
only submitting and coming back shows whether anything was saved. rm36668 does the same in
reverse, checking after the fact that the two checkboxes it clicked really are on.

**A dropdown cannot be driven.** Docent has no verb that picks an option from a select, so
a visibility is set on the way in through the URL (rm36668) and a button is clicked
instead where one exists (rm36917, rm37282).

Twenty-two for one QA list, and what the A/B against v503 said about them
--------------------------------------------------------------------------

rm10138, rm36940, rm37969, rm38171, rm38184, rm38198, rm38200, rm38205, rm38206, rm38223,
rm38236, rm38248, rm38251, rm38257, rm38279, rm38281, rm38283, rm38284, rm38285, rm38302,
rm38303 and rm38309 are one batch, written 2026-09-17 from a list of tickets that had no
script here. They are not a theme: they run from a menu-bar color to a SIGSEGV in a
quickLift view.

EVERY ONE OF THEM WAS THEN RUN AGAINST v503, which is the whole point and is what the
release-ab level above is for. v503_branch (707b184e329) went into ticket sandbox 38316,
CGIs, js and htdocs; twenty-one of the twenty-four fix commits landed after that branch was
cut, so the release has the bugs. Eighteen scripts failed there on their own check and now
carry a release-ab line quoting the failure. The four that did not are each worth reading:

  rm38171   its fix (6a8e756b473, 2026-08-24) is IN v503, so the script passes there.
            v502 or older is its baseline.
  rm36940   the fixture hub draws no rows at all on v503, so the run never reaches the
            field-count check. Its evidence is a hand-patched sandbox instead.
  rm38257   `login:` dies against a park on both baselines -- the hgLogin returnToURL(150)
            race -- so its A/B is blocked by the harness, not by the tree.
  rm38309   PASSES on v503, which is exactly what its header claims: the fix changes no
            byte of any page. That claim is now measured rather than argued.

One script had to be rewritten because of what the A/B said. rm38251 PASSED on a build
with its bug, at every width from 390 to 1099, and only the measurement showed why -- see
its header. That is the case for doing this at all: without the A/B it would have sat here
looking green forever.

**text: and noText: used to take ONE string, and a YAML list failed open.** `noText: ["a",
"b"]` stringified to `"a,b"`, which no page contains, so the check passed on anything -- and
passed silently, which is worse than failing. Six scripts in this batch were written that
way and six of them looked green. Both now take a list, like `rows:`, `noRows:`, `has:` and
`noHas:` always did, so the trap is gone; it is written down because the shape of it will
come back the next time a check is added that stringifies its argument.

**A title becomes `mouseoverText`, not `data-tooltip`.** The rule above says to assert
data-tooltip, and that is right for a MAP BOX, where the server writes both attributes.
Everywhere else the server writes only a title, and utils.js
(convertTitleTagsToMouseovers -> titleTagToMouseover -> addMouseover) moves the text into a
`mouseoverText` attribute and BLANKS the title. rm38279 reads the density note off the
track controls that way.

**A subtrack's longLabel is not readable at all.** The controls below the image list a
composite or superTrack under the container's own label, so a note appended to a CHILD's
longLabel reaches only the center label inside the image and the hgTracks JSON in a
<script> block -- and `text:` reads body.innerText, which skips a script. rm38279 was first
written against jaspar2026 and was measuring nothing. Name a top-level track.

**A CSS or JS file can be read as a page.** `goto: /style/nice_menu.css` renders as a plain
text document, so text:/noText: read the stylesheet the server is really serving. rm38206
and rm38251 both use it. Follow the goto with `wait: 'pre'`: Chromium builds that view a
moment after the load event, and without the wait the step reads an empty body perhaps one
run in five.

**`expect:` could not ask where a box is, and now it can.** rm38251's bug is geometry -- an
icon landing outside the blue bar, sliding across the menu items -- and no check reached it:
a selector says what is in the page and never where, and `color:` samples a track row inside
the image. `box:` was added for it: `inside:`, `clear:` (with an optional `gap:`) and
`height:`/`width:`, all reading real bounding boxes, and a failure that prints the
measurement. rm38251 asserts all three symptoms on the four pages b82bce91b10 measured.

Two things that shape a `box:` check. **One viewport per run** -- `size:` is read once, when
the browser context is made -- so a layout bug that only shows at certain widths needs one
script per width; rm38251 takes 700px and says which other width is worth having. And
**name the LINK, not the list item**: an `li` box carries padding and is wider than its
label, so on the home page the last `li` runs 16px past the icon while the links have 17px
of clear space. A check on the `li` goes red for a reason that is not a bug.

**Docent's default viewport is 1000px, which is inside a media query.** nice_menu.css folds
the top-right links into the hamburger below 1100px, so `#shareLink` and `#loginLink` are
not visible and a click on either times out after 30 seconds. rm10138 and rm38257 set
`size: [1400, 900]`. rm38251 wants the opposite and sets 700.

**A hub's files are cached by udc on the server.** An edit to a fixture hub is not visible
for minutes, and the trackDb, the bigBed and each html page expire independently, so a run
can see a new trackDb and an old description page. The cache is owned by apache and cannot
be cleared from a developer account. Give a changed page a NEW FILE NAME.

Three fixtures were added for these, under ~/public_html/docentFixtures/: rm36940 (seven
bigBed tracks differing only in the field count on the type line, copied from Jairo's hub so
that nothing outside this repository can change it), rm38283 (one description page full of
dollar variables, shared by a composite, a subtrack under a view, and a superTrack child)
and rm38184 (a one-line session settings file). preflight.js now reads hgS_loadUrlName out
of a goto: URL as well, so the last of those is checked like any other fixture.

rm38257 is the FIRST SCRIPT HERE THAT LOGS IN. The Account popup exists only for a signed-in
reader, so there was no other way in. `login:` reads ~/.docentLogin, one section per
hgcentral database; anyone without that file fails this one script at its first step.

Two things in this batch are deliberately not asserted, each said again in its own script:
dd8d4476a69, the spacing follow-up on #38281, had not reached genome-test that day; and
#38303's own case needs a symlinked session-data directory, which genome-test does not have
(`namei -l /data/apache/userdata/sessions` shows no symlink), so that script covers the
trash branch of the same check instead.


Six from the tours, 2026-09-19
------------------------------

rm38249 rm38268 rm38298 rm38335 rm37996 rm38364 were written from Docent TOURS rather than
from a ticket alone. Each ticket had had a before-and-after pair built for it and posted as
a video or a picture, so the state was already reachable and the difference between the two
builds was already measured; turning that into a script was the cheap half.

That is why five of the six carry release-ab and the sixth sandbox-ab on their first day.
The before was not inferred, it was rendered: hgwbeta returns "Mangled CGI input string
bogus" (rm38335), one codon number instead of two (rm38298), the ASCII table with no banner
(rm37996), "not supported by QuickLift" for all four alignment families (rm38249) and
"Can't find strchive in track database hg19" (rm38268). rm38364's before is ts park 38373,
master frozen two days before the merge, because that feature is newer than any release.

Three things in this batch are worth copying:

  * WHERE THE TEXT LIVES decides how it is read. rm38309 reads its exon text out of a map
    box's data-tooltip, but rm38298's codon tooltip is not in the served map at all -- the
    page's own JavaScript builds it on mouseenter -- so that one hovers and asserts `tip:`.
    rm38268's item label is in the drawn PNG and nowhere else, so it asserts the map box's
    href and tooltip instead of a `text:` that can never match.
  * A SUPERTRACK STILL DOES NOT PASS ITS SETTING to its members. rm38268 opens strVar to
    reach strchive and then cannot use `exact:` on hg38, because the other repeat tracks
    come up at their own visibilities. Naming them to hide would rot the day one is added.
  * AN hg.conf GATE BELONGS IN THE HEADER, loudly. rm38364 fails every check if
    `denseClick` is off on the server under test, and that is a configuration answer, not a
    regression. The header carries the grep that settles it, the way rm38248 carries the
    hgsql that settles its missing table.

rm37996 has a DATE ON IT. The new hgBlat results page is opt-in while it is tested and the
banner names 2026-10-21 as the day it becomes the default. On that day the first two steps
come out and the goto: gains blatNewPage=1. A red run then is this script asking to be
updated rather than a bug.
