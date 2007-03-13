include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = orfEnum.o

orfEnum: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/orfEnum $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/orfEnum${EXE}

clean:
	rm -f $O
