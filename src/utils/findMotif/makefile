include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = findMotif.o

findMotif: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/findMotif $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/findMotif

clean:
	rm -f $O
