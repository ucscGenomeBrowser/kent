include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bioImageLoad.o

bioImageLoad: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/bioImageLoad $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/bioImageLoad

clean:
	rm -f $O
