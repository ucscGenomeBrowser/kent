include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = faAlign.o

faAlign: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/faAlign $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/faAlign${EXE}

clean:
	rm -f $O
