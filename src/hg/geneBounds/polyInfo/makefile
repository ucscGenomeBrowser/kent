include ../../../inc/common.mk

L = -lm  $(MYSQLLIBS)
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = polyInfo.o

polyInfo: $O $(MYLIBS) makefile
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/polyInfo $O $(MYLIBS) $L
#	${STRIP} $(HOME)/bin/$(MACHTYPE)/polyInfo${EXE}
