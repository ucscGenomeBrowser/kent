include ../../../inc/common.mk


L = -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = checkGoldDupes.o

checkGoldDupes: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/checkGoldDupes $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/checkGoldDupes
