GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbProcessStep ${BINARCH}/gbProcess

O = gbProcess.o gbFilter.o gbParse.o

all: ${PROGS}

${BINARCH}/gbProcess: ${O}


clean:
	rm -f $(PROGS) ${O}
