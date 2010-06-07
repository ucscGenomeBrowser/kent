include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = netToAxt.o

${BINDIR}/netToAxt: $O ${MYLIBS}
	${CC} ${COPTS} -o ${BINDIR}/netToAxt $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/netToAxt${EXE}

test:
	cd tests && ${MAKE}

clean:
	rm -f ${O}
