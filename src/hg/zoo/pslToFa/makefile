.c.o:
	gcc -ggdb -Wformat -O3 -Wimplicit -Wuninitialized -Wreturn-type -I../../../../../kent/src/inc -I../../../../../kent/src/hg/inc -c $*.c 

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../../../kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = pslToFa.o

pslToFa: $O $(MYLIBS)
	gcc -ggdb -o pslToFa $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/pslToFa
