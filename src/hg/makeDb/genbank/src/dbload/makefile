GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbDbLoadStep \
	$(BINARCH)/gbLoadRna \
	${BINDIR}/dbload-stop \
	${BINDIR}/dumpTableStats \
	${BINDIR}/findUnusedReleases \
	${BINDIR}/dumpGrepIndex

O = gbIgnoreDelete.o gbBuildState.o gbMDParse.o gbMetaData.o gbAlignData.o dbLoadOptions.o \
	dbLoadPartitions.o gbLoadRna.o

all: ${PROGS}

$(BINARCH)/gbLoadRna: ${O} ${LIBGENBANK}

clean:
	rm -f $(PROGS) ${O}
