include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  $(MYLIBDIR)/jkhgap.a $(MYLIBDIR)/jkweb.a

O = motifLogo.o

motifLogo: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/motifLogo $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/motifLogo${EXE}

clean::
	rm -f ${O}
