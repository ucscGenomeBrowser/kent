include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = weedLines.o

weedLines: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/weedLines $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/weedLines

clean:
	rm -f $O
