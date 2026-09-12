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
  server-flip        seen failing then passing on a real server as a real build arrived
  caught-regression  went red for a regression that was then filed and fixed

Two ways to earn the middle levels. sandbox-ab is the one you can choose to do: build the
fix into a ticket sandbox, point a copy of the script at that port with
`target: http://127.0.0.1:PORT/cgi-bin`, and record which checks flipped. It costs one
build and it settles what a tight assertion can only argue.

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
