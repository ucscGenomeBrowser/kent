include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = $(HOME)/kent/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a 
CFLAGS= -g

O = protDat.o

protDat: $O $(MYLIBS)
	gcc -ggdb -o $(HOME)/bin/$(MACHTYPE)/protDat $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/protDat${EXE}
