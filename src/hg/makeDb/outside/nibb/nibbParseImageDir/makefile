include ../../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbParseImageDir.o

nibbParseImageDir: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/nibbParseImageDir $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/nibbParseImageDir${EXE}

clean:
	rm -f $O
