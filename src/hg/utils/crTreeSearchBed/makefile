include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkweb.a

O = crTreeSearchBed.o

crTreeSearchBed: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/crTreeSearchBed $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/crTreeSearchBed${EXE}

clean:
	rm -f $O
