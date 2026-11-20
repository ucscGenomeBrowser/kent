kentSrc = ../../../..
# Install the userRequests scripts that people run by hand, with the
# library and data file they need next to them, in ${SCRIPTS}
# (/cluster/bin/scripts).  The otto cron scripts here (ottoRequestWatch.sh,
# ottoRequestPush.py, ...) run from the source tree and are not installed.

# always /cluster/bin/scripts, pushContrib.sh runs ottoBuildGenArkHub.py
# from there; set before common.mk, which only defaults an empty SCRIPTS
SCRIPTS = /cluster/bin/scripts

include ../../../../inc/common.mk

# ottoBuildGenArkHub.py imports ottoLib.py from its own directory, and
# ottoLib.py reads dbDb.name.clade.tsv from its own directory
LIB_FILES = \
	ottoLib.py \
	dbDb.name.clade.tsv

SCRIPT_FILES = \
	ottoBuildGenArkHub.py

all: mkDir libFiles $(SCRIPT_FILES:%=%.all)

%.all:
	rm -f ${SCRIPTS}/$*
	echo cp -p $* ${SCRIPTS}
	cp -p $* ${SCRIPTS}
	chmod 775 ${SCRIPTS}/$*

mkDir:
	${MKDIR} ${SCRIPTS}

libFiles: mkDir $(LIB_FILES:%=%.libFile)

%.libFile:
	rm -f ${SCRIPTS}/$*
	echo cp -p $* ${SCRIPTS}
	cp -p $* ${SCRIPTS}
	chmod 664 ${SCRIPTS}/$*

install: all

clean:
	@echo nothing to clean in src/hg/utils/otto/userRequests/
