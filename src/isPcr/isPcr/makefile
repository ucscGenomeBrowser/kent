include ../../inc/common.mk

L += -lm $(SOCKETLIB)
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkOwnLib.a ${MYLIBDIR}/jkweb.a

O = isPcr.o

isPcr: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/isPcr $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/isPcr${EXE}

clean:
	rm -f $O
