include ../../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatImageDownload.o ../lib/gs.o

gensatImageDownload: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/gensatImageDownload $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/gensatImageDownload${EXE}

clean:
	rm -f $O
