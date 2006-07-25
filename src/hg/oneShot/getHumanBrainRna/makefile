include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = getHumanBrainRna.o

getHumanBrainRna: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/getHumanBrainRna $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/getHumanBrainRna${EXE}

clean:
	rm -f $O
