GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${GB_BINARCH}/ccdsImport \
	${GB_BINARCH}/ccdsMkTables \
	${GB_BINARCH}/mkCcdsGeneMap \
	${GB_BINDIR}/ccdsDownloadStep \
	${GB_BINDIR}/ccdsImportStep

O = createTables.o createKeys.o ccdsLocationsJoin.o ccdsCommon.o

all: ${PROGS}

# generate C code contain SQL for building tables
%.c: %.sql
	${STRINGIFY} -var=$*Sql $< >$@

clean:
	rm -f ${O} ${PROGS} ccdsImport.o ccdsMkTables.o mkCcdsGeneMap.o createKeys.c createTables.c

${GB_BINARCH}/ccdsImport:  ccdsImport.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/ccdsImport ccdsImport.o ${O} $(LIBS)

${GB_BINARCH}/ccdsMkTables:  ccdsMkTables.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/ccdsMkTables ccdsMkTables.o ${O} $(LIBS)

${GB_BINARCH}/mkCcdsGeneMap:  mkCcdsGeneMap.o ${O} ${LIBGENBANK} ${JKLIBS}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/mkCcdsGeneMap mkCcdsGeneMap.o ${O} $(LIBS)
