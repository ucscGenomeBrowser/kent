GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${GB_BINDIR}/gbProcessStep ${GB_BINARCH}/gbProcess ${GB_BINARCH}/gbProcessedCheck \
	${GB_BINDIR}/gbMkOrganismList

O = gbProcess.o gbParse.o

all: ${PROGS}

${GB_BINARCH}/gbProcess: ${O} ${LIBGENBANK}

${GB_BINARCH}/gbProcessedCheck: ${O} ${LIBGENBANK}

clean:
	rm -f $(PROGS) ${O}
