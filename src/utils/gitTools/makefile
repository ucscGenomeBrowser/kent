include ../../inc/common.mk
GITTOOLS= gitFiles gilt 

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

clean::
	@echo nothing to clean in src/utils/gitTools/
