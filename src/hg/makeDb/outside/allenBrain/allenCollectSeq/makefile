include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = allenCollectSeq.o

allenCollectSeq: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/allenCollectSeq $O ${MYLIBS} $L
	strip ${BINDIR}/allenCollectSeq

clean:
	rm -f $O
