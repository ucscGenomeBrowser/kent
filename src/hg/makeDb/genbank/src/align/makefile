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
	${BINARCH}/gbAlignGet \
	${BINARCH}/gbAlignInstall

all: ${PROGS}

clean:
	rm -f ${O} ${PROGS}

${BINARCH}/gbAlignGet: gbAlignGet.o gbAlignCommon.o  ${LIBGENBANK}
	@mkdir -p ${BINARCH}
	gcc ${CFLAGS} ${LDFLAGS} -o ${BINARCH}/gbAlignGet gbAlignCommon.o gbAlignGet.o $(LIBS)
${BINARCH}/gbAlignInstall: $O gbAlignInstall.o gbAlignCommon.o ${MYLIBS}
	@mkdir -p ${BINARCH}
	gcc ${CFLAGS} -o ${BINARCH}/gbAlignInstall gbAlignCommon.o gbAlignInstall.o $(LIBS)


DEP =   ../inc/gbAligned.h ../inc/gbEntry.h ../inc/gbUpdate.h \
	../inc/gbFileOps.h ../inc/gbGenome.h ../inc/gbIndex.h \
	../inc/gbProcessed.h ../inc/gbRelease.h

gbAlignGet.o: ${DEP}
gbAlignInstall.o: ${DEP}

