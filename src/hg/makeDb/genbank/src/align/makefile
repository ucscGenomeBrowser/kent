GBROOT = ../..
include ${GBROOT}/common.mk

O = gbAlignCommon.o gbAlignGet.o gbAlignInstall.o

PROGS = ${GB_BINDIR}/gbAlignSetup \
	${GB_BINDIR}/gbAlignRun \
	${GB_BINDIR}/gbAlignFinish \
	${GB_BINDIR}/gbAlignStep \
	${GB_BINDIR}/gbBlat \
	${GB_BINDIR}/alignStats \
	${GB_BINDIR}/gbPython \
	${GB_BINARCH}/gbAlignGet \
	${GB_BINARCH}/gbAlignInstall

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS}

${GB_BINARCH}/gbAlignGet: gbAlignGet.o gbAlignCommon.o  ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${GB_BINARCH}/gbAlignGet gbAlignCommon.o gbAlignGet.o $(LIBS)
${GB_BINARCH}/gbAlignInstall: $O gbAlignInstall.o gbAlignCommon.o ${LIBGENBANK}
	@${MKDIR} ${GB_BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} -o ${GB_BINARCH}/gbAlignInstall gbAlignCommon.o gbAlignInstall.o $(LIBS)


DEP =   ../inc/gbAligned.h ../inc/gbEntry.h ../inc/gbUpdate.h \
	../inc/gbFileOps.h ../inc/gbGenome.h ../inc/gbIndex.h \
	../inc/gbProcessed.h ../inc/gbRelease.h

gbAlignGet.o: ${DEP}
gbAlignInstall.o: ${DEP}

