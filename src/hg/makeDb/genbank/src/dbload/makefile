GBROOT = ../..
include ${GBROOT}/common.mk

PROGS = ${BINDIR}/gbDbLoadStep $(BINARCH)/gbLoadRna ${BINDIR}/dbload-stop

O = gbIgnoreDelete.o gbBuildState.o gbMetaData.o gbAlignData.o dbLoadOptions.o gbLoadRna.o 

all: ${PROGS}

$(BINARCH)/gbLoadRna: ${O}

clean:
	rm -f $(PROGS) ${O}
