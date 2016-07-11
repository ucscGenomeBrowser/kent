kentSrc = ../..
include ../../inc/common.mk
GITTOOLS= 
GITSCRIPTS= gitFiles gilt gitu

all: ${GITTOOLS:%=${BINDIR}/%} ${GITSCRIPTS:%=${SCRIPTS}/%}

${BINDIR}/%: %
	@${MKDIR} ${BINDIR}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@

scripts: ${GITSCRIPTS:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@${MKDIR} ${SCRIPTS}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@

clean::
	@echo nothing to clean in src/utils/gitTools/
