include ../../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = gfPcr.o

gfPcr: $O ${MYLIBS}
	${CC} ${COPT} -o ${DESTDIR}${BINDIR}/gfPcr $O ${MYLIBS} $L
#	${STRIP} ${DESTDIR}${BINDIR}/gfPcr${EXE}

clean:
	rm -f $O
