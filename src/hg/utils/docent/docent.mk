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
#     make FAST=1 BP1 # figures only, no video -- roughly a third of the wall clock
#     make -j6        # scenarios in parallel (each run gets its own browser + cart)
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

# FAST=1 -> figures only: no dwells, no cursor animation, no screen recording, no mp4.
# Same stills, about a third of the wall clock. Use it while iterating on figure content;
# drop it for the final build that has to produce the videos.
FAST_ENV = $(if $(FAST),DOCENT_FAST=1 ,)

.PHONY: all list clean hires $(BASES)

all: $(MP4S)

$(FIGDIR)/%.mp4: %.docent.yaml $(DOCENT)
	$(FAST_ENV)$(PW_ENV) node $(DOCENT) $<

# hires: the same tours rendered for print -- SCALE times the pixels (a wider server image
# drawn with a bigger track font, the HTML zoomed to match), stills only, written to their
# own tree so the screen stills and the videos are left alone. Always a full rebuild: a
# print run is rare and cheap to ask for exactly when it is wanted. Its `session:` files go
# to their own tree too: a print run's cart carries pix=2550 and textSize=24, which is not
# the state anyone wants handed to them.
#
#   make hires                     # every scenario at 3x -> stills.hires/<base>/
#   make hires SCALE=2             # 2x
#   make hires BASES=BP1           # one scenario
#
SCALE ?= 3
HIRES ?= stills.hires
HIRESSESS ?= sessions.hires
hires:
	@for b in $(BASES); do \
	  echo "=== $$b at $(SCALE)x"; \
	  DOCENT_SCALE=$(SCALE) DOCENT_STILLS=$(HIRES) DOCENT_SESSIONS=$(HIRESSESS) DOCENT_FAST=1 \
	    $(PW_ENV) node $(DOCENT) $$b.docent.yaml || exit 1; \
	done

# Convenience: `make AP1` -> build ../AP1.mp4
$(BASES): %: $(FIGDIR)/%.mp4

list:
	@echo $(BASES)

clean:
	rm -f $(MP4S)
	rm -rf stills $(HIRES) sessions $(HIRESSESS)
