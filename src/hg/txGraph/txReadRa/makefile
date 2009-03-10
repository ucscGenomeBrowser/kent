include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = txReadRa.o

txReadRa: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/txReadRa $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/txReadRa${EXE}

clean:
	rm -f $O
