include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgPepMrna.o

kgPepMrna: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/kgPepMrna $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/kgPepMrna${EXE}

clean:
	rm -f $O
