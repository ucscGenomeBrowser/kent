kentSrc = ../../../..

include ${kentSrc}/inc/localEnvironment.mk
include ${kentSrc}/inc/common.mk

all:
	chmod +x cdwBackup;
	rm -f ${SCRIPTS}/cdwBackup;
	@cp -p cdwBackup ${SCRIPTS};

clean::
	rm -f ${SCRIPTS}/cdwBackup;
