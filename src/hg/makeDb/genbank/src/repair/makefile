GBROOT = ../..
include ${GBROOT}/common.mk

O = repairExtFile.o refPepRepair.o

PROGS = $(GB_BINARCH)/repairExtFile \
	$(GB_BINDIR)/repairMgcOrfeome

all: ${PROGS}

refPepRepair.o: refPepRepair.c refPepRepair.h
repairExtFile.o: repairExtFile.c refPepRepair.h
$(GB_BINARCH)/repairExtFile: ${O}

$(GB_BINDIR)/repairMgcOrfeome: repairMgcOrfeome
	@mkdir -p $(dir $@)
	cp -f $< $@

clean:
	rm -f $(PROGS) $O
