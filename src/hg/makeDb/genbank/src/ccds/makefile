GBROOT = ../..
include ${GBROOT}/common.mk

O = ccdsAccessions.o

PROGS = ${BINARCH}/ccdsImport

all: ${PROGS}

test:
	(cd tests && ${MAKE} test)

clean:
	rm -f ${O} ${PROGS} $O

${BINARCH}/ccdsImport:  ccdsImport.o ${O} ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsImport ccdsImport.o ${O} $(LIBS)
