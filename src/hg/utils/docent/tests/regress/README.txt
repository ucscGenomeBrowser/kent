Docent regression tests
-----------------------

One script per fixed bug, named for its ticket. Run by hand:

    make test               # every *.docent.yaml here
    make test T=rm36382     # just one

The candidate list this directory is being built from, with the recipe and assertion
worked out for each ticket, is at

    /hive/groups/browser/redmineNotes/37892/claude/2026-09-04_1100_regression_candidates.md

What these are, and what they are not
-------------------------------------

Each script asserts the behavior the ticket says is correct, on genome-test. None of them
was run against a build that still had the bug, so none has been seen to fail for the
reason it exists. That is a deliberate choice about cost, and it puts the whole weight on
how tight the assertion is:

  * name the error string the ticket quoted in `noText:`, not a generic "Error"
  * prefer `rows: [...] exact: true` and `noRows:` over a bare `rows:`
  * a test that only checks a row is PRESENT usually passes on the buggy build too,
    because the bug was an extra row, a wrong label, or a bad tooltip

Two things will rot these tests
-------------------------------

Most recipes start from the saved session named in the ticket, because that is the
cheapest way to reach the exact state. A session that is deleted does not fail loudly:
hgTracks serves a page saying it could not find it, and every `noText:` check on that
page passes. So a session-based test also asserts something that is only true when the
session really loaded.

Six recipes need a test hub on a colleague's public_html. Same problem, same remedy.

Fixtures we own live in ~/public_html/docentFixtures/, and `make preflight` checks that
every hub a script here names still answers. Copy a reporter's hub in there rather than
loading theirs, so nothing outside this repository can change what a test measures.

rm36212 is the one to read before writing another
--------------------------------------------------

It is the only script here that has been watched to fail on a build with the bug AND to
pass on a build with the fix, which is the evidence every other script in this directory
would like to have and does not. The recipe: build the fix into a ticket sandbox, point a
copy of the script at that port with `target: http://127.0.0.1:PORT/cgi-bin`, and record
in the comment which checks flipped. It costs one build and it settles what a tight
assertion can only argue.

It is also the first script to assert a COLOR, using `expect: {color: ...}`, because it
is the first bug here that leaves the page identical -- same rows, same height, same item
names, same tooltips. When rows:, height:, text: and has: are all blind to a bug, the
pixels are what is left. See tests/colorchecks.docent.yaml for the check itself.
