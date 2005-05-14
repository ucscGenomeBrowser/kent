GBROOT = ../..
include ${GBROOT}/common.mk

O = repairExtFile.o refPepRepair.o

PROGS = $(BINARCH)/repairExtFile

all: ${PROGS}

refPepRepair.o: refPepRepair.c refPepRepair.h
repairExtFile.o: repairExtFile.c refPepRepair.h
$(BINARCH)/repairExtFile: ${O}

clean:
	rm -f $(PROGS) $O
