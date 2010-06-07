include ../../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbPrepImages.o

nibbPrepImages: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/nibbPrepImages $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/nibbPrepImages${EXE}

clean:
	rm -f $O
