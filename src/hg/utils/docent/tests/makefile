# Docent tests. NOT wired into the kent tree's `make test`, and deliberately so:
# every test here drives a real browser against a real server, so it needs the
# network and the shared Playwright install. Run it by hand.
#
#     make test               # every *.docent.yaml here
#     make test T=composite   # just one
#     make parity             # same script FAST and slow, and twice over
#
# A test passes by exiting 0. It fails when an `expect:` step does not hold, which
# names what it wanted and what was actually drawn, and exits 1.
#
# A script named *.xfail.docent.yaml is expected to FAIL, and the run fails if it
# passes. That is how a documented trap gets pinned: views.xfail asserts that
# `hideKids` aimed at the composite really does lose the row, so the day that
# changes, someone is told rather than left to notice.

DOCENT = ../docent.js
PARITY ?= composite

include docentTest.mk
