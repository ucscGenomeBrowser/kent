include ../../inc/common.mk
BIN_DIR = $(HOME)/bin
CVSTOOLS=cvsFiles  cvsStatusFilter

all: ${CVSTOOLS:%=${BIN_DIR}/%}

${BIN_DIR}/%: %
	@${MKDIR} ${BIN_DIR}
	chmod a+rx $<
	cp -pf $< $@

scripts: ${CVSTOOLS:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@${MKDIR} ${SCRIPTS}
	chmod a+rx $<
	cp -pf $< $@
