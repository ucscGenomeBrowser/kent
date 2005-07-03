include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = knownToBioImage.o

knownToBioImage: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/knownToBioImage $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/knownToBioImage${EXE}

clean:
	rm -f $O
