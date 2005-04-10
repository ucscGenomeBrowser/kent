include ../../../inc/common.mk

L = -lm
MYLIBDIR = ../../../lib/${MACHTYPE}
MYLIBS =  ${MYLIBDIR}/jkhgap.a ${MYLIBDIR}/jkweb.a

O = axtSomeRecords.o

all: discretize mapRecords axtSomeRecords mi

axtSomeRecords: $O ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/axtSomeRecords $O ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/axtSomeRecords

mi: mi.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mi mi.o ${MYLIBS} $L 
	strip ${HOME}/bin/${MACHTYPE}/mi

discretize: discretize.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/discretize discretize.o ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/discretize

mapRecords: mapRecords.o ${MYLIBS}
	${CC} ${COPT} -o ${HOME}/bin/${MACHTYPE}/mapRecords mapRecords.o ${MYLIBS} $L
	strip ${HOME}/bin/${MACHTYPE}/mapRecords

clean:
	rm -f $O
