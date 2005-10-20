include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafFetch.o

mafFetch: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o $(HOME)/bin/$(MACHTYPE)/mafFetch $O $(MYLIBS) $L
	${STRIP} $(HOME)/bin/$(MACHTYPE)/mafFetch${EXE}
