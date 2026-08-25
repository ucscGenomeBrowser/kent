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

DOCENT ?= ../docent.js
PW_DIR ?= /hive/groups/browser/uiTest/pw
PW_ENV ?= PLAYWRIGHT_BROWSERS_PATH=$(PW_DIR)/browsers NODE_PATH=$(PW_DIR)/node_modules
T      ?=
TESTS  := $(if $(T),$(addsuffix .docent.yaml,$(T)),$(wildcard *.docent.yaml))

.PHONY: test parity clean

test:
	@fail=0; \
	for f in $(TESTS); do \
	  b=$${f%.docent.yaml}; want=0; \
	  case $$b in *.xfail) want=1;; esac; \
	  if [ $$want = 1 ]; then printf '=== %s (expected to fail)\n' "$$b"; \
	  else printf '=== %s\n' "$$b"; fi; \
	  $(PW_ENV) node $(DOCENT) $$f > $$b.log 2>&1; got=$$?; \
	  if [ $$got -ne 0 ] && [ $$want -eq 0 ]; then \
	    echo "  FAILED -- run said:"; sed 's/^/    /' $$b.log; fail=1; \
	  elif [ $$got -eq 0 ] && [ $$want -eq 1 ]; then \
	    echo "  FAILED -- this was supposed to fail, and it passed"; fail=1; \
	  else echo "  ok"; fi; \
	done; \
	if [ $$fail -eq 0 ]; then echo "docent tests passed"; else echo "docent tests FAILED"; exit 1; fi

# Two invariants that need the same script run more than once, so they cannot be
# written as a script of their own:
#   FAST parity   -- FAST drops the dwells, the cursor animation and the recording.
#                    It must not change what the page ends up showing.
#   rerun stability -- a second run in the same directory must reach the same state.
#                    Cart bleed between runs would show up here and nowhere else.
PARITY ?= composite
parity:
	@echo "=== $(PARITY) fast"; \
	  DOCENT_FAST=1 $(PW_ENV) node $(DOCENT) $(PARITY).docent.yaml > parity.fast.log 2>&1 \
	  || { sed 's/^/    /' parity.fast.log; exit 1; }
	@echo "=== $(PARITY) slow (records an mp4, so this one is not quick)"; \
	  $(PW_ENV) node $(DOCENT) $(PARITY).docent.yaml > parity.slow.log 2>&1 \
	  || { sed 's/^/    /' parity.slow.log; exit 1; }
	@echo "=== $(PARITY) again, to catch state left behind by the last run"; \
	  DOCENT_FAST=1 $(PW_ENV) node $(DOCENT) $(PARITY).docent.yaml > parity.rerun.log 2>&1 \
	  || { sed 's/^/    /' parity.rerun.log; exit 1; }
	@echo "parity passed"

# The derivation on its own: DOCENT_DERIVE=1 resolves each `track:` step against the
# server's trackDb and prints the cart variables, with no browser and no navigation. That
# is where Docent's own decisions are, and it runs in about a second, so it is worth
# checking against a baseline.
#
# Only the scripts with a file in expected/ are checked. The output depends on LIVE
# trackDb, so a baseline can go stale for an honest reason -- a new member of a superTrack,
# a retired subtrack. When that happens, read the diff before believing it:
#
#     make derive           # diff every baseline
#     make derive-accept    # rewrite the baselines, then `git diff` them
#
# Scripts whose derivation is large and churny (views, 188 variables from one view-level
# hideKids) deliberately have NO baseline: it would fail every time ENCODE gained a cell
# line, and the browser test already covers the behaviour.
DERIVE_ENV = DOCENT_DERIVE=1 $(PW_ENV)
BASELINES := $(patsubst expected/%.derive,%,$(wildcard expected/*.derive))

.PHONY: derive derive-accept

derive:
	@fail=0; \
	for b in $(BASELINES); do \
	  $(DERIVE_ENV) node $(DOCENT) $$b.docent.yaml > $$b.derive.out 2>&1; \
	  if diff -u expected/$$b.derive $$b.derive.out > $$b.derive.diff; then \
	    echo "=== $$b"; echo "  ok"; rm -f $$b.derive.diff; \
	  else \
	    echo "=== $$b"; echo "  CHANGED -- read this before accepting it:"; \
	    sed 's/^/    /' $$b.derive.diff; fail=1; \
	  fi; \
	  rm -f $$b.derive.out; \
	done; \
	if [ $$fail -eq 0 ]; then echo "derivation baselines match"; else echo "derivation CHANGED"; exit 1; fi

derive-accept:
	@mkdir -p expected
	@for b in $(BASELINES); do \
	  $(DERIVE_ENV) node $(DOCENT) $$b.docent.yaml > expected/$$b.derive 2>&1; \
	  echo "rewrote expected/$$b.derive"; \
	done
	@echo "now read: git diff expected/"

clean:
	rm -rf stills sessions *.log *.derive.out *.derive.diff ../composite.mp4
