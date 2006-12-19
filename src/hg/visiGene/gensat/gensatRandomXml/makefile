include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatRandomXml.o ../lib/gs.o

gensatRandomXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatRandomXml $O ${MYLIBS} $L
	strip ${BINDIR}/gensatRandomXml

clean:
	rm -f $O
