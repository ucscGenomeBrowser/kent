include ../../../inc/common.mk

L = -lm $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = clusterRna.o

clusterRna: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/clusterRna $O $(MYLIBS) $L
#${STRIP} $(HOME)/bin/$(MACHTYPE)/clusterRna${EXE}

tags:
	ctags *.c ../../lib/*.c ../../inc/*.h ../../../lib/*.c ../../../inc/*.h
