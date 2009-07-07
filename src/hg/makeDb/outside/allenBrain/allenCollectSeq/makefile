include ../../../../../inc/common.mk

L += -lm ${SOCKETLIB}
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = allenCollectSeq.o

allenCollectSeq: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/allenCollectSeq $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/allenCollectSeq${EXE}

clean:
	rm -f $O
