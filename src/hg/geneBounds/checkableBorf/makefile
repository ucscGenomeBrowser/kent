include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkableBorf.o

checkableBorf: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/checkableBorf $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/checkableBorf${EXE}
