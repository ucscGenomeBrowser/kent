# Shared make rules for a CGI's browser UI tests (see README.md in this directory).
#
# A CGI that keeps UI tests in its own tests/ directory includes this file and
# gets every target below. Two lines in src/hg/<cgi>/tests/makefile:
#
#     UITEST ?= $(CURDIR)/../../utils/uiTest/uiTest
#     include $(dir $(UITEST))uiTest.mk
#
# The path is relative because a CGI's tests/ directory always sits the same two
# directories from the harness. That way a git worktree, or a tree checked out
# anywhere but $HOME/kent, needs no override.
#
# then:
#
#     make uiTest              # run every t*.js and *.docent.yaml here
#     make uiTest T=t01        # just the files whose name contains t01
#     make uiTest G='hide all' # just the checks whose name contains "hide all"
#     make headed              # show the browser, slowly -- to WATCH a failure
#     make uiTest TARGET=hgwdev-$(USER)
#     make selfcheck           # is the setup sound? run this first when confused
#     make lint                # the waitForTimeout policy
#     make clean               # nothing to do; runs write to the artifacts dir, not here
#
# The default goal is help, so a bare `make` prints usage instead of launching a
# browser at whatever server it felt like.

UITEST      ?= $(CURDIR)/../../utils/uiTest/uiTest
TARGET      ?=
T           ?=
G           ?=
UITEST_FLAGS ?=

# A run in hgTracks/tests is a run of hgTracks, not of "tests".
CGINAME := $(if $(filter tests,$(notdir $(CURDIR))),$(notdir $(patsubst %/,%,$(dir $(CURDIR)))),$(notdir $(CURDIR)))

FLAGS = $(UITEST_FLAGS) \
	$(if $(TARGET),--target '$(TARGET)',) \
	$(if $(T),--only '$(T)',) \
	$(if $(G),--grep '$(G)',)

.PHONY: help test uiTest headed selfcheck lint clean

help:
	@echo "$(CGINAME) UI tests:"
	@echo "  make uiTest              run them"
	@echo "  make uiTest T=t01        one file"
	@echo "  make uiTest G='hide all' one check"
	@echo "  make headed              watch the browser do it"
	@echo "  make selfcheck           check node, playwright, conf and target"
	@echo "  make lint                the waitForTimeout policy"
	@echo "  make TARGET=hgwdev-$(USER) uiTest"

# Deliberately inert. These tests need a network, a browser and a target server,
# so they are NOT part of the tree-wide `make test`. Nothing sweeps a CGI's
# tests/ directory today -- src/hg/makefile's testAll covers APPS, and the CGIs
# are in BROWSER_BINS -- but that is safe by accident of a list. This target
# means that the day a CGI joins APPS, `make test` stays green on a machine with
# no network instead of trying to open a browser.
test:
	@echo "$(CGINAME) UI tests need a network, a browser and a target server."
	@echo "They are deliberately NOT part of the tree-wide 'make test'.  Run:  make uiTest"

uiTest:
	$(UITEST) $(FLAGS) .

headed:
	$(UITEST) $(FLAGS) --headed --slowmo 250 .

selfcheck:
	$(UITEST) $(FLAGS) --selfcheck

lint:
	$(UITEST) lint .

clean:
	@:
