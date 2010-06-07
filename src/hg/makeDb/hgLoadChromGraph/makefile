include ../../../inc/common.mk

L += -lm ${MYSQLLIBS}
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = hgLoadChromGraph.o

hgLoadChromGraph: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/hgLoadChromGraph $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/hgLoadChromGraph${EXE}

clean:
	rm -f $O
