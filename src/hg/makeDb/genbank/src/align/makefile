GBROOT = ../..
include ${GBROOT}/common.mk

O = gbAlignCommon.o gbAlignGet.o gbAlignInstall.o

PROGS = ${BINDIR}/gbAlignSetup \
	${BINDIR}/gbAlignRSync \
	${BINDIR}/gbAlignIServerCopy \
	${BINDIR}/gbAlignIServerClean \
	${BINDIR}/gbAlignRun \
	${BINDIR}/gbAlignFinish \
	${BINDIR}/gbAlignStep \
	${BINDIR}/gbBlat \
	${BINDIR}/alignStats \
	${BINDIR}/gbPython \
	${BINARCH}/gbAlignGet \
	${BINARCH}/gbAlignInstall

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS}

${BINARCH}/gbAlignGet: gbAlignGet.o gbAlignCommon.o  ${LIBGENBANK}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/gbAlignGet gbAlignCommon.o gbAlignGet.o $(LIBS)
${BINARCH}/gbAlignInstall: $O gbAlignInstall.o gbAlignCommon.o ${MYLIBS}
	@${MKDIR} ${BINARCH}
	${CC} ${COPT} ${CFLAGS} ${CFLAGS} -o ${BINARCH}/gbAlignInstall gbAlignCommon.o gbAlignInstall.o $(LIBS)


DEP =   ../inc/gbAligned.h ../inc/gbEntry.h ../inc/gbUpdate.h \
	../inc/gbFileOps.h ../inc/gbGenome.h ../inc/gbIndex.h \
	../inc/gbProcessed.h ../inc/gbRelease.h

gbAlignGet.o: ${DEP}
gbAlignInstall.o: ${DEP}

