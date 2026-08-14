kentSrc = ../..
include ../../inc/common.mk

SCRIPT_FILES = genark

all: ${SCRIPT_FILES:%=${SCRIPTS}/%}

${SCRIPTS}/%: %
	@${MKDIR} ${SCRIPTS}
	chmod a+rx $<
	rm -f $@
	cp -pf $< $@

test::
	@echo "# no tests in src/utils/genark"

clean::
	@echo nothing to clean in src/utils/genark
