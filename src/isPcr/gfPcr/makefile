include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = gfPcr.o

gfPcr: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gfPcr $O ${MYLIBS} $L
#	strip ${HOME}/bin/${MACHTYPE}/gfPcr

clean:
	rm -f $O
