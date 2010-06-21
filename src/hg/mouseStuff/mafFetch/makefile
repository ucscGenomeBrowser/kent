include ../../../inc/common.mk

L += ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafFetch.o

mafFetch: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafFetch $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mafFetch${EXE}

clean::
	rm -f ${O}
