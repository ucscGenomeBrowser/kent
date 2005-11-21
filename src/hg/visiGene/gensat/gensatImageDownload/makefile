include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = gensatImageDownload.o ../lib/gs.o

gensatImageDownload: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/gensatImageDownload $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/gensatImageDownload

clean:
	rm -f $O
