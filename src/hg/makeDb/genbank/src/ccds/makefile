GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINARCH}/ccdsImport \
	${BINARCH}/ccdsMkTables \
	${BINARCH}/mkCcdsGeneMap

O = createTables.o createKeys.o ccdsLocationsJoin.o ccdsCommon.o

all: ${PROGS}

# generate C code contain SQL for building tables
%.c: %.sql
	stringify -var=$*Sql $< >$@

clean:
	rm -f ${O} ${PROGS} ccdsImport.o ccdsMkTables.o mkCcdsGeneMap.o createKeys.c createTables.c

${BINARCH}/ccdsImport:  ccdsImport.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsImport ccdsImport.o ${O} $(LIBS)

${BINARCH}/ccdsMkTables:  ccdsMkTables.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/ccdsMkTables ccdsMkTables.o ${O} $(LIBS)

${BINARCH}/mkCcdsGeneMap:  mkCcdsGeneMap.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/mkCcdsGeneMap mkCcdsGeneMap.o ${O} $(LIBS)
