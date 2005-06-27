include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = motifConvert.o

motifConvert: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/motifConvert $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/motifConvert

clean:
	rm -f $O
