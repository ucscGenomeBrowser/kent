include ../../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = nibbPrepImages.o

nibbPrepImages: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/nibbPrepImages $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/nibbPrepImages

clean:
	rm -f $O
