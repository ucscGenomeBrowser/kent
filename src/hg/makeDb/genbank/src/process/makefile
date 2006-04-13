GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbProcessStep ${BINARCH}/gbProcess ${BINARCH}/gbProcessedCheck \
	${BINDIR}/gbMkOrganismList

O = gbProcess.o gbFilter.o gbParse.o

all: ${PROGS}

${BINARCH}/gbProcess: ${O}

${BINARCH}/gbProcessedCheck: gbProcessedCheck.c makefile ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} -o $@ gbProcessedCheck.c $(LIBS)

clean:
	rm -f $(PROGS) ${O}
