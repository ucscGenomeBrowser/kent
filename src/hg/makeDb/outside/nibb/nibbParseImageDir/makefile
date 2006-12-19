include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbParseImageDir.o

nibbParseImageDir: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/nibbParseImageDir $O ${MYLIBS} $L
	strip ${BINDIR}/nibbParseImageDir

clean:
	rm -f $O
