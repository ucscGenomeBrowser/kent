include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../../kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslToFa.o

pslToFa: $O $(MYLIBS)
	gcc -ggdb -o pslToFa $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/pslToFa
