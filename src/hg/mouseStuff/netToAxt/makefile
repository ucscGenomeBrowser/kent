include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = netToAxt.o

${HOME}/bin/${MACHTYPE}/netToAxt: $O ${MYLIBS}
	${CC} ${COPTS} -o ${HOME}/bin/${MACHTYPE}/netToAxt $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/netToAxt${EXE}

test:
	cd tests && ${MAKE}

clean:
	rm -f ${O}
