include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatRandomXml.o ../lib/gs.o

gensatRandomXml: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gensatRandomXml $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gensatRandomXml

clean:
	rm -f $O
