kentSrc = ../../../..
# Install the GenArk push scripts people run by hand in ${SCRIPTS}
# (/cluster/bin/scripts).  pushContrib.sh runs pushAccessions.sh from its
# own directory, and ottoBuildGenArkHub.py from /cluster/bin/scripts,
# which is installed by the makefile in ../userRequests/

include ../../../../inc/common.mk

SCRIPT_FILES = \
	pushAccessions.sh \
	pushContrib.sh

all: mkDir $(SCRIPT_FILES:%=%.all)

%.all:
	rm -f ${SCRIPTS}/$*
	echo cp -p $* ${SCRIPTS}
	cp -p $* ${SCRIPTS}
	chmod 775 ${SCRIPTS}/$*

mkDir:
	${MKDIR} ${SCRIPTS}

install: all

clean:
	@echo nothing to clean in src/hg/utils/otto/genArk/
