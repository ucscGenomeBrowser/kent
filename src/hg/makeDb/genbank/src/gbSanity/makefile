GBROOT = ../..
include ${GBROOT}/common.mk

O = metaData.o chkMetaDataTbls.o chkGbIndex.o chkAlignTbls.o \
	chkSeqTbl.o chkCommon.o \
	../dbload/dbLoadOptions.o gbSanity.o

PROGS = $(BINARCH)/gbSanity

all: ${PROGS}

${PROGS}: ${O}

clean:
	rm -f $(PROGS) $O
