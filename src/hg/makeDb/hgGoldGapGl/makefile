include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

GOLDGAP_O = hgGoldGapGl.o ntContig.o
LOADGAP_O = hgLoadGap.o

all: hgGoldGapGl hgLoadGap

hgGoldGapGl: $(GOLDGAP_O) ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgGoldGapGl $(GOLDGAP_O) $(MYLIBS) $L

hgLoadGap: $(LOADGAP_O)  ../../../lib/$(MACHTYPE)/jkweb.a
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/hgLoadGap $(LOADGAP_O) $(MYLIBS) $L
lib:
	cd ../../../lib; make
