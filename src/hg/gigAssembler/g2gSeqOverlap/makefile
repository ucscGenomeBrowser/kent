include ../../inc/makefile.h


L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a


O = g2gSeqOverlap.o 

g2gSeqOverlap: $O $(MYLIBS)
	gcc -o $(HOME)/bin/$(MACHTYPE)/g2gSeqOverlap $O $(MYLIBS) $L

lib:
	cd ../../lib; make
