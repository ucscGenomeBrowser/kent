include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterRna.o

clusterRna: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/clusterRna $O $(MYLIBS) $L
#strip $(HOME)/bin/$(MACHTYPE)/clusterRna

tags:
	ctags *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h
