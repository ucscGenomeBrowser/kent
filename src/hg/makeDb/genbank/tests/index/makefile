GBROOT = ../..
include ${GBROOT}/common.mk

O = gbIndexTest.o

PROGS =  ${TESTBINARCH}/gbIndexTest

all: ${PROGS}

clean:
	rm -f $O ${PROGS}

${TESTBINARCH}/gbIndexTest: gbIndexTest.o ${LIBGENBANK}
	@mkdir -p ${TESTBINARCH}
	gcc ${CFLAGS} -o ${TESTBINARCH}/gbIndexTest gbIndexTest.o $(LIBS)

