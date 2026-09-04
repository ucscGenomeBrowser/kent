# Docent REGRESSION tests: one script per bug we have already fixed, asserting the
# behavior the ticket says is correct. Separate from ../ on purpose, and run nightly
# against genome-test.
#
# Measured 2026-09-04: the eleven scripts next door take 56s all together (3-8s each, all
# in FAST mode). A `convert:` test measured 16s, because a quickLift hop is hgConvert plus
# a hub build plus the click through to the browser. Fourteen of the forty here open with
# `convert:`, so the estimate for a full run is about seven minutes, not the half hour a
# guess would suggest. Re-measure rather than trusting this comment.
#
#     make test               # every *.docent.yaml here
#     make test T=rm36382     # just one
#     make derive             # the derivation alone, for the scripts with a baseline
#
# Naming: rm<ticket>.docent.yaml, so the ticket is the filename. Every script says in its
# first lines which ticket it is, what the symptom was, and what the assertion would
# catch -- a regression test whose comment does not name the bug is unmaintainable, since
# the next reader cannot tell a deliberate assertion from an accident.
#
# These assert the FIXED behavior and were never watched to fail on a buggy build, so the
# assertions are deliberately tight: `exact:` and `noRows:` rather than a bare `rows:`,
# and `noText:` on the exact error string the ticket quoted. A loose assertion here is
# indistinguishable from no test at all.

DOCENT = ../../docent.js

include ../docentTest.mk
