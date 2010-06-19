include ../../inc/common.mk
CVSTOOLS= cvsFiles cvsStatusFilter cvsup cvscheck

all: ${CVSTOOLS:%=${BINDIR}/%}

${BINDIR}/%: %
	@${MKDIR} ${BINDIR}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@

scripts: ${CVSTOOLS:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@${MKDIR} ${SCRIPTS}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@

clean::
	@echo nothing to clean in src/utils/cvsTools/
