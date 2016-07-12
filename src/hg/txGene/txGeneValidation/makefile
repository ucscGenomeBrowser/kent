kentSrc = ../../..
include ../../../inc/common.mk

txGVScripts = checkRandomLinesExist.py \
	compareModifiedFileSizes.csh \
        compareTableSizesFromDbSnoop.py

all:	${txGVScripts}
	@for T in ${txGVScripts}; do \
	  ( rm -f ${DESTDIR}${SCRIPTS}/$$T ) ; \
	done
	${MKDIR} ${DESTDIR}${SCRIPTS}
	cp ${txGVScripts} ${DESTDIR}${SCRIPTS}/

clean:

