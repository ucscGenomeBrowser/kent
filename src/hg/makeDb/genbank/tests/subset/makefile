GBROOT = ../..
include ${GBROOT}/common.mk

O = gbGetTestSubset.o

PROGS =  ${TESTBINARCH}/gbGetTestSubset

all: ${PROGS}

clean:
	rm -f $O ${PROGS}

${TESTBINARCH}/gbGetTestSubset: gbGetTestSubset.o ${LIBGENBANK}
	@mkdir -p ${TESTBINARCH}
	gcc ${CFLAGS} -o ${TESTBINARCH}/gbGetTestSubset gbGetTestSubset.o $(LIBS)

