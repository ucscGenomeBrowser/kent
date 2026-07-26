# Shared make rules for Docent tour scripts (see README.md in this directory).
#
# A project that keeps a set of *.docent.yaml scripts includes this file and gets
# incremental rebuilds: each ../<base>.mp4 is regenerated when its own script — or
# docent.js itself — is newer. docent.js writes the mp4 and the named stills in one
# run, so the mp4 stands in for both as the make target.
#
# In the project's Makefile:
#
#     DOCENT ?= $(HOME)/kent/src/hg/utils/docent/docent.js
#     include $(dir $(DOCENT))docent.mk
#
# then:
#
#     make            # build every mp4 whose script (or docent.js) changed
#     make AP1        # build just ../AP1.mp4 (if stale)
#     make -B AP2     # force a rebuild
#     make list       # list the base names discovered
#     make clean      # remove generated mp4s and stills/
#
# Override before the include: FIGDIR (where mp4s land, default ..), PW_ENV (the
# Playwright runtime), SCRIPTS/BASES (to build an explicit subset).

DOCENT  ?= $(HOME)/kent/src/hg/utils/docent/docent.js
SCRIPTS ?= $(wildcard *.docent.yaml)
BASES   ?= $(SCRIPTS:.docent.yaml=)
FIGDIR  ?= ..
MP4S    := $(addprefix $(FIGDIR)/,$(addsuffix .mp4,$(BASES)))

# Shared Playwright/Chromium install. Anywhere you have playwright + js-yaml works;
# at UCSC this is the ~/pwrec tree.
PW_ENV  ?= PLAYWRIGHT_BROWSERS_PATH=$(HOME)/pwrec/browsers NODE_PATH=$(HOME)/pwrec/node_modules

.PHONY: all list clean $(BASES)

all: $(MP4S)

$(FIGDIR)/%.mp4: %.docent.yaml $(DOCENT)
	$(PW_ENV) node $(DOCENT) $<

# Convenience: `make AP1` -> build ../AP1.mp4
$(BASES): %: $(FIGDIR)/%.mp4

list:
	@echo $(BASES)

clean:
	rm -f $(MP4S)
	rm -rf stills
