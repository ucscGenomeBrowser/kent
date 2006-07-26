include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbParseImageDir.o

nibbParseImageDir: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/nibbParseImageDir $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/nibbParseImageDir

clean:
	rm -f $O
