GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbProcessStep ${BINARCH}/gbProcess ${BINARCH}/gbProcessedCheck

O = gbProcess.o gbFilter.o gbParse.o

all: ${PROGS}

${BINARCH}/gbProcess: ${O}

${BINARCH}/gbProcessedCheck: gbProcessedCheck.c makefile ${LIBGENBANK}
	@mkdir -p ${BINARCH}
	gcc ${CFLAGS} -o $@ gbProcessedCheck.c $(LIBS)

clean:
	rm -f $(PROGS) ${O}
