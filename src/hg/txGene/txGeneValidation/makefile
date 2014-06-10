include ../../../inc/common.mk

txGVScripts = checkRandomLinesExist.py \
	compareModifiedFileSizes.csh \
        compareTableSizesFromDbSnoop.py

all:	${txGVScripts}
	@for T in ${txGVScripts}; do \
	  ( rm -f ${SCRIPTS}/$$T ) ; \
	done
	${MKDIR} ${SCRIPTS}
	cp ${txGVScripts} ${SCRIPTS}/

clean:

