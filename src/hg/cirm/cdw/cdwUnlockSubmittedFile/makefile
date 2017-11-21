kentSrc = ../../../..
include ${kentSrc}/inc/common.mk

A = cdwUnlockSubmittedFile.csh

all:
	@${MKDIR} ${SCRIPTS}
	chmod a+rx ${A}
	rm -f ${SCRIPTS}/${A}
	cp -pf ${A} ${SCRIPTS}/

clean::
	@echo nothing to clean 
