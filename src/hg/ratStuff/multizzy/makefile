include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = multizzy.o

multizzy: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/multizzy $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/multizzy

clean:
	rm -f $O
