include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = hgFakeAgp.o

hgFakeAgp: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgFakeAgp $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgFakeAgp${EXE}

clean:
	rm -f $O
