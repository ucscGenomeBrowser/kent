GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINARCH}/ccdsImport \
	${BINARCH}/ccdsMkTables \
	${BINARCH}/mkCcdsGeneMap

O = ccdsLocationsJoin.o ccdsCommon.o

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS} ccdsImport.o ccdsMkTables.o mkCcdsGeneMap.o

${BINARCH}/ccdsImport:  ccdsImport.o ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsImport ccdsImport.o $(LIBS)

${BINARCH}/ccdsMkTables:  ccdsMkTables.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsMkTables ccdsMkTables.o ${O} $(LIBS)

${BINARCH}/mkCcdsGeneMap:  mkCcdsGeneMap.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/mkCcdsGeneMap mkCcdsGeneMap.o ${O} $(LIBS)
