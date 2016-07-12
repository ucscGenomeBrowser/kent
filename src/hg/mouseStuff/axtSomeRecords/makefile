kentSrc = ../../..
include ../../../inc/common.mk

L += -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = axtSomeRecords.o

all: discretize mapRecords axtSomeRecords mi

axtSomeRecords: $O ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/axtSomeRecords $O ${MYLIBS} $L
	${STRIP} ${BINDIR}/axtSomeRecords${EXE}

mi: mi.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mi mi.o ${MYLIBS} $L 
	${STRIP} ${BINDIR}/mi${EXE}

discretize: discretize.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/discretize discretize.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/discretize${EXE}

mapRecords: mapRecords.o ${MYLIBS}
	${CC} ${COPT} -o ${BINDIR}/mapRecords mapRecords.o ${MYLIBS} $L
	${STRIP} ${BINDIR}/mapRecords${EXE}

clean:
	rm -f $O
