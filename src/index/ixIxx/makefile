include ../../inc/common.mk

L = -lm
MYLIBDIR = ../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = ixIxx.o

ixIxx: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/ixIxx $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/ixIxx

clean:
	rm -f $O
