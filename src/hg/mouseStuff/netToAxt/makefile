include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = netToAxt.o

${HOME}/bin/${MACHTYPE}/netToAxt: $O ${MYLIBS}
	${CC} ${COPTS} -o ${HOME}/bin/${MACHTYPE}/netToAxt $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/netToAxt

clean:
	rm -f ${O}
