include ../../../inc/common.mk

L = $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

PROG = hgExperiment
O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/${PROG} $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/${PROG}${EXE}
