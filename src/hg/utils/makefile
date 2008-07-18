# Build all directories in utils
# test comment
# 	$Id: makefile,v 1.11 2008/07/18 04:37:03 markd Exp $

include ../../inc/common.mk

all:    
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done

scripts:
	@for F in ${SCRIPT_FILES}; do \
	    chmod +x $$F; \
	    rm -f ${SCRIPTS}/$${F}; \
	    echo cp -p $$F ${SCRIPTS}; \
	    cp -p $$F ${SCRIPTS}; \
	 done
	cd automation && ${MAKE} scripts

alpha: scripts all

test:
	@echo "nothing for tests yet"

clean:
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) clean ) ;\
	    x=$$? ; if [ $$x -ne 0 ]; then exit $$x ; fi \
	    fi ;\
	done
	touch non-empty-rm.o
	- find . -name \*.o -print | xargs rm

SCRIPT_FILES = \
	dumpDb.pl \
	mergeOverlapBed4.pl

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	bedWeedOverlapping \
	chromGraphFromBin \
	chromGraphToBin \
	gapToLift \
	oligoMatch \
	safePush \
	toDev64 \
	twoBitMask

BAD_DIRS= \
	ccCpLock \
	geneStarts \
	jimgrep \
	jkShell \
	dnsInfo \
	wigAsciiToBinary \
	positionalTblCheck \
	cCp

