GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINARCH}/ccdsImport

all: ${PROGS}

test:
	(cd tests && ${MAKE} test)

clean:
	rm -f ${O} ${PROGS} ccdsImport.o

${BINARCH}/ccdsImport:  ccdsImport.o ${O} ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsImport ccdsImport.o ${O} $(LIBS)

savebak:
	savebak ccdsImport `cvsFiles`
