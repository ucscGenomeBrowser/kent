include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = axtSomeRecords.o

all: discretize mapRecords axtSomeRecords mi

axtSomeRecords: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/axtSomeRecords $O ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/axtSomeRecords${EXE}

mi: mi.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mi mi.o ${MYLIBS} $L 
	${STRIP} ${HOME}/bin/${MACHTYPE}/mi${EXE}

discretize: discretize.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/discretize discretize.o ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/discretize${EXE}

mapRecords: mapRecords.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mapRecords mapRecords.o ${MYLIBS} $L
	${STRIP} ${HOME}/bin/${MACHTYPE}/mapRecords${EXE}

clean:
	rm -f $O
