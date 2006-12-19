include ../../../inc/common.mk
HG_WARN = ${HG_WARN_ERR}

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../../src/lib/$(MACHTYPE)
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = mafFetch.o

mafFetch: $O $(MYLIBS)
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/mafFetch $O $(MYLIBS) $L
	${STRIP} ${BINDIR}/mafFetch${EXE}
