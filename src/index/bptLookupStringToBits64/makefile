include ../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = bptLookupStringToBits64.o

bptLookupStringToBits64: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/bptLookupStringToBits64 $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/bptLookupStringToBits64${EXE}

clean:
	rm -f $O
