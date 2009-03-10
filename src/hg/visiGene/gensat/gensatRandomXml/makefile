include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatRandomXml.o ../lib/gs.o

gensatRandomXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatRandomXml $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gensatRandomXml${EXE}

clean:
	rm -f $O
