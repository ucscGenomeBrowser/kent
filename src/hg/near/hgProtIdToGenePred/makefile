include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = $(HOME)/src/lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = hgProtIdToGenePred.o

hgProtIdToGenePred: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/hgProtIdToGenePred $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/hgProtIdToGenePred${EXE}
