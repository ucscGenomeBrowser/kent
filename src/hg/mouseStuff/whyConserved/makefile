include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = whyConserved.o 

all: whyConserved trackOverlap

whyConserved: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/whyConserved $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/whyConserved${EXE}

trackOverlap: trackOverlap.o
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/trackOverlap trackOverlap.o $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/trackOverlap${EXE}
