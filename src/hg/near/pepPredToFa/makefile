include ../../../inc/common.mk

L = -lm $(MYSQLLIBS) 
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pepPredToFa.o

pepPredToFa: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/pepPredToFa $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/pepPredToFa
