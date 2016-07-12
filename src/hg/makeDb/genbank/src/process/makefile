kentSrc = ../../../../..
GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${GB_BINDIR}/gbProcessStep ${GB_BINARCH}/gbProcess ${GB_BINARCH}/gbProcessedCheck \
	${GB_BINDIR}/gbMkOrganismList

O = gbProcess.o gbParse.o

all: ${PROGS}

${GB_BINARCH}/gbProcess: ${O} ${LIBGENBANK}

${GB_BINARCH}/gbProcessedCheck: gbProcessedCheck.c makefile ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} -o $@ gbProcessedCheck.c $(LIBS)

clean:
	rm -f $(PROGS) ${O}
