include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifLogo.o

motifLogo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/motifLogo $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/motifLogo${EXE}
