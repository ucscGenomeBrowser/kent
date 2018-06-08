kentSrc = ../../../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

A = cdwUpdateIx

all:
	chmod +x ${A}
	rm -f ${SCRIPTS}/${A}
	cp -p ${A} ${SCRIPTS}

clean::
	rm -f ${SCRIPTS}/${A}
