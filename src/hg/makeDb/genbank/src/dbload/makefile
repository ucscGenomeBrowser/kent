GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbDbLoadStep $(BINARCH)/gbLoadRna

O = gbBuildState.o gbMetaData.o gbAlignData.o gbLoadRna.o 

all: ${PROGS}

$(BINARCH)/gbLoadRna: ${O}

clean:
	rm -f $(PROGS) ${O}
