include ../../../inc/common.mk


L = $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = ooCloneSpan.o

ooCloneSpan: $O 
	gcc -o $(HOME)/bin/$(MACHTYPE)/ooCloneSpan $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/ooCloneSpan${EXE}

