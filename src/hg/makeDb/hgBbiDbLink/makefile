include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgBbiDbLink.o

hgBbiDbLink: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgBbiDbLink $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgBbiDbLink${EXE}

clean:
	rm -f $O
