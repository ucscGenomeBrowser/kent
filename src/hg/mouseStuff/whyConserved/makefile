include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = whyConserved.o 

all: whyConserved trackOverlap

whyConserved: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/whyConserved $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/whyConserved

trackOverlap: trackOverlap.o
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/trackOverlap trackOverlap.o $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/trackOverlap
