# Build all directories in utils

# 	$Id: makefile,v 1.2 2006/06/13 20:21:04 kent Exp $

include ../../inc/common.mk

all:    
	@for D in $(USE_DIRS) x; do \
	  if test "$$D" != "x" ; then \
	    ( cd $$D && echo $$D && $(MAKE) 'HG_WARN=${HG_WARN_ERR}' ) ;\
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
	dumpDb.pl

# List of directories to build
USE_DIRS = $(DIRS)

DIRS = \
	chromGraphFromBin \
	chromGraphToBin \
	safePush \
	toDev64

BAD_DIRS= \
	ccCpLock \
	geneStarts \
	jimgrep \
	jkShell \
	dnsInfo \
	wigAsciiToBinary \
	cCp

