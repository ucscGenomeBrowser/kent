include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = pslxToFa.o

pslxToFa: $O ${MYLIBS}
	${CC} ${COPT} ${CFLAGS} -o ${BINDIR}/pslxToFa $O ${MYLIBS} $L
#	${STRIP} ${BINDIR}/pslxToFa${EXE}

clean::
	rm -f ${O} pslxToFa
