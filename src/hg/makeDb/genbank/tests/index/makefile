kentSrc = ../../../../..
GBROOT = ../..
include ${GBROOT}/common.mk

O = gbIndexTest.o

PROGS =  ${TESTBINARCH}/gbIndexTest

all: ${PROGS}

clean:
	rm -f $O ${PROGS}

${TESTBINARCH}/gbIndexTest: gbIndexTest.o ${LIBGENBANK}
	@${MKDIR} ${TESTBINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} -o ${TESTBINARCH}/gbIndexTest gbIndexTest.o $(LIBS)

