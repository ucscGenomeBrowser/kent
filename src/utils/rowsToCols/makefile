include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = rowsToCols.o

rowsToCols: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/rowsToCols $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/rowsToCols

clean:
	rm -f $O
