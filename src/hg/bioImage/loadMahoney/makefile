kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = loadMahoney.o mahoney.o

loadMahoney: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/loadMahoney $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/loadMahoney${EXE}

clean:
	rm -f $O
