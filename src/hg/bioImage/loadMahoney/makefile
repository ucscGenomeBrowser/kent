include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = loadMahoney.o mahoney.o

loadMahoney: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/loadMahoney $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/loadMahoney

clean:
	rm -f $O
