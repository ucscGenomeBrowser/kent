include ../../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = allenCollectSeq.o

allenCollectSeq: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/allenCollectSeq $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/allenCollectSeq

clean:
	rm -f $O
