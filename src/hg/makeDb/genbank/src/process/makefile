GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbProcessStep ${BINARCH}/gbProcess

O = gbProcess.o pepFa.o gbParse.o

${BINARCH}/gbProcess: ${O}

all: ${PROGS}

clean:
	rm -f $(PROGS) ${O}
