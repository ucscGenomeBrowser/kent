include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = cJoinX.o

cJoinX: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/cJoinX $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/cJoinX

clean:
	rm -f $O
