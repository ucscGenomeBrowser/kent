include ../../inc/common.mk


L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = getRnaPred.o

getRnaPred: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/getRnaPred $O $(MYLIBS) $L
	strip $(HOME)/bin/$(MACHTYPE)/getRnaPred
