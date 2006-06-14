include ../../../inc/common.mk

L = ${MYSQLLIBS} -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = kgPutBack.o

kgPutBack: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/kgPutBack $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/kgPutBack

clean:
	rm -f $O
