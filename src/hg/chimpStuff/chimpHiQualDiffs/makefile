include ../../../inc/common.mk

L = -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = chimpHiQualDiffs.o

chimpHiQualDiffs: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/chimpHiQualDiffs $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/chimpHiQualDiffs${EXE}
