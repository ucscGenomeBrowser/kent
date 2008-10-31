include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = i16Make.o

i16Make: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/i16Make $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/i16Make${EXE}

clean:
	rm -f $O
