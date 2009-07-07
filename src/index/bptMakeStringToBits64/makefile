include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bptMakeStringToBits64.o

bptMakeStringToBits64: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bptMakeStringToBits64 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bptMakeStringToBits64${EXE}

clean:
	rm -f $O
