include ../../inc/common.mk

L = -lm $(SOCKETLIB)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = gfPcr.o

gfPcr: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gfPcr $O ${MYLIBS} $L
#	${STRIP} ${HOME}/bin/${MACHTYPE}/gfPcr${EXE}

clean:
	rm -f $O
