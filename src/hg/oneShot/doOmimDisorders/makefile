kentSrc = ../../..
include ../../../inc/common.mk

L = ${kentSrc}/htslib/libhts.a ${MYSQLLIBS}
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = doOmimDisorders.o

doOmimDisorders: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/doOmimDisorders $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/doOmimDisorders${EXE}

clean::
	rm -f ${O}
