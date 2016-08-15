kentSrc = ../../../../..
GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${GB_BINDIR}/gbDbLoadStep \
	$(GB_BINARCH)/gbLoadRna \
	${GB_BINDIR}/dbload-stop \
	${GB_BINDIR}/dumpTableStats \
	${GB_BINDIR}/findUnusedReleases \
	${GB_BINDIR}/dumpGrepIndex

O = gbIgnoreDelete.o gbBuildState.o gbMDParse.o gbMetaData.o gbAlignData.o gbAlignTbl.o gbGeneTbl.o dbLoadOptions.o \
	dbLoadPartitions.o gbLoadRna.o

all: ${PROGS}

$(GB_BINARCH)/gbLoadRna: ${O} ${LIBGENBANK}

clean:
	rm -f $(PROGS) ${O}
