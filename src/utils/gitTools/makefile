include ../../inc/common.mk
GITTOOLS= gitFiles 

all: ${GITTOOLS:%=${BINDIR}/%}

${BINDIR}/%: %
	@${MKDIR} ${BINDIR}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@

scripts: ${GITTOOLS:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@${MKDIR} ${SCRIPTS}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@
