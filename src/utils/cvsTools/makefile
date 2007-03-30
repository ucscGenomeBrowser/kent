include ../../inc/common.mk
CVSTOOLS=cvsFiles  cvsStatusFilter

all: ${CVSTOOLS:%=${BINDIR}/%}

${BINDIR}/%: %
	@${MKDIR} ${BINDIR}
	chmod a+rx $<
	cp -pf $< $@

scripts: ${CVSTOOLS:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@${MKDIR} ${SCRIPTS}
	chmod a+rx $<
	cp -pf $< $@
