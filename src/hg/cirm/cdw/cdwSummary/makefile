kentSrc = ../../../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

all:
	chmod +x cdwSummary;
	rm -f ${SCRIPTS}/cdwSummary;
	@cp -p cdwSummary ${SCRIPTS};

clean::
	rm -f ${SCRIPTS}/cdwSummary;
