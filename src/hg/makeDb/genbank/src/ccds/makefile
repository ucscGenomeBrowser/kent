GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINARCH}/ccdsImport \
	${BINARCH}/ccdsMkTables \
	${BINARCH}/mkCcdsKgMap

O = ccdsLocationsJoin.o ccdsCommon.o

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS} ccdsImport.o ccdsMkTables.o mkCcdsKgMap.o

${BINARCH}/ccdsImport:  ccdsImport.o ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsImport ccdsImport.o $(LIBS)

${BINARCH}/ccdsMkTables:  ccdsMkTables.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsMkTables ccdsMkTables.o ${O} $(LIBS)

${BINARCH}/mkCcdsKgMap:  mkCcdsKgMap.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/mkCcdsKgMap mkCcdsKgMap.o ${O} $(LIBS)
