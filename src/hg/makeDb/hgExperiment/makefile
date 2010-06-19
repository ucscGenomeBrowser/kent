include ../../../inc/common.mk

L += $(MYSQLLIBS) -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

PROG = hgExperiment
O = ${PROG}.o

${PROG}: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/${PROG} $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/${PROG}${EXE}

clean::
	rm -f ${O}
