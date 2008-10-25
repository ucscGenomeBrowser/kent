include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = sufaMake.o

sufaMake: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/sufaMake $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/sufaMake${EXE}

clean:
	rm -f $O
