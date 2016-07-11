kentSrc = ../..
include ../../inc/common.mk

L += -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = dateXmlCgi.o

dateXmlCgi: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/dateXmlCgi $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/dateXmlCgi${EXE}

clean:
	rm -f $O
