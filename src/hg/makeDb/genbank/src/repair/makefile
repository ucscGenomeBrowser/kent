GBROOT = ../..
include ${GBROOT}/common.mk

O = repairExtFile.o refPepRepair.o

PROGS = $(GB_BINARCH)/repairExtFile

all: ${PROGS}

refPepRepair.o: refPepRepair.c refPepRepair.h
repairExtFile.o: repairExtFile.c refPepRepair.h
$(GB_BINARCH)/repairExtFile: ${O}

clean:
	rm -f $(PROGS) $O
