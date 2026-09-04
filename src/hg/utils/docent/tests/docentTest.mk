# Shared rules for a directory of Docent tests. Included by tests/makefile and by
# tests/regress/makefile, so both directories run the same code rather than a copy of it.
#
# An including makefile sets, before the include:
#   DOCENT   path to docent.js from THIS directory   (required)
#   PARITY   which script `make parity` runs         (default: the first one found)
#
# Everything else -- which scripts are tests, which have derive baselines -- comes from
# what is on disk here, so a new *.docent.yaml is picked up with no edit.

ifndef DOCENT
$(error include docentTest.mk only after setting DOCENT, e.g. DOCENT = ../docent.js)
endif

PW_DIR ?= /hive/groups/browser/uiTest/pw
PW_ENV ?= PLAYWRIGHT_BROWSERS_PATH=$(PW_DIR)/browsers NODE_PATH=$(PW_DIR)/node_modules
T      ?=
# `make parity` needs one script that is expected to PASS, so an .xfail one is no use as
# the default. An including makefile can name a better one.
PASSING := $(filter-out %.xfail,$(patsubst %.docent.yaml,%,$(wildcard *.docent.yaml)))
PARITY ?= $(firstword $(PASSING))
TESTS  := $(if $(T),$(addsuffix .docent.yaml,$(T)),$(wildcard *.docent.yaml))

.PHONY: test parity clean

test:
	@if [ -z "$(strip $(TESTS))" ]; then \
	  echo "no *.docent.yaml here -- nothing was tested"; exit 1; fi
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
#
# One line has to be stripped before the diff. docent.js caches the trackDb listing in
# $TMPDIR for a day, and prints `trackDb: N tracks for DB from .../hubApi` only when it
# actually fetches. So the first run of the day carries a line that every run after it
# does not, and a baseline captured warm would fail against a cold run for a reason that
# is not about trackDb at all. Both targets strip exactly that line, so it cannot get
# into a baseline either. The other two trackDb lines -- a hub genome, an unreachable
# hubApi -- are real news about the derivation and are left in.
DERIVE_ENV = DOCENT_DERIVE=1 $(PW_ENV)
DERIVE_FILTER = sed '/^trackDb: [0-9][0-9]* tracks for /d'
BASELINES := $(patsubst expected/%.derive,%,$(wildcard expected/*.derive))

.PHONY: derive derive-accept

derive:
	@if [ -z "$(strip $(BASELINES))" ]; then \
	  echo "no baselines in expected/ -- nothing was checked"; exit 1; fi
	@fail=0; \
	for b in $(BASELINES); do \
	  $(DERIVE_ENV) node $(DOCENT) $$b.docent.yaml 2>&1 | $(DERIVE_FILTER) > $$b.derive.out; \
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
	  $(DERIVE_ENV) node $(DOCENT) $$b.docent.yaml 2>&1 | $(DERIVE_FILTER) > expected/$$b.derive; \
	  echo "rewrote expected/$$b.derive"; \
	done
	@echo "now read: git diff expected/"

clean:
	rm -rf stills sessions *.log *.derive.out *.derive.diff *.mp4
