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
