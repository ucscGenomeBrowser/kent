include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = crTreeIndexBed.o

crTreeIndexBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/crTreeIndexBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/crTreeIndexBed${EXE}

clean:
	rm -f $O
