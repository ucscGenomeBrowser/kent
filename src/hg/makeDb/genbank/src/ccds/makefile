GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINARCH}/ccdsImport \
	${BINARCH}/ccdsMkTables

O = ccdsLocationsJoin.o

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS} ccdsImport.o

${BINARCH}/ccdsImport:  ccdsImport.o ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsImport ccdsImport.o $(LIBS)

${BINARCH}/ccdsMkTables:  ccdsMkTables.o ${O} ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsMkTables ccdsMkTables.o ${O} $(LIBS)
